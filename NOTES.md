# kd is like ls same feel different name. and written in C

## check your locale first.

### and if you like this and are ready for converting add this to your .bashrc .zshrc etc.
alias ls="kd -l"
alias l="kd -laGW)

### fish uses something like
alias ls "kd -l"

# Flags

Short flags cluster (`-lha`). Supported: `-l` long, `-a`/`-A` hidden files,
`-h` no-op, `-B` raw bytes, `-D` directories only, `-g` show group, `-G` git
status and commit subject, `-s`/`-S` short long-format, `-m` pager, `-W` full path,
`-T` tree, `-1` one per line.

Long flags: `--tree`, `--git`, `--short`, `--bytes`, `--group`,
`--color[=auto|always|never]`, `--group-directories-first`, `--ls-colors`,
`--no-ls-colors`, `--ext-colors`, `--no-ext-colors`, `--header`,
`--no-header`, `--no-pager`, `--where`.

The group column is off by default because it almost always repeats the owner
name. `-g` turns it on. Same choice eza makes. `--icons`, `--no-icons` and
`--sort` are parsed and ignored — they exist only so pasted eza aliases don't
die. `--sort=name` is the default anyway.

## Color

Every color lives in `kf.h` as a `KF_*` macro. Each macro is the raw SGR
parameter between `ESC[` and `m` — `"2"` becomes `ESC[2m`. An empty string
means no sequence at all, i.e. the terminal's normal foreground. That is how
you switch off a single field without touching the code.

`paint(color, text)` is the only output path. It skips the sequence both when
color is globally off and when the macro is empty, so `""` costs nothing.

### Long-mode fields

Each column has its own macro so they can be told apart:

| Macro | Field | Default |
|---|---|---|
| `KF_NLINK` | link count | `32` green |
| `KF_USER` | owner | `35` purple |
| `KF_GROUP` | group, only with `-g` | `2` |
| `KF_SIZE` | size | empty |
| `KF_DAY` | day number | `34` blue |
| `KF_MONTH` | month | `94` light blue |
| `KF_TIME` | time of day | `37` white |
| `KF_AGE` | commit age, replaces the date trio under `-G` | `KF_TIME` |
| `KF_GIT_MSG` | commit subject, clean entry | `KF_SIZE`, i.e. the size column's ink |
| `KF_GIT_DIRTY` | commit subject, changed entry | `32` green |
| `KF_TREE` | tree branches | `2` |
| `KF_TOTAL` | summary row | empty |
| `KF_CLOCK` | clock in the summary row | `34` blue |

### The permission string

`putperms()` colors character by character rather than the field as a whole,
so `rwxr-xr-x` reads as a pattern instead of a blob.

| Macro | Character | Default |
|---|---|---|
| `KF_P_DASH` | `-` | `30` black |
| `KF_P_READ` | `r` | empty, normal foreground |
| `KF_P_WRITE` | `w` | `37` white |
| `KF_P_EXEC` | `x` | `94` light blue |
| `KF_P_OTHER` | anything else | `90` gray |

The field is nine characters, not ten: the leading type character that `ls`
prints is left out, so the first visible column of a listing lines up with the
header line above it. Directories and symlinks are already told apart by name
color, and a link also carries its `-> target`, so the `d` and `l` were paying
for a column they did not need.

Note that `30` (black) and `90` (gray) can sit close to the background in dark
themes. That is deliberate here — the dashes in `-rw-r--r--` are meant to
recede — but it is the same mechanism that made the metadata fields invisible
earlier.

### File names

| Macro | Applies to | Default |
|---|---|---|
| `KF_DIR` | directory | `01;34` |
| `KF_LINK` | symlink | `01;36` |
| `KF_DEV` | character and block devices | `33` yellow |
| `KF_ARROW` | the `->` arrow | `2` |
| `KF_TARGET` | link target that exists | `36` cyan |
| `KF_BROKEN` | link target that is missing | `31` red |
| `KF_EXEC` | executable | `01;32` |
| `KF_FILE` | everything else | empty |

### Why `2` and not `90`

SGR `2` is faint, an attribute that dims whatever foreground color the
terminal is already using. `90` selects palette color 8, which in dark themes
often sits so close to the background that the text disappears. Terminals
without faint support render normal foreground — visible either way, unlike
`90`.

### LS_COLORS

Off by default. `KF_USE_LS_COLORS` in `kf.h` sets the compile-time default;
`--ls-colors` and `--no-ls-colors` control it per run.

When on, `LS_COLORS` governs file names only, never the long-mode metadata
fields — ls does not color its own columns and therefore has no keys for them.
The `KF_*` fields above are always in effect.

`lookup()` is a straight linear scan through the `key=value:` string. No
pre-parsing into a hash table: the string is ~2 kB and is looked up once per
file, which does not show. The return value lives in a static buffer, so it
must be consumed before the next call.

The lookup order in `color()` follows GNU ls: symlink, directory, executable,
then extension (`*.gz` and friends), finally `fi`. Executable taking
precedence over extension matches `print_color_indicator()` in ls. If the key
is missing it falls back to the corresponding `KF_*` value, so neither mode
ever yields an empty result.

Extension matching only happens in `--ls-colors` mode; the built-in palette
has no extension table.

## Summary row

`footer()` prints one extra line at the end of a long listing: total size
under the size column, current time under the time column. The other fields
are left blank.

Alignment is achieved by printing the same field widths as `longline()` but
with empty strings — `%9s %3s %-8s` for permissions, link count and owner,
skipped entirely under `--short`, plus another `%-8s` when `-g` is on, and
`msgw + 1` in front of all of it when `-G` is on, or nothing at all when
`msgw` came out zero. Then `%8s` for the total, and for the date either
`%2s %3s` under the day and month or, when `-G` replaced those with the age
column, `KF_AGE_WIDTH - 5` spaces — either way the clock ends on the same edge
as the field above it.

If `longline()` ever changes a field width, `footer()` has to follow; they do
not share a format string.

The total is `st_size` summed over the listed entries — not disk allocation,
and not recursive into subdirectories. A directory entry contributes its own
inode size, exactly as shown on its own row. It is therefore the sum of the
column above it, not `du`.

The clock is the time the listing was printed, not any file's timestamp.

## Color detection

`o_color` is set to `isatty(1)` in `main()` before argument parsing, so piping
produces uncolored output without you having to flag for it. `--color=always`
overrides. `when()` maps auto/always/never.

## Layout

`columns()` reproduces the GNU ls layout rather than a single global width.

Column count is found by trying the widest layout first and stepping down:
for each candidate count, the entries are laid out **column-major** and each
column's width is taken as the widest entry in that column. The first
candidate whose total width plus two-space gutters fits `termcols` wins.
Layouts that would leave an entire trailing column empty are skipped.

This matters because a single global width pads every entry to the longest
name in the whole listing. With `NOTES.md` present, `kd` would get six
trailing spaces even in a column of two-character names.

Entries fill down columns, not across rows, which is why the reading order
matches `ls` once the listing wraps to several lines.

Not a tty, or `-1`, gives one per line.

Column width is measured with `strlen()`, i.e. bytes, not graphemes. File
names with non-ASCII therefore misalign. Doing it properly needs `wcwidth()`
over a decoded multibyte string — not done, since `-l` is unaffected and that
is the mode the aliases use.

## Full path

`-W` / `--where` prints the absolute path instead of just the name.

The path is constructed, not resolved. `realpath()` would have been tempting
but resolves symlinks, so `-W` on a link would have shown the target rather
than the link itself. `curcwd` is read once in `main()`, `curdir` is set per
directory listed, and `putname()` joins them.

Three cases are handled specially:

- The name is already absolute — printed unchanged. Without this, `kd -lW
  /etc/hostname` became `<cwd>//etc/hostname`.
- The directory is `/` — no extra slash is added.
- A leading `./` is stripped. In tree mode `curdir` is set to the recursive
  path, which starts with `.`, and without stripping the result was
  `/home/user/kd/./sub/inner.txt`.

`normpath()` normalizes lexically: `.` segments are dropped, `..` pops one
level via a stack of write positions, duplicate slashes collapse.
`/home/user/kd/../u-boot` becomes `/home/user/u-boot`.

The separator is written **before** each segment, not after. This is not a
style preference but a requirement: the function writes into the same buffer
it is reading from, and when nothing has been compacted the write pointer
coincides with the read pointer. With the slash written after the segment it
landed on the string's NUL terminator, and the loop ran on into the garbage
past the string — in practice the remains of the previous file name in the
same stack buffer. `kk` became `kk/h`, with the `h` borrowed from `kf.h` on
the line above. With the separator first, the write pointer always trails the
read pointer by at least one byte and the terminator is out of reach.

Normalization is purely lexical; it does not touch the filesystem. If `kd` is
a symlink, `kd/..` refers to the link's parent in the path string, not the
target's parent. `realpath()` would do the opposite — and resolve the link
itself, which is exactly what `-W` must not do.

## Sorting

`cmp()` sorts with `strcoll()`, which follows `LC_COLLATE`. `setlocale(LC_ALL,
"")` in `main()` is what activates it — without it å/ä/ö would sort after z.

`--group-directories-first` is applied as a prefix test in the same
comparator.

## Reading

`readents()` uses `lstat()`, not `stat()`, so symlinks are reported as links
rather than as their targets. `.` and `..` are always filtered out — that is
eza behavior, not ls behavior, and matches what the aliases expect.

The vector grows by doubling from 64 entries. Entries where `lstat()` fails
are skipped silently; in practice that only happens when racing a file being
removed during the listing.

## Tree

`tree()` recurses with a prefix string built up at each level — `"│   "` when
siblings remain, four spaces when the node is last. No depth limit and no loop
detection, so a directory symlink pointing upward would recurse forever.
Directories are only descended into when `S_ISDIR` holds on the `lstat()`
result, which means symlinks to directories are *not* followed — that is the
protective detail that keeps it from happening in practice.

## Size

Human-readable size is the default. `-B` / `--bytes` gives raw bytes. `-h` is
accepted and does nothing — it exists only so pasted ls aliases don't die.

`hsize()` divides by **1000**, not 1024. That is deliberate: 1.5 MB should
render as `1500K`, which it does not in base 1024 (there it becomes 1465).

The roll-up threshold is 1000 for every unit **except K, which holds until
10000**. That asymmetry is what keeps `1500K` in K while `1200M` rolls on to
`1.2G`.

A decimal is printed only when the mantissa is below 100. Hence `1.2G` but
`500M` and `1500K` without one.

Ladder from an actual run:

| bytes | shown |
|---|---|
| 500 | `500` |
| 1 500 | `1.5K` |
| 999 999 | `1000K` |
| 1 500 000 | `1500K` |
| 5 000 000 | `5000K` |
| 9 999 000 | `9999K` |
| 10 500 000 | `10.5M` |
| 500 000 000 | `500M` |
| 1 200 000 000 | `1.2G` |

`999999` rendering as `1000K` rather than `1.0M` is a direct consequence of
the K threshold, not a rounding bug.

## Pager

`-m` pauses per screenful. No external process is involved — neither `less`
nor `more` needs to exist on the system.

Output never goes through a pipe; it is written straight to the terminal as
usual. That is why color survives: `kd -l | more` turns color off because
`isatty(1)` becomes false, whereas `-m` leaves stdout untouched and merely
interrupts itself between lines.

`endline()` replaces every `putchar('\n')` in the four output paths —
`longline()`, `columns()`, `tree()` and the single-file branch in `main()`.
It counts lines and stops at `pgrows`. With the pager off the whole function
is a `putchar()` plus one comparison.

`pginit()` opens `/dev/tty` separately instead of reading stdin. That is what
makes `kd -lm` work even when stdin is redirected, and it keeps the keypress
distinct from any input data.

The terminal is put in cbreak: `ICANON` and `ECHO` off, `VMIN=1`, `VTIME=0`.
Without it you would have to press Enter after every space, and the key would
echo into the listing. `atexit(pgrestore)` restores termios, which also covers
`exit(0)` on `q` — otherwise the shell would be left with echo disabled.

Keys: `q` quits, Enter advances one line, anything else a screenful. Enter is
implemented by setting `pgcount = pgrows - 1` so the next line triggers the
prompt again.

The prompt shows how much of the listing has gone by: `-- 53% --`. The frame
is colored with `KF_PROMPT` (bold yellow) and the number with `KF_PERCENT`
(bold white), so it stands clearly apart from the surrounding rows. It is
erased with `\r\033[K` before the next line and leaves no trace in the
listing.

The percentage is `pgline` against `pgtotal`. `pgtotal` is set to `n + 1` —
the entries plus the summary row — right before the long-mode loop, and
`pgline` is reset at the same time, so multiple path arguments are counted
separately. `pgline` is incremented in `endline()` and is therefore a count of
printed lines, not entries; the two coincide in long mode but not in column
mode.

If `pgtotal` is zero there is nothing to measure against and the prompt falls
back to `-- more --`. That happens when `-m` is used in column or tree mode,
where the line count is not known in advance.

The pager only engages when stdout is a terminal. `kd -lm | head` therefore
behaves like `kd -l | head`.

### Automatic mode

`kd -l` turns the pager on by itself when the listing will not fit, i.e. when
`n + 1 >= termrows`. The count is known before the first line is printed
because `readents()` has already read the whole directory. `--no-pager`
disables the automatic behavior.

Only long mode is auto-paged. Column mode would require deriving the line
count from the column layout first, and that is not done.

### The pong

When the pager was started automatically, a game of pong plays beside the
prompt, so you can see that kd made the decision rather than you. With an
explicit `-m` nothing is drawn — telling the two apart is the entire point, and
`pgauto` is the flag that does it.

It is one line, `KF_PONG_GAP` columns right of the prompt and `KF_PONG_WIDTH`
cells wide, clamped to what is left of `termcols` and skipped below
`KF_PONG_MIN`. Only the column is addressed, never the row.

Time comes from `poll()`, not from paging. The prompt used to block in
`getc()`; it now redraws on a timeout and reads only once a key is really
waiting. The read is a raw `read()` on the fd, because stdio can buffer a byte
that `poll()` would then never report.

Ball and tail are one comet, `KF_PONG_AGES` long, indexed by age into
`KF_PONG_RING`, which rolls a step a frame in the direction of travel. Drawing
the ball as `⠆` instead reads as a colon, not a ball.

Color tracks progress, not time. `KF_PONG_RAMP` is five RGB anchors — grey,
green, purple, blue, white — and `pongtint()` interpolates between them on the
percentage, then scales by age for the comet's fade and quantizes into the
256-color cube. So the whole prompt drifts up the ramp as you page toward 100%,
a step per screenful, and the comet fades within whatever hue it is at.

The cube is the compromise here: interpolation is smooth but lands on 51-step
channels, so the ramp bands slightly and the oldest ages collapse to black.
Truecolor (`38;2;R;G;B`) would remove both, at the cost of terminals that do
not speak it.

A cell is two dots across, so the ball steps half a column and the tail cannot
break up. Speed is the frame time instead: `KF_PONG_MS_SLOW` at the ends down
to `KF_PONG_MS` in the middle, squared over `KF_PONG_EASE` half-cells, clamped
to a third of the court so a narrow one still has a fast middle.

The whole frame time is then scaled by progress, from full at 0% down to
`KF_PONG_RUSH` percent of it at 100%, so the game winds up as the listing runs
out. The dash divider is scaled by the same factor in the opposite direction —
otherwise the dashes ride the rising frame rate and speed up with it. Both
numbers want headroom above the 1 ms `poll()` floor: at `KF_PONG_MS 2` the
scaling truncated to 1 ms and the rush only reached the bounce ease.

The prompt's own `--` are redrawn from the same ring, mirrored around the
percentage, but on their own counter — a step every `KF_PONG_DASH` frames, one
way only. At the ball's own rate they strobe, and following its direction makes
them rock back and forth between bounces instead of rolling. The right pair is
`KF_PONG_DASHSKEW` further round, half the ring, so the two sides never show
the same glyphs at once — mirroring alone still reads as the same thing twice.

The court is drawn last of all, which leaves the cursor parked at the end of
the line rather than blinking in the middle of the prompt.

The walls are drawn only while lit — a bounce sets the glow, which walks down
`KF_PONG_HITFADE` and then stops being drawn at all. Blank cells are `U+2800`,
not spaces, so nothing shifts as it fades.

An empty cell is a blank braille cell, `U+2800`, not a space. The earlier
ASCII spinner avoided braille because a font that draws it wide would shift
the right edge; drawing the whole court out of one Unicode block instead means
every cell is whatever that font does, uniformly, and the court is a margin
short of the edge either way.

Row count is read from the same `TIOCGWINSZ` as the column width, once in
`main()`. Resizing the terminal mid-pagination is not noticed until the next
run.

## Git

`-G` / `--git` starts every long-mode line with one field: the subject of the
last commit that touched the entry, and the permissions, link count and owner
make room for it.

Whether that happens is decided per directory, not once at startup. `gitload()`
sets `gitok` from `--show-toplevel`; `main()` turns that into `gitcol` for the
listing it is about to print, and `shortfmt` is `o_short || gitcol`. Outside a
work tree `-G` therefore costs one failed `rev-parse` and changes nothing else
— no blank column, and the permissions it would have displaced stay. Listing a
repo and a plain directory in one command gives each the treatment it deserves.
An explicit `-S` sets `o_short` and survives regardless.

Status has no column of its own. `gitstat()` is still consulted, but only to
pick the ink: `KF_GIT_DIRTY` when the porcelain pair is anything other than
`--`, `KF_GIT_MSG` otherwise. A dirty tree is then a handful of green lines,
which is what the eye is looking for anyway.

What that gives up is the kind of change. `M`, `D`, `R` and `A` all come out
the same green, and an untracked file has no subject to paint, so it reads
like a file that was never committed — which, as far as `git log` is
concerned, it is. `git status` remains the place to go for detail.

`gitload()` runs git twice per directory: `rev-parse --show-prefix
--show-toplevel` to get the directory's position relative to the repo root
plus the repo's own name, and `status --porcelain -z` for the status itself.
The prefix is needed because `--porcelain` always reports paths from the root.
The toplevel rides along in the same invocation only to spare a third fork;
its basename is what `msgload()` strips off the front of a subject.

`-z` is mandatory, not an optimization. Without it git quotes names containing
non-ASCII using C syntax — `spårad.txt` becomes `"sp\303\245rad.txt"` — and
the string comparison against the `readdir()` name would fail. With `-z` raw
bytes are written with NUL as the separator.

Records are read as X (index) and Y (working tree); a space becomes `-`. If
the changed file sits below a subdirectory the status is attributed to the
first path component, i.e. the directory entry in the listing. First match
wins, so a directory with several changed files shows the first status rather
than any kind of merge. `R` and `C` consume an extra record because git sends
the origin path separately.

`msgload()` fetches the commit subjects with a third invocation: `log
--relative --format=%x01%ct%x02%s --name-only -- .`, one walk for the whole
directory. The `%ct` costs nothing over `%s` alone — it is the same walk, a few
more bytes down the pipe — and it is what the age column is built from. `\001` in the format marks the subject lines so they cannot be
confused with a path, and `-c core.quotePath=false` keeps non-ASCII names
unquoted the way `-z` does for the status. Every path is reduced to its first
component, and the first commit that mentions a component wins, so a
directory inherits the last commit touching anything beneath it.

A subject that opens with the repo's own name and a colon — `kd: color file
names by extension` — loses that prefix. In a repo whose commits all carry the
same component prefix it would otherwise eat a quarter of the column while
repeating what the header already says.

The walk stops as soon as every entry in the listing has a subject, which
makes an active directory nearly free. An entry nobody has committed — an
untracked or ignored file — never resolves, so that early exit never fires
and the walk runs to the root of history. On llama.cpp, 9550 commits and 76
entries, that is about half a second.

`msgfit()` decides how wide the column gets, as the smallest of three: the
`KF_MSG_WIDTH` ceiling, the longest subject actually present, and `termcols`
minus the fixed fields minus the longest name. The content cap is the one that
is easy to forget — without it a repo whose subjects run to 37 characters
still gets a 45-wide column, and the eight spaces of padding read as a gap
between the text and the size. Only the terminal cap is clamped to
`KF_MSG_MIN`; a short subject is allowed to give a short column. Subjects are
measured with `utf8w()`, names with `strlen()` via `dispname()`, which is also
what `-W` needs.

The overhead is spelled out as a sum in the function rather than as one
number, because it changes with `--short` and `-g` and the next person to add
a column will miss a bare 25. Piped output skips only the terminal cap.

A directory where nothing is committed yields `msgw == 0`, and both
`longline()` and `footer()` then omit the field and its separating space
rather than printing an empty column.

Names are measured with `dispname()`, not `strlen(e->name)`, so `-W` and its
absolute paths are accounted for.

`agestr()` turns a timestamp into `3 weeks ago`. The unit table is walked
until the age no longer reaches the next threshold; the divisor is the previous
row and the label the current one, which is why `second` sits in the table
without ever being printed. One day reads `yesterday`, under a minute `just
now`, and a timestamp in the future — clock skew, a checkout from a machine
running ahead — also lands on `just now` rather than a negative count.

Under `-G` this replaces the day/month/time trio, on the reasoning that in a
repo you want to know when the content last changed, not when the file was
written to disk. An entry with no commit falls back to its own mtime through
the same formatter, so an untracked file still says something. `KF_AGE_WIDTH`
is 14, the width of the longest phrasing (`59 minutes ago`), and `footer()`
right-aligns its clock to the same edge.

`fitmsg()` pads or cuts the subject to the width `msgfit()` settled on. It counts
characters rather than bytes, treating any byte that is not a UTF-8
continuation byte as one column, so a subject full of `åäö` is cut at
`KF_MSG_WIDTH` columns rather than that many bytes, and never splits a
character in half. Wide East Asian
characters still count as one, the same approximation `columns()` makes.

Reading `.git` directly would be the right approach if performance mattered,
but it needs zlib, index v2 parsing and tree diffing. Running git is a
fraction of the code, and git is installed anyway if the directory is a repo.

`gitopen()` does `pipe()` + `fork()` + `execvp()` directly, not `popen()`.
This is not a style question. `popen()` goes through `/bin/sh`, and the path
then has to be quoted into a command string. The first version used single
quotes, which gave **command injection**: a directory with an apostrophe in
its name broke out of the quoting and the rest of the name ran as shell.
Verified — a directory name alone was enough to create a file in `/tmp`.

With `execvp()` the path is passed as one element of the argument vector and
never reaches a shell parser. The entire problem class disappears instead of
being patched with escaping. `stderr` is silenced by the child dup'ing
`/dev/null` over fd 2 rather than `2>/dev/null` in a command string.

`gitclose()` does `fclose()` plus `waitpid()`. Without the wait the git
processes become zombies — one per directory.

## Symlinks

`putlink()` writes ` -> target` after the name, but **only in long mode**.

In column and tree mode the target is not shown at all. There the link is
marked with a trailing `@` instead, appended in `dispname()` so the column
width accounts for it. Color alone was not enough: it disappears when piping,
leaving no indication that the entry was a link. The target is
read with `readlink()`, not `realpath()` — what is shown is the link's own
text, not the resolved final destination. A chain `a -> b -> c` therefore
shows `b` on the row for `a`, exactly as ls does.

The path handed to `readlink()` is built from `curdir` plus the name, not from
`dispname()`. The difference matters: `dispname()` normalizes and may produce
an absolute path that does not exist relative to the process's cwd when `-W`
is on.

The target is colored by reachability. A failing `stat()` means a broken link
and yields `KF_BROKEN`, otherwise `KF_TARGET`. `stat()` follows the link
unlike `lstat()`, which is the whole point here.

## Devices

Character and block devices get `KF_DEV`. They are tested after directory but
before the executable test, since device nodes often have the x bit set and
would otherwise be counted as executables.

In `--ls-colors` mode `cd` and `bd` respectively are looked up first, the same
keys ls uses.

## configure

Takes no options; it finds what it needs and prints what it finds. Compiler:
`$CC`, then clang, then gcc, then cc. Which libc that compiler targets is asked
of the preprocessor, not guessed from its name — no `__GLIBC__` means musl, and
then `LDFLAGS = -static`. That is what Makefile.musl was for, so it is gone.
`CC="zig cc -target aarch64-linux-musl" ./configure` still cross-compiles.

The probe strips line markers and joins the output first: gcc breaks the line
before a macro that came from a system header, so `__GLIBC__` does not stay
next to the word it was written beside.

The generated file is `makefile`, small m, which GNU make and BSD make both
read before `Makefile`. `-include` and `ifeq` are GNU-only, so a config
fragment could never have worked on BSD or Minix. Where `makefile` and
`Makefile` are the same file, configure writes `GNUmakefile` — macOS wants that
anyway. The case test creates a file and looks for it under the other case
rather than asking `uname`.

awk generates it *from* `Makefile`, so the rules live in one place. If the
Makefile stops matching what awk substitutes into, configure removes the
half-made file instead of leaving one that ignores the configuration.
`install -Dm755` was GNU-only and is now `mkdir -p` plus `install -m 755`.

## Where kd becomes ls

Every `ls` on `$PATH` is classified as kd or not: same inode as a known kd, or
a symlink chain ending in a file named kd, or a binary containing
`--ls-colors`, which GNU ls has no reason to contain.

The first non-kd one becomes `LSPRE` in the makefile, and `LSPOST` is that plus
`.og`. `make install` moves the real ls aside and links kd in its place;
`make uninstall` moves it back. PATH order is then irrelevant — kd holds the
name itself.

configure never touches ls, only decides. The four cases in the recipe matter:
under `DESTDIR` nothing happens at all, so packaging cannot displace the build
host's ls. A symlink or a missing `LSPRE` is relinked without a move, since
there is nothing there worth keeping. An existing `LSPOST` means the move
already happened once, and the recipe stops rather than overwriting the backup
— that is the case after a coreutils update has restored a real `ls`, and
overwriting there would destroy the only original. Otherwise: move, then link.

`uninstall` restores only when `readlink LSPRE` is the kd it installed, so
someone else's ls is never moved on top of.

`gcc` warns about `-Wformat-truncation` in `dispname()` where three components
are written into an 8192 buffer. It is `snprintf`, i.e. truncation rather than
overflow, and clang does not warn. The behavior is safe but output can be cut
for extremely long paths.

Note on musl: `strcoll()` is effectively `strcmp()` because musl has no
locale-aware collation. Sorting of å/ä/ö therefore differs between a glibc
build and a musl build of the same source.

## Known limitations

- No column alignment for non-ASCII names (see Layout).
- Unknown uid/gid prints as `?` instead of the number, the way ls does.
  Shows up on the pmbootstrap chroots.
- No sorting by time or size; name only.
- Hardcoded limit of 256 path arguments.
- `-G` works only together with `-l`, and not in tree mode.
- `tree()` recurses with two 4 kB buffers per level, i.e. ~8 kB of stack per
  directory level. With an 8 MB stack that runs out around 1000 levels deep.
- `lookup()` truncates `LS_COLORS` values longer than 31 characters.
- Paths longer than 4 kB are truncated in `readents()`; the entry is then
  skipped silently because `lstat()` fails.
- `normpath()` stops popping `..` beyond 256 path segments.
