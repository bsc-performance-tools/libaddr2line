# AX_PROG_BINUTILS
# ----------------
AC_DEFUN([AX_PROG_BINUTILS],
[
  AC_ARG_WITH([binutils-addr2line],
    AS_HELP_STRING(
      [--with-binutils-addr2line=@<:@=FILE@:>@],
      [specify where to find binutils' addr2line command]
    ),
    [binutils_cmd="${withval}"],
    [binutils_cmd=""]
  )

  AC_MSG_CHECKING([for binutils' addr2line command])
  if test -f "${binutils_cmd}"; then
    AC_MSG_RESULT([${binutils_cmd}])
    AC_DEFINE([HAVE_BINUTILS], [1], [Define to 1 if binutils' addr2line command is available])
    AC_DEFINE_UNQUOTED([BINUTILS_ADDR2LINE], ["${binutils_cmd}"], [Path to elfutil's addr2line command])
  else
    AC_MSG_RESULT([not available])
  fi
])