/* Hand-written replacement for gnuplot's autoconf config.h: one header for
 * Linux, macOS and Windows (mingw) so the build needs no configure step.
 * Only dependency-free features are enabled: no X11, Qt, wx, Cairo, GD,
 * readline, Lua, libcerf or zlib. */
#ifndef LMPLOT_GNUPLOT_CONFIG_H
#define LMPLOT_GNUPLOT_CONFIG_H

/* Windows: gnuplot's native Windows port is a GUI (GDI+/Direct2D, console
 * window, help viewer) that lmplot does not use. Instead gnuplot is built with
 * its portable stdio/POSIX code paths: system headers are pulled in first (they
 * need _WIN32), then _WIN32 is hidden from gnuplot's own sources. */
#if defined(_WIN32)
# define LMPLOT_WINDOWS 1
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <strings.h>
# include <math.h>
# include <time.h>
# include <io.h>
# include <fcntl.h>
# include <direct.h>
# include <process.h>
# include <errno.h>
# include <limits.h>
# include <ctype.h>
# include <signal.h>
# include <setjmp.h>
# include <stdint.h>
# include <stdarg.h>
# include <wchar.h>
# include <locale.h>
# include <float.h>
# include <malloc.h>
# include <unistd.h>
# include <dirent.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/timeb.h>
# include <sys/time.h>
# undef _WIN32
# undef WIN32
# undef __WIN32__
# undef __MINGW32__
# undef __MINGW64__
#endif

#define PACKAGE "gnuplot"
#define PACKAGE_NAME "gnuplot"
#define PACKAGE_VERSION "6.1.0"
#define PACKAGE_STRING "gnuplot 6.1.0"
#define PACKAGE_TARNAME "gnuplot"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_URL ""
#define VERSION "6.1.0"
#define VERSION_MAJOR "6.1"
#define PATCHLEVEL "0"
#define DEVELOPMENT_VERSION 1
#define DIST_CONTACT "lmplot (https://github.com/lmctl-ai/lmplot), a modified gnuplot; report lmplot issues there, not to gnuplot"

#define STDC_HEADERS 1
#define HAVE__BOOL 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1
#define HAVE_FLOAT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_TIME_H 1
#define HAVE_TIME_T_IN_TIME_H 1
#define HAVE_WCHAR_H 1
#define HAVE_FENV_H 1
#define HAVE_COMPLEX_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TIMEB_H 1
#define HAVE_OFF_T 1
#define HAVE_LIBM 1

#define HAVE_ATEXIT 1
#define HAVE_FDOPEN 1
#define HAVE_FSEEKO 1
#define HAVE_GETCWD 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMSET 1
#define HAVE_POPEN 1
#define HAVE_PCLOSE 1
#define HAVE_SETVBUF 1
#define HAVE_STRCHR 1
#define HAVE_STRCSPN 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_VFPRINTF 1
#define HAVE_STRINGIZE 1
#define HAVE_ERF 1
#define HAVE_ERFC 1
#define HAVE_LGAMMA 1
#define HAVE_TGAMMA 1
#define HAVE_CBRT 1
#define HAVE_CABS 1
#define HAVE_CEXP 1
#define HAVE_CLOG 1
#define HAVE_CSQRT 1
#define HAVE_JN 1

#if defined(LMPLOT_WINDOWS)
/* mingw: no terminal/socket/process-control headers */
# define HAVE_DIRENT_H 1
# define HAVE_UNISTD_H 1
# define HAVE_SYS_PARAM_H 1
# define HAVE_POPEN 1
# define PIPES 1
# define HAVE_STRNCASECMP 1
# define HAVE_STRCASECMP 1
#else
# define HAVE_BCOPY 1
# define HAVE_BZERO 1
# define HAVE_INDEX 1
# define HAVE_RINDEX 1
# define HAVE_DIRENT_H 1
# define HAVE_DLFCN_H 1
# define HAVE_UNISTD_H 1
# define HAVE_STRINGS_H 1
# define HAVE_SYS_PARAM_H 1
# define HAVE_SYS_SELECT_H 1
# define HAVE_SYS_SOCKET_H 1
# define HAVE_SYS_IOCTL_H 1
# define HAVE_SYS_UTSNAME_H 1
# define HAVE_SYS_WAIT_H 1
# define HAVE_TERMIOS_H 1
# define HAVE_TCGETATTR 1
# define HAVE_POLL 1
# define HAVE_POLL_H 1
# define HAVE_SELECT 1
# define HAVE_SIGSETJMP 1
# define HAVE_SLEEP 1
# define HAVE_USLEEP 1
# define HAVE_STPCPY 1
# define HAVE_STRCASECMP 1
# define HAVE_STRNCASECMP 1
# define HAVE_STRNDUP 1
# define HAVE_STRNLEN 1
# define HAVE_UNAME 1
# define HAVE_LANGINFO_H 1
# define HAVE_WCWIDTH 1
# define HAVE_STRUCT_TM_TM_GMTOFF 1
# define SELECT_TYPE_ARG1 int
# define SELECT_TYPE_ARG234 (fd_set *)
# define SELECT_TYPE_ARG5 (struct timeval *)
# define PIPES 1
#endif

#if defined(__linux__)
# define HAVE_MALLOC_H 1
# define HAVE_VALUES_H 1
# define HAVE_SYSINFO 1
# define HAVE_ON_EXIT 1
# define HAVE_DECL_SIGNGAM 1
# define HAVE_GAMMA 1
#endif

#if !defined(LMPLOT_WINDOWS)
/* the built-in readline needs termios; the embedded host never reads a terminal on Windows */
# define GNUPLOT_HISTORY 1
# define READLINE 1
#endif
#define USE_POLAR_GRID 1
#define USE_STATS 1
#define USE_WATCHPOINTS 1
#define WITH_CHI_SHAPES 1
#define WITH_EXTRA_COORDINATE 1
#define WITH_TEKTRONIX 1
#define X_DISPLAY_MISSING 1

#endif
