#ifndef KF_H
#define KF_H

#define KF_P_DASH	"30"
#define KF_P_DIR	"90"
#define KF_P_READ	""
#define KF_P_WRITE	"37"
#define KF_P_EXEC	"94"
#define KF_P_OTHER	"90"

#define KF_NLINK	"32"
#define KF_USER		"35"
#define KF_GROUP	"2"
#define KF_SIZE		""
#define KF_DAY		"34"
#define KF_MONTH	"94"
#define KF_TIME		"37"
#define KF_TREE		"2"
#define KF_TOTAL	""
#define KF_CLOCK	"34"
#define KF_PROMPT	"1;33"
#define KF_PERCENT	"1;97"

#define KF_SPIN_FRAMES	"|/-\\"
#define KF_SPIN_COLORS	{ "36", "32", "33", "35" }

#define KF_GIT_CLEAN	"2"
#define KF_GIT_DIRTY	""


#define KF_DIR		"01;34"
#define KF_LINK		"01;36"
#define KF_ARROW	"2"
#define KF_TARGET	"36"
#define KF_BROKEN	"31"
#define KF_EXEC		"01;32"
#define KF_DEV		"33"
#define KF_FILE		""

#define KF_USE_LS_COLORS 0
#define KF_USE_EXT_COLORS 1

/* Colors by file format. One macro per family, so retheming a whole
 * family is a single edit.
 */
#define KF_X_ARCHIVE	"31"
#define KF_X_IMAGE	"35"
#define KF_X_VIDEO	"95"
#define KF_X_AUDIO	"36"
#define KF_X_DOC	"33"
#define KF_X_CODE	"32"
#define KF_X_DATA	"93"
#define KF_X_MARKUP	"37"
#define KF_X_KEY	"91"
#define KF_X_JUNK	"90"

/* Extensions are matched without regard to case and without the dot.
 * Order does not matter; the first match wins.
 */
#define KF_EXTENSIONS \
	{ "7z",		KF_X_ARCHIVE }, \
	{ "apk",	KF_X_ARCHIVE }, \
	{ "ar",		KF_X_ARCHIVE }, \
	{ "arj",	KF_X_ARCHIVE }, \
	{ "br",		KF_X_ARCHIVE }, \
	{ "bz2",	KF_X_ARCHIVE }, \
	{ "cab",	KF_X_ARCHIVE }, \
	{ "cpio",	KF_X_ARCHIVE }, \
	{ "deb",	KF_X_ARCHIVE }, \
	{ "dmg",	KF_X_ARCHIVE }, \
	{ "ear",	KF_X_ARCHIVE }, \
	{ "gz",		KF_X_ARCHIVE }, \
	{ "img",	KF_X_ARCHIVE }, \
	{ "iso",	KF_X_ARCHIVE }, \
	{ "jar",	KF_X_ARCHIVE }, \
	{ "lha",	KF_X_ARCHIVE }, \
	{ "lz",		KF_X_ARCHIVE }, \
	{ "lz4",	KF_X_ARCHIVE }, \
	{ "lzh",	KF_X_ARCHIVE }, \
	{ "lzma",	KF_X_ARCHIVE }, \
	{ "pkg",	KF_X_ARCHIVE }, \
	{ "rar",	KF_X_ARCHIVE }, \
	{ "rpm",	KF_X_ARCHIVE }, \
	{ "squashfs",	KF_X_ARCHIVE }, \
	{ "tar",	KF_X_ARCHIVE }, \
	{ "taz",	KF_X_ARCHIVE }, \
	{ "tbz",	KF_X_ARCHIVE }, \
	{ "tbz2",	KF_X_ARCHIVE }, \
	{ "tgz",	KF_X_ARCHIVE }, \
	{ "txz",	KF_X_ARCHIVE }, \
	{ "tzst",	KF_X_ARCHIVE }, \
	{ "war",	KF_X_ARCHIVE }, \
	{ "xz",		KF_X_ARCHIVE }, \
	{ "z",		KF_X_ARCHIVE }, \
	{ "zip",	KF_X_ARCHIVE }, \
	{ "zst",	KF_X_ARCHIVE }, \
	\
	{ "ai",		KF_X_IMAGE }, \
	{ "arw",	KF_X_IMAGE }, \
	{ "avif",	KF_X_IMAGE }, \
	{ "bmp",	KF_X_IMAGE }, \
	{ "cr2",	KF_X_IMAGE }, \
	{ "dng",	KF_X_IMAGE }, \
	{ "eps",	KF_X_IMAGE }, \
	{ "gif",	KF_X_IMAGE }, \
	{ "heic",	KF_X_IMAGE }, \
	{ "heif",	KF_X_IMAGE }, \
	{ "ico",	KF_X_IMAGE }, \
	{ "jpeg",	KF_X_IMAGE }, \
	{ "jpg",	KF_X_IMAGE }, \
	{ "nef",	KF_X_IMAGE }, \
	{ "orf",	KF_X_IMAGE }, \
	{ "pbm",	KF_X_IMAGE }, \
	{ "pgm",	KF_X_IMAGE }, \
	{ "png",	KF_X_IMAGE }, \
	{ "pnm",	KF_X_IMAGE }, \
	{ "ppm",	KF_X_IMAGE }, \
	{ "psd",	KF_X_IMAGE }, \
	{ "raw",	KF_X_IMAGE }, \
	{ "svg",	KF_X_IMAGE }, \
	{ "svgz",	KF_X_IMAGE }, \
	{ "tga",	KF_X_IMAGE }, \
	{ "tif",	KF_X_IMAGE }, \
	{ "tiff",	KF_X_IMAGE }, \
	{ "webp",	KF_X_IMAGE }, \
	{ "xbm",	KF_X_IMAGE }, \
	{ "xcf",	KF_X_IMAGE }, \
	{ "xpm",	KF_X_IMAGE }, \
	\
	{ "3g2",	KF_X_VIDEO }, \
	{ "3gp",	KF_X_VIDEO }, \
	{ "asf",	KF_X_VIDEO }, \
	{ "avi",	KF_X_VIDEO }, \
	{ "divx",	KF_X_VIDEO }, \
	{ "flv",	KF_X_VIDEO }, \
	{ "m2ts",	KF_X_VIDEO }, \
	{ "m2v",	KF_X_VIDEO }, \
	{ "m4v",	KF_X_VIDEO }, \
	{ "mkv",	KF_X_VIDEO }, \
	{ "mov",	KF_X_VIDEO }, \
	{ "mp4",	KF_X_VIDEO }, \
	{ "mpeg",	KF_X_VIDEO }, \
	{ "mpg",	KF_X_VIDEO }, \
	{ "mts",	KF_X_VIDEO }, \
	{ "ogm",	KF_X_VIDEO }, \
	{ "ogv",	KF_X_VIDEO }, \
	{ "rmvb",	KF_X_VIDEO }, \
	{ "vob",	KF_X_VIDEO }, \
	{ "webm",	KF_X_VIDEO }, \
	{ "wmv",	KF_X_VIDEO }, \
	{ "xvid",	KF_X_VIDEO }, \
	\
	{ "aac",	KF_X_AUDIO }, \
	{ "aif",	KF_X_AUDIO }, \
	{ "aiff",	KF_X_AUDIO }, \
	{ "ape",	KF_X_AUDIO }, \
	{ "au",		KF_X_AUDIO }, \
	{ "flac",	KF_X_AUDIO }, \
	{ "it",		KF_X_AUDIO }, \
	{ "m4a",	KF_X_AUDIO }, \
	{ "mid",	KF_X_AUDIO }, \
	{ "midi",	KF_X_AUDIO }, \
	{ "mka",	KF_X_AUDIO }, \
	{ "mod",	KF_X_AUDIO }, \
	{ "mp3",	KF_X_AUDIO }, \
	{ "oga",	KF_X_AUDIO }, \
	{ "ogg",	KF_X_AUDIO }, \
	{ "opus",	KF_X_AUDIO }, \
	{ "ra",		KF_X_AUDIO }, \
	{ "s3m",	KF_X_AUDIO }, \
	{ "wav",	KF_X_AUDIO }, \
	{ "wma",	KF_X_AUDIO }, \
	{ "xm",		KF_X_AUDIO }, \
	\
	{ "azw",	KF_X_DOC }, \
	{ "azw3",	KF_X_DOC }, \
	{ "chm",	KF_X_DOC }, \
	{ "djvu",	KF_X_DOC }, \
	{ "doc",	KF_X_DOC }, \
	{ "docx",	KF_X_DOC }, \
	{ "epub",	KF_X_DOC }, \
	{ "fb2",	KF_X_DOC }, \
	{ "key",	KF_X_DOC }, \
	{ "mobi",	KF_X_DOC }, \
	{ "numbers",	KF_X_DOC }, \
	{ "odp",	KF_X_DOC }, \
	{ "ods",	KF_X_DOC }, \
	{ "odt",	KF_X_DOC }, \
	{ "pages",	KF_X_DOC }, \
	{ "pdf",	KF_X_DOC }, \
	{ "ppt",	KF_X_DOC }, \
	{ "pptx",	KF_X_DOC }, \
	{ "ps",		KF_X_DOC }, \
	{ "rtf",	KF_X_DOC }, \
	{ "tex",	KF_X_DOC }, \
	{ "xls",	KF_X_DOC }, \
	{ "xlsx",	KF_X_DOC }, \
	\
	{ "asm",	KF_X_CODE }, \
	{ "awk",	KF_X_CODE }, \
	{ "bash",	KF_X_CODE }, \
	{ "bat",	KF_X_CODE }, \
	{ "c",		KF_X_CODE }, \
	{ "cc",		KF_X_CODE }, \
	{ "clj",	KF_X_CODE }, \
	{ "cljs",	KF_X_CODE }, \
	{ "cmd",	KF_X_CODE }, \
	{ "cpp",	KF_X_CODE }, \
	{ "cr",		KF_X_CODE }, \
	{ "cs",		KF_X_CODE }, \
	{ "css",	KF_X_CODE }, \
	{ "cxx",	KF_X_CODE }, \
	{ "d",		KF_X_CODE }, \
	{ "dart",	KF_X_CODE }, \
	{ "el",		KF_X_CODE }, \
	{ "erl",	KF_X_CODE }, \
	{ "ex",		KF_X_CODE }, \
	{ "exs",	KF_X_CODE }, \
	{ "fish",	KF_X_CODE }, \
	{ "fs",		KF_X_CODE }, \
	{ "go",		KF_X_CODE }, \
	{ "h",		KF_X_CODE }, \
	{ "hh",		KF_X_CODE }, \
	{ "hpp",	KF_X_CODE }, \
	{ "hrl",	KF_X_CODE }, \
	{ "hs",		KF_X_CODE }, \
	{ "hxx",	KF_X_CODE }, \
	{ "java",	KF_X_CODE }, \
	{ "jl",		KF_X_CODE }, \
	{ "js",		KF_X_CODE }, \
	{ "jsx",	KF_X_CODE }, \
	{ "kt",		KF_X_CODE }, \
	{ "kts",	KF_X_CODE }, \
	{ "less",	KF_X_CODE }, \
	{ "lua",	KF_X_CODE }, \
	{ "m",		KF_X_CODE }, \
	{ "ml",		KF_X_CODE }, \
	{ "mli",	KF_X_CODE }, \
	{ "mm",		KF_X_CODE }, \
	{ "nim",	KF_X_CODE }, \
	{ "php",	KF_X_CODE }, \
	{ "pl",		KF_X_CODE }, \
	{ "pm",		KF_X_CODE }, \
	{ "ps1",	KF_X_CODE }, \
	{ "py",		KF_X_CODE }, \
	{ "r",		KF_X_CODE }, \
	{ "rb",		KF_X_CODE }, \
	{ "rs",		KF_X_CODE }, \
	{ "s",		KF_X_CODE }, \
	{ "sass",	KF_X_CODE }, \
	{ "scala",	KF_X_CODE }, \
	{ "scss",	KF_X_CODE }, \
	{ "sh",		KF_X_CODE }, \
	{ "sql",	KF_X_CODE }, \
	{ "svelte",	KF_X_CODE }, \
	{ "swift",	KF_X_CODE }, \
	{ "tcl",	KF_X_CODE }, \
	{ "ts",		KF_X_CODE }, \
	{ "tsx",	KF_X_CODE }, \
	{ "v",		KF_X_CODE }, \
	{ "vim",	KF_X_CODE }, \
	{ "vue",	KF_X_CODE }, \
	{ "zig",	KF_X_CODE }, \
	{ "zsh",	KF_X_CODE }, \
	\
	{ "cfg",	KF_X_DATA }, \
	{ "conf",	KF_X_DATA }, \
	{ "csv",	KF_X_DATA }, \
	{ "db",		KF_X_DATA }, \
	{ "env",	KF_X_DATA }, \
	{ "ini",	KF_X_DATA }, \
	{ "json",	KF_X_DATA }, \
	{ "ndjson",	KF_X_DATA }, \
	{ "parquet",	KF_X_DATA }, \
	{ "plist",	KF_X_DATA }, \
	{ "properties",	KF_X_DATA }, \
	{ "sqlite",	KF_X_DATA }, \
	{ "sqlite3",	KF_X_DATA }, \
	{ "toml",	KF_X_DATA }, \
	{ "tsv",	KF_X_DATA }, \
	{ "xml",	KF_X_DATA }, \
	{ "yaml",	KF_X_DATA }, \
	{ "yml",	KF_X_DATA }, \
	\
	{ "adoc",	KF_X_MARKUP }, \
	{ "htm",	KF_X_MARKUP }, \
	{ "html",	KF_X_MARKUP }, \
	{ "markdown",	KF_X_MARKUP }, \
	{ "md",		KF_X_MARKUP }, \
	{ "nfo",	KF_X_MARKUP }, \
	{ "org",	KF_X_MARKUP }, \
	{ "rst",	KF_X_MARKUP }, \
	{ "txt",	KF_X_MARKUP }, \
	{ "xhtml",	KF_X_MARKUP }, \
	\
	{ "asc",	KF_X_KEY }, \
	{ "cer",	KF_X_KEY }, \
	{ "crt",	KF_X_KEY }, \
	{ "der",	KF_X_KEY }, \
	{ "gpg",	KF_X_KEY }, \
	{ "kbx",	KF_X_KEY }, \
	{ "p12",	KF_X_KEY }, \
	{ "pem",	KF_X_KEY }, \
	{ "pfx",	KF_X_KEY }, \
	{ "pgp",	KF_X_KEY }, \
	{ "sig",	KF_X_KEY }, \
	\
	{ "a",		KF_X_JUNK }, \
	{ "bak",	KF_X_JUNK }, \
	{ "cache",	KF_X_JUNK }, \
	{ "class",	KF_X_JUNK }, \
	{ "crdownload",	KF_X_JUNK }, \
	{ "elc",	KF_X_JUNK }, \
	{ "ko",		KF_X_JUNK }, \
	{ "la",		KF_X_JUNK }, \
	{ "lo",		KF_X_JUNK }, \
	{ "lock",	KF_X_JUNK }, \
	{ "log",	KF_X_JUNK }, \
	{ "o",		KF_X_JUNK }, \
	{ "obj",	KF_X_JUNK }, \
	{ "old",	KF_X_JUNK }, \
	{ "orig",	KF_X_JUNK }, \
	{ "part",	KF_X_JUNK }, \
	{ "pid",	KF_X_JUNK }, \
	{ "pyc",	KF_X_JUNK }, \
	{ "pyo",	KF_X_JUNK }, \
	{ "rej",	KF_X_JUNK }, \
	{ "swo",	KF_X_JUNK }, \
	{ "swp",	KF_X_JUNK }, \
	{ "temp",	KF_X_JUNK }, \
	{ "tmp",	KF_X_JUNK }

#endif
