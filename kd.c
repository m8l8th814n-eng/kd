/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
	   termrows = 24, o_nopager, o_abs,
	   o_extcolors = KF_USE_EXT_COLORS, o_header = KF_USE_HEADER, o_short,
	   o_crazy;

static off_t crazymax;

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
static char gitrepo[256];
static int gitok;	/* the listed directory is inside a work tree */
static int gitcol;	/* ...and so the subject column is being printed */
static int shortfmt;	/* -S, or -G having taken the space */

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

static const struct {
	const char *ext;
	const char *col;
	const char *ic;
	const char *ty;
} extcols[] = { KF_EXTENSIONS };

static const char *extcolor(const char *ext)
{
	size_t i;

	for (i = 0; i < sizeof extcols / sizeof *extcols; i++)
		if (!strcasecmp(ext, extcols[i].ext))
			return extcols[i].col;
	return NULL;
}

static size_t extfind(const char *ext)
{
	size_t i;

	for (i = 0; i < sizeof extcols / sizeof *extcols; i++)
		if (!strcasecmp(ext, extcols[i].ext))
			return i + 1;
	return 0;
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
	d = strrchr(e->name, '.');
	if (d && d != e->name && d[1]) {
		if (o_lscolors) {
			snprintf(key, sizeof key, "*%s", d);
			c = lookup(key);
			if (c)
				return c;
		}
		if (o_extcolors) {
			c = extcolor(d + 1);
			if (c)
				return c;
		}
	}
	return themed("fi", KF_FILE);
}

/* The lookup color() does, answering with the glyph or the family name
 * instead of the color. --crazy is the only caller.
 */
static const char *fam(const struct ent *e, int name)
{
	const char *d;
	size_t i;

	if (S_ISLNK(e->st.st_mode))
		return name ? "link" : KF_I_LINK;
	if (S_ISDIR(e->st.st_mode))
		return name ? "dir" : KF_I_DIR;
	if (S_ISCHR(e->st.st_mode) || S_ISBLK(e->st.st_mode))
		return name ? "dev" : KF_I_DEV;
	if (e->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return name ? "exec" : KF_I_EXEC;
	d = strrchr(e->name, '.');
	if (d && d != e->name && d[1]) {
		i = extfind(d + 1);
		if (i)
			return name ? extcols[i - 1].ty : extcols[i - 1].ic;
	}
	return name ? "file" : KF_I_FILE;
}

/* A foreground code turned into its background twin: 31 lights up as 41,
 * 95 as 105. Anything else falls back to a plain grey block.
 */
static const char *bgof(const char *fg)
{
	static char b[16];
	const char *p;
	int n;

	p = fg && *fg ? strrchr(fg, ';') : NULL;
	p = p ? p + 1 : (fg && *fg ? fg : "37");
	n = atoi(p);
	if ((n >= 30 && n <= 37) || (n >= 90 && n <= 97))
		n += 10;
	else
		n = 47;
	snprintf(b, sizeof b, "%d", n);
	return b;
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

	for (i = 0; i < 9; i++)
		o[i] = (m & (1 << (8 - i))) ? rwx[i] : '-';
	o[9] = 0;
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
	char *av1[] = { "git", "-C", NULL, "rev-parse", "--show-prefix",
			"--show-toplevel", NULL };
	char *av2[] = { "git", "-C", NULL, "status", "--porcelain", "-z", NULL };
	char pfx[4096], top[4096], xy[3], *buf, *p, *rel, *slash;
	FILE *f;
	size_t len = 0, cap = 1 << 16, plen, rl;
	int c;
	char x;

	gitfree();
	gitok = 0;
	av1[2] = (char *)path;
	av2[2] = (char *)path;
	pfx[0] = 0;
	top[0] = 0;
	gitrepo[0] = 0;
	f = gitopen(av1);
	if (!f)
		return;
	if (!fgets(pfx, sizeof pfx, f))
		pfx[0] = 0;
	if (!fgets(top, sizeof top, f))
		top[0] = 0;
	gitclose(f);
	pfx[strcspn(pfx, "\n")] = 0;
	plen = strlen(pfx);
	top[strcspn(top, "\n")] = 0;
	gitok = top[0] == '/';
	if (!gitok)
		return;		/* not a work tree; asking for status is pointless */
	slash = strrchr(top, '/');
	snprintf(gitrepo, sizeof gitrepo, "%s", slash ? slash + 1 : top);

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

struct msgent {
	const char *name;
	char *msg;
	time_t when;
};

static struct msgent *msgs;
static size_t nmsgs;

static void msgfree(void)
{
	size_t i;

	for (i = 0; i < nmsgs; i++)
		free(msgs[i].msg);
	free(msgs);
	msgs = NULL;
	nmsgs = 0;
}

/* Subject of the last commit that touched each entry, the way a github
 * directory listing shows it. One log walk for the whole directory, cut
 * short as soon as every entry has an answer.
 */
static void msgload(const char *path, struct ent *v, size_t n)
{
	char *av[] = { "git", "-C", NULL, "-c", "core.quotePath=false", "log",
		       "--relative", "--format=%x01%ct%x02%s", "--name-only",
		       "--", ".", NULL };
	char line[8192], subj[512], *slash, *s, *sep;
	size_t i, left = n, rl = strlen(gitrepo);
	time_t when = 0;
	FILE *f;

	msgfree();
	if (!n)
		return;
	msgs = calloc(n, sizeof *msgs);
	if (!msgs)
		die("out of memory");
	for (i = 0; i < n; i++)
		msgs[i].name = v[i].name;
	nmsgs = n;

	av[2] = (char *)path;
	f = gitopen(av);
	if (!f)
		return;
	subj[0] = 0;
	while (left && fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = 0;
		if (line[0] == '\1') {
			s = line + 1;
			sep = strchr(s, '\2');
			if (!sep)
				continue;
			*sep = 0;
			when = (time_t)strtoll(s, NULL, 10);
			s = sep + 1;
			/* "kd: color file names" — the repo name is already in
			 * the header, so it only costs width here.
			 */
			if (rl && !strncmp(s, gitrepo, rl) && s[rl] == ':') {
				s += rl + 1;
				while (*s == ' ')
					s++;
			}
			snprintf(subj, sizeof subj, "%s", s);
			continue;
		}
		if (!line[0])
			continue;
		slash = strchr(line, '/');
		if (slash)
			*slash = 0;
		for (i = 0; i < n; i++)
			if (!msgs[i].msg && !strcmp(msgs[i].name, line)) {
				msgs[i].msg = strdup(subj);
				msgs[i].when = when;
				left--;
				break;
			}
	}
	gitclose(f);
}

static const char *gitmsg(const char *name)
{
	size_t i;

	for (i = 0; i < nmsgs; i++)
		if (!strcmp(msgs[i].name, name))
			return msgs[i].msg ? msgs[i].msg : "";
	return "";
}

static time_t gitwhen(const char *name)
{
	size_t i;

	for (i = 0; i < nmsgs; i++)
		if (!strcmp(msgs[i].name, name))
			return msgs[i].when;
	return 0;
}

/* "3 days ago", the way a github listing dates a file. */
static void agestr(time_t t, char *o, size_t sz)
{
	static const struct {
		long secs;
		const char *unit;
	} step[] = {
		{ 60,		"second" },
		{ 3600,		"minute" },
		{ 86400,	"hour" },
		{ 7 * 86400,	"day" },
		{ 30 * 86400,	"week" },
		{ 365 * 86400,	"month" },
		{ 0,		"year" },
	};
	long d = (long)(time(NULL) - t), n;
	size_t i;

	if (d < 60) {
		snprintf(o, sz, "just now");
		return;
	}
	for (i = 1; step[i].secs && d >= step[i].secs; i++)
		;
	n = d / step[i - 1].secs;
	if (!strcmp(step[i].unit, "day") && n == 1) {
		snprintf(o, sz, "yesterday");
		return;
	}
	snprintf(o, sz, "%ld %s%s ago", n, step[i].unit, n == 1 ? "" : "s");
}

/* "43 minutes ago" right-aligned, the count in one color and the words in
 * another. Everything agestr() writes is ASCII, so bytes are columns here.
 */
static void putage(const char *age)
{
	size_t n = strspn(age, "0123456789");
	char num[16];
	int pad = KF_AGE_WIDTH - (int)strlen(age);

	if (pad > 0)
		printf("%*s", pad, "");
	if (n) {
		snprintf(num, sizeof num, "%.*s", (int)n, age);
		paint(KF_AGE_NUM, num);
	}
	paint(KF_AGE, age + n);
	putchar(' ');
}

/* Copy s into o padded to w columns, cut with an ellipsis if it does not
 * fit. Counts characters, not bytes, so UTF-8 subjects keep their width
 * and never break mid-character.
 */
/* Columns, not bytes: a UTF-8 continuation byte takes no width of its own.
 * Wide East Asian characters still count as one, as everywhere else here.
 */
static size_t utf8w(const char *s)
{
	const unsigned char *p = (const unsigned char *)s;
	size_t n = 0;

	for (; *p; p++)
		if ((*p & 0xC0) != 0x80)
			n++;
	return n;
}

/* Trim from the left, the way a prompt does: the tail of a path carries
 * more than its root, so /usr/share/fastfetch/presets/examples becomes
 * ..fetch/presets/examples. Cuts on columns, never mid-character.
 */
static void cutleft(char *s, size_t w)
{
	unsigned char *p = (unsigned char *)s;
	size_t skip;

	if (utf8w(s) <= w || w < 3)
		return;
	skip = utf8w(s) - (w - 2);
	while (skip && *p) {
		p++;
		while ((*p & 0xC0) == 0x80)
			p++;
		skip--;
	}
	memmove(s + 2, p, strlen((char *)p) + 1);
	s[0] = s[1] = '.';
}

static void fitmsg(const char *s, char *o, size_t sz, size_t w)
{
	size_t k = 0, used = 0, lim, cut = strlen(KF_MSG_CUT);
	const unsigned char *p;
	int trunc = 0;

	trunc = utf8w(s) > w;
	lim = trunc ? w - 1 : w;

	p = (const unsigned char *)s;
	used = 0;
	while (*p && used < lim && k + cut + 2 < sz) {
		o[k++] = (char)*p++;
		while ((*p & 0xC0) == 0x80 && k + cut + 2 < sz)
			o[k++] = (char)*p++;
		used++;
	}
	if (trunc) {
		memcpy(o + k, KF_MSG_CUT, cut);
		k += cut;
		used++;
	}
	while (used < w && k + 1 < sz) {
		o[k++] = ' ';
		used++;
	}
	o[k] = 0;
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

static void abspath(const char *p, char *out, size_t sz)
{
	if (p[0] == '/')
		snprintf(out, sz, "%s", p);
	else if (!strcmp(p, "."))
		snprintf(out, sz, "%s", curcwd);
	else
		snprintf(out, sz, "%s/%s", curcwd, p);
	normpath(out);
}

static int gitcmd(char *const av[], char *out, size_t sz)
{
	FILE *f;

	out[0] = 0;
	f = gitopen(av);
	if (!f)
		return 0;
	if (!fgets(out, (int)sz, f))
		out[0] = 0;
	gitclose(f);
	out[strcspn(out, "\n")] = 0;
	return out[0] != 0;
}

/* github.com/user/repo out of whatever form the remote is written in. */
static void shorturl(char *u)
{
	char *p, *slash;
	size_t l;

	p = strstr(u, "://");
	if (p)
		memmove(u, p + 3, strlen(p + 3) + 1);
	slash = strchr(u, '/');
	p = strchr(u, '@');
	if (p && (!slash || p < slash))
		memmove(u, p + 1, strlen(p + 1) + 1);
	p = strchr(u, ':');
	if (p && p[1] && (p[1] < '0' || p[1] > '9'))
		*p = '/';
	l = strlen(u);
	while (l && u[l - 1] == '/')
		u[--l] = 0;
	if (l > 4 && !strcmp(u + l - 4, ".git"))
		u[l - 4] = 0;
}

/* Where you are, with $HOME written as ~ the way a prompt would. */
static void curpath(const char *path, char *out, size_t sz)
{
	char abs[KDPATH], *disp = abs;
	const char *home;
	size_t hl;

	abspath(path, abs, sizeof abs);
	home = getenv("HOME");
	if (home && home[0] == '/' && home[1]) {
		hl = strlen(home);
		while (hl > 1 && home[hl - 1] == '/')
			hl--;
		if (!strncmp(abs, home, hl) && (!abs[hl] || abs[hl] == '/')) {
			abs[hl - 1] = '~';
			disp = abs + hl - 1;
		}
	}
	snprintf(out, sz, "%s", disp);
}

/* Which repo it is and which branch — one line, trimmed from the least
 * useful end when the terminal is too narrow to hold it. The path itself
 * sits in the footer, next to the clock.
 */
static void header(const char *path)
{
	char *avbr[] = { "git", "-C", NULL, "symbolic-ref", "--short", "HEAD", NULL };
	char *avsha[] = { "git", "-C", NULL, "rev-parse", "--short", "HEAD", NULL };
	char *avurl[] = { "git", "-C", NULL, "config", "--get", "remote.origin.url", NULL };
	char url[512], br[128], sha[64], *p;
	size_t need;

	avbr[2] = avsha[2] = avurl[2] = (char *)path;
	if (!gitcmd(avbr, br, sizeof br) && gitcmd(avsha, sha, sizeof sha))
		snprintf(br, sizeof br, "%s%s", KF_HEAD_DETACH, sha);
	if (br[0]) {
		gitcmd(avurl, url, sizeof url);
		shorturl(url);
	} else {
		url[0] = 0;
	}

	need = (url[0] ? strlen(url) : 0) + (br[0] ? strlen(br) + 2 : 0);
	if (need > (size_t)termcols && url[0]) {
		p = strchr(url, '/');
		if (p) {
			need -= (size_t)(p + 1 - url);
			memmove(url, p + 1, strlen(p + 1) + 1);
		}
	}
	if (need > (size_t)termcols && url[0]) {
		need -= strlen(url) + 1;
		url[0] = 0;
	}
	if (!url[0] && !br[0])
		return;
	if (url[0])
		paint(o_crazy ? KF_CRAZY_HEAD : KF_HEAD_REMOTE, url);
	if (br[0]) {
		if (url[0])
			putchar(' ');
		paint(o_crazy ? KF_CRAZY_HEAD : KF_HEAD_BRANCH, br);
	}
	endline();
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

/* What --crazy adds ahead of the name: the glyph, what kind of file it is,
 * what it actually costs on disk, and a bar of that against the greediest
 * entry in the listing. st_blocks is what the filesystem spent, which is
 * not the size a sparse or tiny file reports.
 */
static void putcrazy(const struct ent *e)
{
	char buf[128], du[32];
	off_t used = (off_t)e->st.st_blocks * 512;
	int n, i;

	paint(color(e), fam(e, 0));
	putchar(' ');
	snprintf(buf, sizeof buf, "%-7s ", fam(e, 1));
	paint(KF_CRAZY_TYPE, buf);
	hsize(used, du);
	snprintf(buf, sizeof buf, "%7s ", du);
	paint(KF_CRAZY_DISK, buf);

	n = crazymax > 0 ? (int)((used * KF_CRAZY_BARW + crazymax - 1) / crazymax) : 0;
	if (n > KF_CRAZY_BARW)
		n = KF_CRAZY_BARW;
	buf[0] = 0;
	for (i = 0; i < n; i++)
		strncat(buf, KF_CRAZY_BAR, sizeof buf - strlen(buf) - 1);
	paint(n * 3 <= KF_CRAZY_BARW ? KF_CRAZY_LOW :
	      n * 3 <= KF_CRAZY_BARW * 2 ? KF_CRAZY_MID : KF_CRAZY_HIGH, buf);
	printf("%*s ", KF_CRAZY_BARW - n, "");
}

static void crazyfit(const struct ent *v, size_t n)
{
	size_t i;

	crazymax = 0;
	for (i = 0; i < n; i++)
		if ((off_t)v[i].st.st_blocks * 512 > crazymax)
			crazymax = (off_t)v[i].st.st_blocks * 512;
}

static void putname(const struct ent *e)
{
	char full[KDPATH], c[64];

	dispname(e, full, sizeof full);
	if (o_crazy) {
		snprintf(c, sizeof c, "%s;%s", KF_CRAZY_TEXT, bgof(color(e)));
		putchar(' ');
		paint(c, full);
		putchar(' ');
		return;
	}
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

static size_t msgw = KF_MSG_WIDTH;

/* KF_MSG_WIDTH is a ceiling, not a promise. Everything in longline() except
 * the subject and the name is a known width, so the subject can give back
 * whatever the longest name needs rather than pushing the line off screen.
 */
static void msgfit(struct ent *v, size_t n)
{
	const size_t fixed = 1	/* the space after the subject */
			   + 9	/* size */
			   + KF_AGE_WIDTH + 1;	/* commit age */
	char b[KDPATH];
	size_t i, l, name = 0, subj = 0, over, avail;

	for (i = 0; i < n; i++) {
		dispname(&v[i], b, sizeof b);
		l = strlen(b);
		if (l > name)
			name = l;
		l = utf8w(gitmsg(v[i].name));
		if (l > subj)
			subj = l;
	}

	/* Never wider than the longest subject actually present: padding out
	 * to the ceiling would push the size column away from text that is
	 * not there.
	 */
	msgw = subj < KF_MSG_WIDTH ? subj : KF_MSG_WIDTH;
	if (!o_tty || !msgw)
		return;
	over = fixed + name + (shortfmt ? 0 : 23) + (o_showgrp ? 9 : 0);
	avail = over < (size_t)termcols ? (size_t)termcols - over : 0;
	if (avail < msgw)
		msgw = avail > KF_MSG_MIN ? avail : KF_MSG_MIN;
}

static void longline(const struct ent *e)
{
	char m[10], sz[32], day[8], mon[16], tim[16], buf[64];
	char msg[KF_MSG_WIDTH * 4 + 8], age[32];
	const char *g;
	time_t t;
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

	if (gitcol && msgw) {
		g = gitstat(e->name);
		fitmsg(gitmsg(e->name), msg, sizeof msg, msgw);
		paint(strcmp(g, "--") ? KF_GIT_DIRTY : KF_GIT_MSG, msg);
		putchar(' ');
	}
	if (!shortfmt) {
		putperms(m);
		snprintf(buf, sizeof buf, " %3lu ", (unsigned long)e->st.st_nlink);
		paint(KF_NLINK, buf);
		snprintf(buf, sizeof buf, "%-8s ", pw ? pw->pw_name : "?");
		paint(KF_USER, buf);
	}
	if (o_showgrp) {
		snprintf(buf, sizeof buf, "%-8s ", gr ? gr->gr_name : "?");
		paint(KF_GROUP, buf);
	}
	snprintf(buf, sizeof buf, "%8s ", sz);
	paint(KF_SIZE, buf);
	if (gitcol) {
		/* When git is telling the story, date the entry by its last
		 * commit rather than by its mtime, the way github does.
		 */
		t = gitwhen(e->name);
		agestr(t ? t : e->st.st_mtime, age, sizeof age);
		putage(age);
	} else {
		paint(KF_DAY, day);
		putchar(' ');
		paint(KF_MONTH, mon);
		putchar(' ');
		paint(KF_TIME, tim);
		putchar(' ');
	}
	if (o_crazy)
		putcrazy(e);
	putname(e);
	putlink(e);
	endline();
}

/* Total size and the time of day, opened by the path the listing is of:
 * the columns before the size are empty here, so the path lives there
 * rather than costing a line of its own.
 */
static void footer(off_t total, const char *path)
{
	char sz[32], clk[16], buf[64], cwd[KDPATH];
	int lead = 0;
	time_t now;
	struct tm *tm;

	hsize(total, sz);
	now = time(NULL);
	tm = localtime(&now);
	strftime(clk, sizeof clk, "%H:%M", tm);

	curpath(path, cwd, sizeof cwd);
	if (gitcol && msgw)
		lead += (int)msgw + 1;
	if (!shortfmt)
		lead += 23;
	if (o_showgrp)
		lead += 9;

	if (lead > 3) {
		cutleft(cwd, (size_t)lead - 1);
		paint(KF_HEAD_PATH, cwd);
		printf("%*s", lead - (int)utf8w(cwd), "");
	} else {
		printf("%*s", lead, "");
	}
	snprintf(buf, sizeof buf, "%8s ", sz);
	paint(KF_TOTAL, buf);
	if (gitcol)
		printf("%*s", KF_AGE_WIDTH - 5, "");
	else
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
	else if (kl == 10 && !strncmp(k, "ext-colors", 10))
		o_extcolors = 1;
	else if (kl == 13 && !strncmp(k, "no-ext-colors", 13))
		o_extcolors = 0;
	else if (kl == 5 && !strncmp(k, "short", 5))
		o_short = o_long = 1;
	else if (kl == 6 && !strncmp(k, "header", 6))
		o_header = 1;
	else if (kl == 9 && !strncmp(k, "no-header", 9))
		o_header = 0;
	else if (kl == 5 && !strncmp(k, "color", 5))
		o_color = when(v);
	else if (kl == 23 && !strncmp(k, "group-directories-first", 23))
		o_group = 1;
	else if (kl == 5 && !strncmp(k, "crazy", 5))
		o_crazy = o_long = 1;
	else if (kl == 9 && !strncmp(k, "crazy-git", 9))
		o_crazy = o_long = o_git = 1;
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
	int np = 0, i, hdr;
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
				case 's':
				case 'S': o_short = o_long = 1; break;
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
		if (np > 1) {
			if (i)
				putchar('\n');
			if (!(o_long && o_header && S_ISDIR(st.st_mode)))
				printf("%s:\n", paths[i]);
		}
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
		hdr = o_long && o_header;
		if (o_long && o_tty && !pgtty && !o_nopager &&
		    n + 1 + (size_t)hdr >= (size_t)termrows) {
			o_more = 1;
			pgauto = 1;
			pginit();
		}
		gitcol = 0;
		if (o_git && o_long) {
			gitload(paths[i]);
			gitcol = gitok;
			if (gitcol)
				msgload(paths[i], v, n);
		}
		/* Outside a work tree -G has nothing to show, so it does not
		 * get to spend the permissions column either.
		 */
		shortfmt = o_short || gitcol;
		if (o_long) {
			pgtotal = n + 1 + (size_t)hdr;
			pgline = 0;
			if (gitcol)
				msgfit(v, n);
			if (o_crazy)
				crazyfit(v, n);
			if (hdr)
				header(paths[i]);
			total = 0;
			for (j = 0; j < n; j++) {
				longline(&v[j]);
				total += v[j].st.st_size;
			}
			footer(total, paths[i]);
		} else
			columns(v, n);
		for (j = 0; j < n; j++)
			free(v[j].name);
		free(v);
	}
	fflush(stdout);
	return 0;
}
