# AX_PROG_ELFUTILS
# ----------------
AC_DEFUN([AX_PROG_ELFUTILS],
[
  AX_FLAGS_SAVE()
  AC_ARG_WITH([elfutils-addr2line],
    AS_HELP_STRING(
      [--with-elfutils-addr2line=@<:@=FILE@:>@],
      [Specify where to find elfutils' addr2line command]
    ),
    [elfutils_cmd="${withval}"],
    [elfutils_cmd="no"]
  )

  AC_MSG_CHECKING([for elfutils' addr2line command])
  if test "${elfutils_cmd}" != "no"; then
    # TODO: Currently, this only checks for the presence of the file provided via --with-elfutils-addr2line
    #       It does not verify that the file is actually a valid 'addr2line' executable
    if test -f "${elfutils_cmd}"; then
      AC_MSG_RESULT([${elfutils_cmd}])

      # Get the root path
      elfutils_home=`dirname $(dirname ${elfutils_cmd})`
    
      # Test for includes
      CFLAGS="-I${elfutils_home}/include"
      AC_CHECK_HEADERS([libelf.h], [], [AC_MSG_ERROR([Required header libelf.h not found in ${elfutils_home}/include. Ensure it's installed.])])
      AC_CHECK_HEADERS([gelf.h], [], [AC_MSG_ERROR([Required header gelf.h not found in ${elfutils_home}/include. Ensure it's installed.])])
      AC_SUBST(ELFUTILS_CFLAGS, ${CFLAGS})

      # Test for libraries
      LDFLAGS="-L${elfutils_home}/lib -lelf"
      AC_SEARCH_LIBS(elf_begin, elf, [], [AC_MSG_ERROR([Required library libelf not found in ${elfutils_home/lib}. Ensure it's installed.])])
      AC_SUBST(ELFUTILS_LDFLAGS, ${LDFLAGS})

      # Define config.h variables
      AC_DEFINE([HAVE_ELFUTILS], [1], [Define to 1 if elfutils' addr2line command is available])
      AC_DEFINE_UNQUOTED([ELFUTILS_ADDR2LINE], ["${elfutils_cmd}"], [Path to elfutil's addr2line command])
      have_elfutils="yes"
    else
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([The value provided to --with-elfutils-addr2line must point to the 'addr2line' executable from elfutils.])
    fi
  else
    AC_MSG_RESULT([not available])
  fi

  AX_FLAGS_RESTORE()
])
