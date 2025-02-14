# AX_FLAGS_SAVE
# -------------
AC_DEFUN([AX_FLAGS_SAVE],
[
   saved_LIBS="${LIBS}"
   saved_CC="${CC}"
   saved_CFLAGS="${CFLAGS}"
   saved_CXXFLAGS="${CXXFLAGS}"
   saved_CPPFLAGS="${CPPFLAGS}"
   saved_LDFLAGS="${LDFLAGS}"
])


# AX_FLAGS_RESTORE
# ----------------
AC_DEFUN([AX_FLAGS_RESTORE],
[
   LIBS="${saved_LIBS}"
   CC="${saved_CC}"
   CFLAGS="${saved_CFLAGS}"
   CXXFLAGS="${saved_CXXFLAGS}"
   CPPFLAGS="${saved_CPPFLAGS}"
   LDFLAGS="${saved_LDFLAGS}"
])

# AX_GET_FIXED_BASE_ADDRESS
# -------------------------
AC_DEFUN([AX_GET_FIXED_BASE_ADDRESS],
[
    AX_FLAGS_SAVE()
    CFLAGS="-no-pie"
    LIBS=""
    AC_MSG_CHECKING([for non-PIE executable fixed base address])
    AC_RUN_IFELSE([
        AC_LANG_SOURCE([
            #include <stdio.h>
            #include <stdlib.h>
            #include <string.h>
            #include <unistd.h>
            #include <limits.h>

            #define DEFAULT_FIXED_BASE_ADDRESS 0x400000

            /* 
             * get_fixed_base_address() attempts to determine the base address of the current 
             * executable by reading /proc/self/maps. If it cannot determine the address,
             * it returns the fallback 0x400000, which is common in most Linux systems.
             */
            unsigned long get_fixed_base_address(void) {
                char exe_path[[PATH_MAX]];
                ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
                if (len == -1) {
                    /* Error reading the symlink, return fallback */
                    return DEFAULT_FIXED_BASE_ADDRESS;
                }
                exe_path[[len]] = '\0';  // Null-terminate the path

                FILE *maps = fopen("/proc/self/maps", "r");
                if (!maps) {
                    /* Error opening /proc/self/maps, return fallback */
                    return DEFAULT_FIXED_BASE_ADDRESS;
                }

                char line[[4096]];
                unsigned long base_addr = 0;
                int found = 0;

                /* Look for a mapping line that references our executable */
                while (fgets(line, sizeof(line), maps)) {
                    if (strstr(line, exe_path)) {
                        /* The first field of the line is the address range, e.g.,
                         * "00400000-00452000". We want the starting address.
                         */
                        char *dash = strchr(line, '-');
                        if (dash) {
                            *dash = '\0';  // Terminate at the dash to isolate the start address
                            base_addr = strtoul(line, NULL, 16);
                            found = 1;
                            break;
                        }
                    }
                }
                fclose(maps);
                return found ? base_addr : DEFAULT_FIXED_BASE_ADDRESS;
            }

            int main(void) {
                unsigned long base_addr = get_fixed_base_address();
                printf("0x%lx\n", base_addr); // Can't return the address as an exit status as int can't hold a 64-bit address, output to stdout instead
                return 0; // Use exit status 0 to indicate success running the test 
            }
        ])
    ],
    [base_addr=`./conftest 2>/dev/null`], dnl Run the test again manually to capture the base address from the stdout
    [base_addr='0x400000']) dnl Fallback to the default base address
    AC_DEFINE_UNQUOTED([DL_FIXED_BASE_ADDRESS], ${base_addr}, [Default fixed base address for non-PIE binaries])
    AX_FLAGS_RESTORE()
])