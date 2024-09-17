# AX_PROG_ELFUTILS
# ----------------
AC_DEFUN([AX_PROG_ELFUTILS],
[
  AC_ARG_WITH([elfutils-addr2line],
    AS_HELP_STRING(
      [--with-elfutils-addr2line=@<:@=FILE@:>@],
      [specify where to find elfutils' addr2line command]
    ),
    [elfutils_cmd="${withval}"],
    [elfutils_cmd=""]
  )

  AC_MSG_CHECKING([for elfutils' addr2line command])
  if test -f "${elfutils_cmd}"; then
    AC_MSG_RESULT([${elfutils_cmd}])
    AC_DEFINE([HAVE_ELFUTILS], [1], [Define to 1 if elfutils' addr2line command is available])
    AC_DEFINE_UNQUOTED([ELFUTILS_ADDR2LINE], ["${elfutils_cmd}"], [Path to elfutil's addr2line command])
  else
    AC_MSG_RESULT([not available])
  fi
])