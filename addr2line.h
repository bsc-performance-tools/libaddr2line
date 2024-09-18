#pragma once

#include "config.h"

#define UNKNOWN_ADDRESS "??"

// Available configuration options
#define OPTION_KEEP_UNRESOLVED_ADDRESSES (1 << 0) // Keep the unresolved addresses in the output instead of "??"

// Available addr2line backends
enum {
#if defined(HAVE_ELFUTILS)
	USE_ELFUTILS,
#endif
#if defined(HAVE_BINUTILS)
	USE_BINUTILS,
#endif
	NUM_AVAILABLE_BACKENDS
};

// Default addr2line backend
#if defined(HAVE_ELFUTILS)
# define DEFAULT_BACKEND USE_ELFUTILS
#elif defined(HAVE_BINUTILS)
# define DEFAULT_BACKEND USE_BINUTILS
#else
# error "No addr2line backend available"
#endif

enum {
	READ_END = 0,
	WRITE_END = 1
};

// Function prototypes
void addr2line_init(char *filename, int options);
void addr2line_translate(void *address, char **function, char **file, int *line, int *column);
