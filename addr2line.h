#pragma once

#include <stdio.h>

// Available configuration options
#define OPTION_CLEAR_PRELOAD             (1 << 0) // Clears LD_PRELOAD to prevent other libraries to be loaded when addr2line command is exec'd
#define OPTION_KEEP_UNRESOLVED_ADDRESSES (1 << 1) // Keep the unresolved addresses in the output instead of "??"

enum {
	READ_END = 0,
	WRITE_END = 1
};

typedef struct addr2line_process
{
	int parentWrite[2];  // Pipes for communication between parent and child processes
	int childWrite[2];  
	int useBackend;      // User can override the default backend through the environment variable LIBADDR2LINE_BACKEND
	int setOptions;      // Selected configuration options
	FILE *outputStream;  // File descriptor for reading addr2line output
} addr2line_process_t;

// Function prototypes
addr2line_process_t * addr2line_exec(char *object, int options);
void addr2line_translate(addr2line_process_t *backend, void *address, char **function, char **file, int *line, int *column);
