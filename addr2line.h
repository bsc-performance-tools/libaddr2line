#pragma once

// Available configuration options
#define OPTION_CLEAR_PRELOAD             (1 << 0) // Clears LD_PRELOAD to prevent other libraries to be loaded when addr2line command is exec'd
#define OPTION_KEEP_UNRESOLVED_ADDRESSES (1 << 1) // Keep the unresolved addresses in the output instead of "??"
										  
// Function prototypes
void addr2line_init(char *filename, int options);
void addr2line_translate(void *address, char **function, char **file, int *line, int *column);
