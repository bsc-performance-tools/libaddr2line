# AX_PROG_BINUTILS
# ----------------
AC_DEFUN([AX_PROG_BINUTILS],
[
  AC_ARG_WITH([binutils-addr2line],
    AS_HELP_STRING(
      [--with-binutils-addr2line=@<:@=FILE@:>@],
      [Specify where to find binutils' addr2line command]
    ),
    [binutils_cmd="${withval}"],
    [binutils_cmd="no"]
  )

  AC_MSG_CHECKING([for binutils' addr2line command])
  if test "${binutils_cmd}" != "no"; then
    # TODO: Currently, this only checks for the presence of the file provided via --with-binutils-addr2line
    #       It does not verify that the file is actually a valid 'addr2line' executable
    if test -f "${binutils_cmd}"; then
      AC_MSG_RESULT([${binutils_cmd}])
      AC_DEFINE([HAVE_BINUTILS], [1], [Define to 1 if binutils' addr2line command is available])
      AC_DEFINE_UNQUOTED([BINUTILS_ADDR2LINE], ["${binutils_cmd}"], [Path to binutils' addr2line command])
      have_binutils="yes"
    else
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([The value provided to --with-binutils-addr2line must point to the 'addr2line' executable from binutils.])
    fi
  else
    AC_MSG_RESULT([not available])
  fi
])
