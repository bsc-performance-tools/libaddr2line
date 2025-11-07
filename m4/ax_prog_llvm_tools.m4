# AX_PROG_LLVM_TOOLS
# ----------------
AC_DEFUN([AX_PROG_LLVM_TOOLS],
[
  AC_ARG_WITH([llvm-tools-addr2line],
    AS_HELP_STRING(
      [--with-llvm-tools-addr2line=@<:@=FILE@:>@],
      [Specify where to find llvm-tools' addr2line command]
    ),
    [llvm_tools_cmd="${withval}"],
    [llvm_tools_cmd="no"]
  )

  AC_MSG_CHECKING([for llvm-tools' addr2line command])
  if test "${llvm_tools_cmd}" != "no"; then
    # TODO: Currently, this only checks for the presence of the file provided via --with-llvm-tools-addr2line
    #       It does not verify that the file is actually a valid 'addr2line' executable
    if test -f "${llvm_tools_cmd}"; then
      AC_MSG_RESULT([${llvm_tools_cmd}])
      AC_DEFINE([HAVE_LLVM_TOOLS], [1], [Define to 1 if llvm-tools' addr2line command is available])
      AC_DEFINE_UNQUOTED([LLVM_TOOLS_ADDR2LINE], ["${llvm_tools_cmd}"], [Path to llvm-tools' addr2line command])
      have_llvm_tools="yes"
    else
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([The value provided to --with-llvm-tools-addr2line must point to the 'llvm-addr2line' executable from llvm-tools.])
    fi
  else
    AC_MSG_RESULT([not available])
  fi
])
