#pragma once

// Available configuration options
#define OPTION_KEEP_UNRESOLVED_ADDRESSES (1 << 0) // Keep the unresolved addresses in the output instead of "??"
										  
// Function prototypes
void addr2line_init(char *filename, int options);
void addr2line_translate(void *address, char **function, char **file, int *line, int *column);
