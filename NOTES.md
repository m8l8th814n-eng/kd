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
status, `-m` pager, `-W` full path, `-T` tree, `-1` one per line.

Long flags: `--tree`, `--git`, `--bytes`, `--group`,
`--color[=auto|always|never]`, `--group-directories-first`, `--ls-colors`,
`--no-ls-colors`, `--no-pager`, `--where`.

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
| `KF_GIT_CLEAN` | git column when `--` | `2` |
| `KF_GIT_DIRTY` | git column otherwise | empty |
| `KF_TREE` | tree branches | `2` |
| `KF_TOTAL` | summary row | empty |
| `KF_CLOCK` | clock in the summary row | `34` blue |

### The permission string

`putperms()` colors character by character rather than the field as a whole,
so `rwxr-xr-x` reads as a pattern instead of a blob.

| Macro | Character | Default |
|---|---|---|
| `KF_P_DASH` | `-` | `30` black |
| `KF_P_DIR` | `d` | `90` gray |
| `KF_P_READ` | `r` | empty, normal foreground |
| `KF_P_WRITE` | `w` | `37` white |
| `KF_P_EXEC` | `x` | `94` light blue |
| `KF_P_OTHER` | `l p s c b` | `90` gray |

`KF_P_OTHER` covers the type characters for symlink, fifo, socket and devices,
which would otherwise fall through uncolored.

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
with empty strings — `%10s %3s %-8s` for permissions, link count and owner,
plus another `%-8s` when `-g` is on. Then `%8s` for the total and `%2s %3s`
for day and month, so the clock lands exactly under the time field. If
`longline()` ever changes a field width, `footer()` has to follow; they do not
share a format string.

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

### The spinner

When the pager was started automatically, a single character is drawn at the
right edge of the prompt line, so you can see that kd made the decision rather
than you. With an explicit `-m` it is not shown — telling the two apart is the
entire point, and `pgauto` is the flag that does it.

It is **one** character cell. `\033[<termcols>G` moves the cursor to the right
edge of the line already written, then one character is drawn. No absolute row
positioning is involved, so it scrolls along with everything else instead of
colliding with the listing.

Shape and color step per screenful, not per unit of time — it spins as you
page. Four shapes from `KF_SPIN_FRAMES` and four colors from
`KF_SPIN_COLORS`; since both have length four they stay in lockstep. Make the
lists different lengths and you get sixteen combinations instead.

The shapes are deliberately ASCII (`|/-\`). Braille and block spinners are
double-width or missing in some fonts, and a wrong character width would shift
the right edge.

Row count is read from the same `TIOCGWINSZ` as the column width, once in
`main()`. Resizing the terminal mid-pagination is not noticed until the next
run.

## Git

`-G` / `--git` adds a two-character column before the file name in long mode.

`gitload()` runs git twice per directory: `rev-parse --show-prefix` to get the
directory's position relative to the repo root, and `status --porcelain -z`
for the status itself. The prefix is needed because `--porcelain` always
reports paths from the root.

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

## Makefile.musl

A separate Makefile for musl builds. `ARCH` selects the target and defaults to
`uname -m`, so `make -f Makefile.musl` builds for the host and
`make -f Makefile.musl ARCH=aarch64` for arm64.

The compiler is picked automatically in descending order: `musl-gcc` when the
target is the host architecture, otherwise `<arch>-linux-musl-gcc`, otherwise
clang with a sysroot, otherwise `zig cc -target <arch>-linux-musl`. If none
exists the build stops with a readable error instead of silently falling back
to the system libc.

`zig` is in the chain because it ships musl sources for every target
architecture and can therefore cross-compile without an installed cross
toolchain. On this machine `musl-gcc` exists but `aarch64-linux-musl-gcc` does
not, so arm64 goes through zig.

`-static` is the default. For packaging against Alpine's system musl you
probably want to set `LDFLAGS=` empty instead.

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
