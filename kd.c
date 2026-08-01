/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <termios.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>
#include "kf.h"

#define KDPATH (PATH_MAX * 2 + NAME_MAX + 8)

static int o_long, o_all, o_bytes, o_dironly, o_tree, o_group, o_color, o_one, o_git,
	   o_showgrp, o_lscolors = KF_USE_LS_COLORS, o_more, o_tty, termcols = 80,
	   termrows = 24, o_nopager, o_abs;

static char curdir[4096], curcwd[4096];

static FILE *pgtty;
static int pgrows, pgcount, pgauto, spinframe;
static size_t pgtotal, pgline;
static struct termios pgsaved;

struct ent {
	char *name;
	struct stat st;
};

struct gitent {
	char *name;
	char xy[3];
};

static struct gitent *gits;
static size_t ngits;

static void die(const char *m)
{
	fprintf(stderr, "kd: %s\n", m);
	exit(1);
}

static int when(const char *v)
{
	if (!v || !strcmp(v, "auto") || !strcmp(v, "tty") || !strcmp(v, "if-tty"))
		return isatty(1);
	if (!strcmp(v, "always") || !strcmp(v, "yes") || !strcmp(v, "force"))
		return 1;
	return 0;
}

static char *lsc;

static const char *lookup(const char *key)
{
	static char val[32];
	size_t klen = strlen(key), n;
	char *p = lsc, *eq, *end;

	while (p && *p) {
		eq = strchr(p, '=');
		end = strchr(p, ':');
		if (!eq || (end && eq > end))
			break;
		if ((size_t)(eq - p) == klen && !strncmp(p, key, klen)) {
			n = end ? (size_t)(end - eq - 1) : strlen(eq + 1);
			if (n >= sizeof val)
				n = sizeof val - 1;
			memcpy(val, eq + 1, n);
			val[n] = 0;
			return n ? val : NULL;
		}
		if (!end)
			break;
		p = end + 1;
	}
	return NULL;
}

static const char *themed(const char *key, const char *def)
{
	const char *c;

	if (!o_lscolors)
		return def;
	c = lookup(key);
	return c ? c : def;
}

static const char *color(const struct ent *e)
{
	const char *c, *d;
	char key[64];

	if (!o_color)
		return NULL;
	if (S_ISLNK(e->st.st_mode))
		return themed("ln", KF_LINK);
	if (S_ISDIR(e->st.st_mode))
		return themed("di", KF_DIR);
	if (S_ISCHR(e->st.st_mode))
		return themed("cd", KF_DEV);
	if (S_ISBLK(e->st.st_mode))
		return themed("bd", KF_DEV);
	if (e->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return themed("ex", KF_EXEC);
	if (o_lscolors) {
		d = strrchr(e->name, '.');
		if (d && d != e->name) {
			snprintf(key, sizeof key, "*%s", d);
			c = lookup(key);
			if (c)
				return c;
		}
	}
	return themed("fi", KF_FILE);
}

static void pgrestore(void)
{
	if (pgtty)
		tcsetattr(fileno(pgtty), TCSANOW, &pgsaved);
}

static void pginit(void)
{
	struct termios t;

	if (!o_more || !o_tty)
		return;
	pgtty = fopen("/dev/tty", "r");
	if (!pgtty)
		return;
	if (tcgetattr(fileno(pgtty), &pgsaved)) {
		fclose(pgtty);
		pgtty = NULL;
		return;
	}
	t = pgsaved;
	t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(fileno(pgtty), TCSANOW, &t);
	atexit(pgrestore);
	pgrows = termrows > 1 ? termrows - 1 : 23;
}

static void paint(const char *c, const char *s)
{
	if (o_color && c && *c)
		printf("\033[%sm%s\033[0m", c, s);
	else
		fputs(s, stdout);
}

static void spin(void)
{
	static const char *cols[] = KF_SPIN_COLORS;
	const char *frames = KF_SPIN_FRAMES;
	char f[2];

	printf("\033[%dG", termcols);
	f[0] = frames[spinframe % (int)strlen(frames)];
	f[1] = 0;
	paint(cols[spinframe % (int)(sizeof cols / sizeof *cols)], f);
	spinframe++;
}

static void putperms(const char *m)
{
	char ch[2];
	int i;

	for (i = 0; m[i]; i++) {
		ch[0] = m[i];
		ch[1] = 0;
		switch (m[i]) {
		case '-': paint(KF_P_DASH, ch); break;
		case 'd': paint(KF_P_DIR, ch); break;
		case 'r': paint(KF_P_READ, ch); break;
		case 'w': paint(KF_P_WRITE, ch); break;
		case 'x': paint(KF_P_EXEC, ch); break;
		default:  paint(KF_P_OTHER, ch); break;
		}
	}
}

static void endline(void)
{
	char pb[16];
	int c, pct;

	putchar('\n');
	pgline++;
	if (!pgtty || ++pgcount < pgrows)
		return;
	pgcount = 0;
	if (pgtotal) {
		pct = (int)(pgline * 100 / pgtotal);
		if (pct > 100)
			pct = 100;
		snprintf(pb, sizeof pb, "%d%%", pct);
		paint(KF_PROMPT, "-- ");
		paint(KF_PERCENT, pb);
		paint(KF_PROMPT, " --");
	} else {
		paint(KF_PROMPT, "-- more --");
	}
	if (pgauto)
		spin();
	fflush(stdout);
	c = getc(pgtty);
	fputs("\r\033[K", stdout);
	if (c == 'q' || c == 'Q' || c == EOF)
		exit(0);
	if (c == '\n' || c == '\r')
		pgcount = pgrows - 1;
}

static void modestr(mode_t m, char *o)
{
	const char *rwx = "rwxrwxrwx";
	int i;

	o[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISFIFO(m) ? 'p' :
	       S_ISSOCK(m) ? 's' : S_ISCHR(m) ? 'c' : S_ISBLK(m) ? 'b' : '-';
	for (i = 0; i < 9; i++)
		o[i + 1] = (m & (1 << (8 - i))) ? rwx[i] : '-';
	o[10] = 0;
}

static void hsize(off_t b, char *o)
{
	const char *u = " KMGTP";
	double v = b, lim;
	int i = 0;

	if (o_bytes) {
		sprintf(o, "%lld", (long long)b);
		return;
	}
	for (;;) {
		lim = (i == 1) ? 10000.0 : 1000.0;
		if (v < lim || !u[i + 1])
			break;
		v /= 1000.0;
		i++;
	}
	if (!i)
		sprintf(o, "%lld", (long long)b);
	else if (v < 100)
		sprintf(o, "%.1f%c", v, u[i]);
	else
		sprintf(o, "%.0f%c", v, u[i]);
}

static int cmp(const void *a, const void *b)
{
	const struct ent *x = a, *y = b;
	int dx, dy;

	if (o_group) {
		dx = S_ISDIR(x->st.st_mode) != 0;
		dy = S_ISDIR(y->st.st_mode) != 0;
		if (dx != dy)
			return dy - dx;
	}
	return strcoll(x->name, y->name);
}

static struct ent *readents(const char *path, size_t *n)
{
	DIR *d;
	struct dirent *de;
	struct ent *v;
	size_t cap = 64, cnt = 0;
	char buf[4096];

	d = opendir(path);
	if (!d) {
		fprintf(stderr, "kd: %s: cannot open\n", path);
		return NULL;
	}
	v = malloc(cap * sizeof *v);
	if (!v)
		die("out of memory");
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
			continue;
		if (de->d_name[0] == '.' && !o_all)
			continue;
		if (cnt == cap) {
			cap *= 2;
			v = realloc(v, cap * sizeof *v);
			if (!v)
				die("out of memory");
		}
		snprintf(buf, sizeof buf, "%s/%s", path, de->d_name);
		if (lstat(buf, &v[cnt].st))
			continue;
		if (o_dironly && !S_ISDIR(v[cnt].st.st_mode))
			continue;
		v[cnt].name = strdup(de->d_name);
		cnt++;
	}
	closedir(d);
	qsort(v, cnt, sizeof *v, cmp);
	*n = cnt;
	return v;
}

static void gitfree(void)
{
	size_t i;

	for (i = 0; i < ngits; i++)
		free(gits[i].name);
	free(gits);
	gits = NULL;
	ngits = 0;
}

static void gitadd(const char *name, const char *xy)
{
	size_t i;

	for (i = 0; i < ngits; i++)
		if (!strcmp(gits[i].name, name))
			return;
	gits = realloc(gits, (ngits + 1) * sizeof *gits);
	if (!gits)
		die("out of memory");
	gits[ngits].name = strdup(name);
	memcpy(gits[ngits].xy, xy, 3);
	ngits++;
}

static pid_t gitpid;

static FILE *gitopen(char *const av[])
{
	int fd[2], null;

	if (pipe(fd))
		return NULL;
	gitpid = fork();
	if (gitpid < 0) {
		close(fd[0]);
		close(fd[1]);
		return NULL;
	}
	if (!gitpid) {
		dup2(fd[1], 1);
		null = open("/dev/null", O_WRONLY);
		if (null >= 0)
			dup2(null, 2);
		close(fd[0]);
		close(fd[1]);
		execvp(av[0], av);
		_exit(127);
	}
	close(fd[1]);
	return fdopen(fd[0], "r");
}

static void gitclose(FILE *f)
{
	fclose(f);
	waitpid(gitpid, NULL, 0);
}

static void gitload(const char *path)
{
	char *av1[] = { "git", "-C", NULL, "rev-parse", "--show-prefix", NULL };
	char *av2[] = { "git", "-C", NULL, "status", "--porcelain", "-z", NULL };
	char pfx[4096], xy[3], *buf, *p, *rel, *slash;
	FILE *f;
	size_t len = 0, cap = 1 << 16, plen, rl;
	int c;
	char x;

	gitfree();
	av1[2] = (char *)path;
	av2[2] = (char *)path;
	pfx[0] = 0;
	f = gitopen(av1);
	if (!f)
		return;
	if (!fgets(pfx, sizeof pfx, f))
		pfx[0] = 0;
	gitclose(f);
	pfx[strcspn(pfx, "\n")] = 0;
	plen = strlen(pfx);

	f = gitopen(av2);
	if (!f)
		return;
	buf = malloc(cap);
	if (!buf)
		die("out of memory");
	while ((c = fgetc(f)) != EOF) {
		if (len + 1 >= cap) {
			cap *= 2;
			buf = realloc(buf, cap);
			if (!buf)
				die("out of memory");
		}
		buf[len++] = (char)c;
	}
	gitclose(f);

	p = buf;
	while (p < buf + len) {
		rl = strnlen(p, (size_t)(buf + len - p));
		if (rl < 4)
			break;
		x = p[0];
		xy[0] = p[0] == ' ' ? '-' : p[0];
		xy[1] = p[1] == ' ' ? '-' : p[1];
		xy[2] = 0;
		rel = p + 3;
		if (!strncmp(rel, pfx, plen)) {
			rel += plen;
			slash = strchr(rel, '/');
			if (slash)
				*slash = 0;
			if (*rel)
				gitadd(rel, xy);
		}
		p += rl + 1;
		if (x == 'R' || x == 'C') {
			rl = strnlen(p, (size_t)(buf + len - p));
			p += rl + 1;
		}
	}
	free(buf);
}

static const char *gitstat(const char *name)
{
	size_t i;

	for (i = 0; i < ngits; i++)
		if (!strcmp(gits[i].name, name))
			return gits[i].xy;
	return "--";
}

static void normpath(char *p)
{
	char *r, *w, *seg, *stack[256];
	int top = 0;
	size_t len;

	if (*p != '/')
		return;
	r = p + 1;
	w = p;
	while (*r) {
		seg = r;
		while (*r && *r != '/')
			r++;
		len = (size_t)(r - seg);
		while (*r == '/')
			r++;
		if (!len || (len == 1 && seg[0] == '.'))
			continue;
		if (len == 2 && seg[0] == '.' && seg[1] == '.') {
			if (top > 0)
				w = stack[--top];
			continue;
		}
		if (top < 256)
			stack[top++] = w;
		*w++ = '/';
		memmove(w, seg, len);
		w += len;
	}
	if (w == p)
		*w++ = '/';
	*w = 0;
}

static void dispname(const struct ent *e, char *out, size_t sz)
{
	const char *d = curdir;
	size_t l;

	if (!o_abs || e->name[0] == '/') {
		snprintf(out, sz, "%s", e->name);
	} else {
		while (d[0] == '.' && d[1] == '/')
			d += 2;
		if (d[0] == '/')
			snprintf(out, sz, "%s%s%s", d,
				 strcmp(d, "/") ? "/" : "", e->name);
		else if (!*d || !strcmp(d, "."))
			snprintf(out, sz, "%s/%s", curcwd, e->name);
		else
			snprintf(out, sz, "%s/%s/%s", curcwd, d, e->name);
		normpath(out);
	}
	if (!o_long && S_ISLNK(e->st.st_mode)) {
		l = strlen(out);
		if (l + 2 <= sz) {
			out[l] = '@';
			out[l + 1] = 0;
		}
	}
}

static void putname(const struct ent *e)
{
	char full[KDPATH];

	dispname(e, full, sizeof full);
	paint(color(e), full);
}

static void putlink(const struct ent *e)
{
	char path[KDPATH], tgt[PATH_MAX];
	struct stat st;
	ssize_t k;

	if (!S_ISLNK(e->st.st_mode))
		return;
	if (e->name[0] == '/')
		snprintf(path, sizeof path, "%s", e->name);
	else
		snprintf(path, sizeof path, "%s/%s", curdir, e->name);
	k = readlink(path, tgt, sizeof tgt - 1);
	if (k < 0)
		return;
	tgt[k] = 0;
	paint(KF_ARROW, " -> ");
	paint(stat(path, &st) ? KF_BROKEN : KF_TARGET, tgt);
}

static void longline(const struct ent *e)
{
	char m[11], sz[32], day[8], mon[16], tim[16], buf[64];
	const char *g;
	struct passwd *pw;
	struct group *gr;
	struct tm *tm;

	modestr(e->st.st_mode, m);
	hsize(e->st.st_size, sz);
	pw = getpwuid(e->st.st_uid);
	gr = getgrgid(e->st.st_gid);
	tm = localtime(&e->st.st_mtime);
	strftime(day, sizeof day, "%e", tm);
	strftime(mon, sizeof mon, "%b", tm);
	strftime(tim, sizeof tim, "%H:%M", tm);

	putperms(m);
	snprintf(buf, sizeof buf, " %3lu ", (unsigned long)e->st.st_nlink);
	paint(KF_NLINK, buf);
	snprintf(buf, sizeof buf, "%-8s ", pw ? pw->pw_name : "?");
	paint(KF_USER, buf);
	if (o_showgrp) {
		snprintf(buf, sizeof buf, "%-8s ", gr ? gr->gr_name : "?");
		paint(KF_GROUP, buf);
	}
	snprintf(buf, sizeof buf, "%8s ", sz);
	paint(KF_SIZE, buf);
	paint(KF_DAY, day);
	putchar(' ');
	paint(KF_MONTH, mon);
	putchar(' ');
	paint(KF_TIME, tim);
	putchar(' ');
	if (o_git) {
		g = gitstat(e->name);
		paint(strcmp(g, "--") ? KF_GIT_DIRTY : KF_GIT_CLEAN, g);
		putchar(' ');
	}
	putname(e);
	putlink(e);
	endline();
}

static void footer(off_t total)
{
	char sz[32], clk[16], buf[64];
	time_t now;
	struct tm *tm;

	hsize(total, sz);
	now = time(NULL);
	tm = localtime(&now);
	strftime(clk, sizeof clk, "%H:%M", tm);

	printf("%10s %3s %-8s ", "", "", "");
	if (o_showgrp)
		printf("%-8s ", "");
	snprintf(buf, sizeof buf, "%8s ", sz);
	paint(KF_TOTAL, buf);
	printf("%2s %3s ", "", "");
	paint(KF_CLOCK, clk);
	endline();
}

static void columns(struct ent *v, size_t n)
{
	char b[KDPATH];
	size_t *len, *colw, i, p, r, c, rows, cols, try, tryrows, tot, mw, minw;

	if (!n)
		return;
	len = malloc(n * sizeof *len);
	if (!len)
		die("out of memory");
	minw = (size_t)-1;
	for (i = 0; i < n; i++) {
		dispname(&v[i], b, sizeof b);
		len[i] = strlen(b);
		if (len[i] < minw)
			minw = len[i];
	}

	cols = 1;
	if (!o_one && o_tty) {
		try = (size_t)termcols / (minw + 2) + 1;
		if (try > n)
			try = n;
		for (; try > 1; try--) {
			tryrows = (n + try - 1) / try;
			if ((try - 1) * tryrows >= n)
				continue;
			tot = 0;
			for (c = 0; c < try; c++) {
				mw = 0;
				for (i = c * tryrows;
				     i < (c + 1) * tryrows && i < n; i++)
					if (len[i] > mw)
						mw = len[i];
				tot += mw + (c + 1 < try ? 2 : 0);
			}
			if (tot <= (size_t)termcols)
				break;
		}
		cols = try;
	}
	rows = (n + cols - 1) / cols;

	colw = malloc(cols * sizeof *colw);
	if (!colw)
		die("out of memory");
	for (c = 0; c < cols; c++) {
		colw[c] = 0;
		for (i = c * rows; i < (c + 1) * rows && i < n; i++)
			if (len[i] > colw[c])
				colw[c] = len[i];
	}

	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++) {
			i = c * rows + r;
			if (i >= n)
				continue;
			putname(&v[i]);
			if (c + 1 < cols && i + rows < n)
				for (p = len[i]; p < colw[c] + 2; p++)
					putchar(' ');
		}
		endline();
	}
	free(len);
	free(colw);
}

static void tree(const char *path, const char *prefix)
{
	struct ent *v;
	size_t n, i;
	char sub[4096], pre[4096];
	int last;

	v = readents(path, &n);
	if (!v)
		return;
	for (i = 0; i < n; i++) {
		last = (i + 1 == n);
		snprintf(curdir, sizeof curdir, "%s", path);
		paint(KF_TREE, prefix);
		paint(KF_TREE, last ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ");
		putname(&v[i]);
		endline();
		if (S_ISDIR(v[i].st.st_mode)) {
			snprintf(sub, sizeof sub, "%s/%s", path, v[i].name);
			snprintf(pre, sizeof pre, "%s%s", prefix,
				 last ? "    " : "\u2502   ");
			tree(sub, pre);
		}
		free(v[i].name);
	}
	free(v);
}

static void longopt(char *a)
{
	char *v = strchr(a + 2, '=');
	size_t kl = v ? (size_t)(v - (a + 2)) : strlen(a + 2);
	char *k = a + 2;

	if (v)
		v++;
	if (kl == 4 && !strncmp(k, "tree", 4))
		o_tree = 1;
	else if (kl == 3 && !strncmp(k, "git", 3))
		o_git = 1;
	else if (kl == 5 && !strncmp(k, "bytes", 5))
		o_bytes = 1;
	else if (kl == 5 && !strncmp(k, "group", 5))
		o_showgrp = 1;
	else if (kl == 8 && !strncmp(k, "no-pager", 8))
		o_nopager = 1;
	else if (kl == 5 && !strncmp(k, "where", 5))
		o_abs = 1;
	else if (kl == 9 && !strncmp(k, "ls-colors", 9))
		o_lscolors = 1;
	else if (kl == 12 && !strncmp(k, "no-ls-colors", 12))
		o_lscolors = 0;
	else if (kl == 5 && !strncmp(k, "color", 5))
		o_color = when(v);
	else if (kl == 23 && !strncmp(k, "group-directories-first", 23))
		o_group = 1;
	else if (kl == 5 && !strncmp(k, "icons", 5))
		return;
	else if (kl == 8 && !strncmp(k, "no-icons", 8))
		return;
	else if (kl == 4 && !strncmp(k, "sort", 4))
		return;
	else
		die("unknown option");
}

int main(int argc, char **argv)
{
	char *paths[256];
	int np = 0, i;
	char *a, *p;
	struct winsize ws;
	struct stat st;
	struct ent e;
	struct ent *v;
	size_t n, j;
	off_t total;

	setlocale(LC_ALL, "");
	o_tty = isatty(1);
	o_color = o_tty;
	if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_col)
			termcols = ws.ws_col;
		if (ws.ws_row)
			termrows = ws.ws_row;
	}
	lsc = getenv("LS_COLORS");
	if (!getcwd(curcwd, sizeof curcwd))
		curcwd[0] = 0;

	for (i = 1; i < argc; i++) {
		a = argv[i];
		if (a[0] == '-' && a[1] == '-' && a[2]) {
			longopt(a);
		} else if (a[0] == '-' && a[1]) {
			for (p = a + 1; *p; p++)
				switch (*p) {
				case 'l': o_long = 1; break;
				case 'a':
				case 'A': o_all = 1; break;
				case 'h': break;
				case 'B': o_bytes = 1; break;
				case 'D': o_dironly = 1; break;
				case 'g': o_showgrp = 1; break;
				case 'm': o_more = 1; break;
				case 'W': o_abs = 1; break;
				case 'G': o_git = 1; break;
				case 'T': o_tree = 1; break;
				case '1': o_one = 1; break;
				default: die("unknown flag");
				}
		} else if (np < 256) {
			paths[np++] = a;
		}
	}
	if (!np)
		paths[np++] = ".";

	pginit();

	for (i = 0; i < np; i++) {
		if (lstat(paths[i], &st)) {
			fprintf(stderr, "kd: %s: not found\n", paths[i]);
			continue;
		}
		if (np > 1)
			printf("%s%s:\n", i ? "\n" : "", paths[i]);
		if (!S_ISDIR(st.st_mode)) {
			snprintf(curdir, sizeof curdir, ".");
			e.name = paths[i];
			e.st = st;
			if (o_long)
				longline(&e);
			else {
				putname(&e);
				endline();
			}
			continue;
		}
		if (o_tree) {
			puts(paths[i]);
			tree(paths[i], "");
			continue;
		}
		v = readents(paths[i], &n);
		if (!v)
			continue;
		snprintf(curdir, sizeof curdir, "%s", paths[i]);
		if (o_long && o_tty && !pgtty && !o_nopager &&
		    n + 1 >= (size_t)termrows) {
			o_more = 1;
			pgauto = 1;
			pginit();
		}
		if (o_git && o_long)
			gitload(paths[i]);
		if (o_long) {
			pgtotal = n + 1;
			pgline = 0;
			total = 0;
			for (j = 0; j < n; j++) {
				longline(&v[j]);
				total += v[j].st.st_size;
			}
			footer(total);
		} else
			columns(v, n);
		for (j = 0; j < n; j++)
			free(v[j].name);
		free(v);
	}
	fflush(stdout);
	return 0;
}
