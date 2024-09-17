# configure.ac

# Initialize autoconf and define the package name and version
AC_INIT([libaddr2line],[0.1],[tools@bsc.es])

# Create config.h header file with substituted values
AC_CONFIG_HEADERS([config.h])

# Directory that contains custom macros
AC_CONFIG_MACRO_DIRS([m4])

# Enable automake 
AM_INIT_AUTOMAKE([tar-ustar subdir-objects no-dependencies foreign -Wall -Werror])

# Check for the AR command (only if AM_PROG_AR exists)
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Enable libtool 
LT_INIT

# Check for programs
AC_PROG_CC           # Check for C compiler
AC_PROG_INSTALL      # Check for install program

# Check for binutils backend
AX_PROG_BINUTILS

# Check for elfutils backend
AX_PROG_ELFUTILS

# Optionally check for libraries or dependencies 
# AC_CHECK_LIB([pthread], [pthread_create])

# Optionally check for header files
AC_CHECK_HEADERS([ctype.h stdio.h stdlib.h string.h sys/types.h unistd.h])

# Define directories that contain Makefile.am
AC_CONFIG_FILES([Makefile])

# Complete configuration process
AC_OUTPUT
