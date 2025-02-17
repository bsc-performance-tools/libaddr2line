#pragma once

#include <stdio.h>
#include "maps.h"

// Available configuration options
#define OPTION_CLEAR_PRELOAD             (1 << 0) // Clears LD_PRELOAD to prevent other libraries to be loaded when addr2line command is exec'd
#define OPTION_KEEP_UNRESOLVED_ADDRESSES (1 << 1) // Keep the unresolved addresses in the output instead of "??"
#define OPTION_NON_PERSISTENT            (1 << 2) // Do not keep the addr2line process running in the background

enum {
	READ_END = 0,
	WRITE_END = 1
};

typedef struct code_loc
{
	void *adjusted_address;
	char *mapping_name;
	char *file;
	int line;
	int column;
	char *function;
	int translated;
} code_loc_t;

typedef struct addr2line_process
{
	int parentWrite[2];        // Pipes for communication between parent and child processes
	int childWrite[2];  
	FILE *outputStream;        // File descriptor for reading addr2line output
	maps_entry_t *execMapping; // Executable mapping associated with the addr2line process (only used when binutils is the backend and the input is a /proc/self/maps file)
	int isForked;              // Flag to indicate if the process has been forked already (deferred until the first translation)
} addr2line_process_t;

typedef struct addr2line
{
	char *inputObject;                // Path to the input object (either a binary or a dump of the /proc/self/maps)
	int useBackend;                   // User can override the default backend through the environment variable LIBADDR2LINE_BACKEND
	int setOptions;                   // Selected configuration options

	maps_t *procMaps;                 // Parsed /proc/self/maps file (only used when the input is a /proc/self/maps file or a parsed maps object)

	addr2line_process_t *processList; // Array of addr2line processes (> 1 when binutils is the backend and the input is a /proc/self/maps file, 1 otherwise)
	int numProcesses;
} addr2line_t;

// Function prototypes
addr2line_t * addr2line_init_file(char *object, int options);
addr2line_t * addr2line_init_maps(maps_t *parsed_maps, int options);
void addr2line_translate(addr2line_t *backend, void *address, code_loc_t *code_loc);
void addr2line_close(addr2line_t *backend);
