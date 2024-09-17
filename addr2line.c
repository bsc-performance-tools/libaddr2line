#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"

#define UNKNOWN_FUNCTION "??"

enum {
	READ_END = 0,
	WRITE_END = 1
};

// File descriptor for reading addr2line output
FILE *addr2line_fd = NULL;	

// Pipes for communication between parent and child processes
int parent_write[2];		// Parent writes to child
int child_write[2];			// Child writes to parent

// Flag to indicate whether to use elfutils or binutils

enum {
	USE_ELFUTILS = 0,
	USE_BINUTILS,
	NUM_BACKENDS
};

char *backend_commands[NUM_BACKENDS] = {
	[USE_ELFUTILS] = ELFUTILS_ADDR2LINE,
	[USE_BINUTILS] = BINUTILS_ADDR2LINE
};

int backend = USE_ELFUTILS;

/**
 * select_backend
 * 
 * Check the environment variable LIBADDR2LINE_BACKEND to determine whether to use elfutils (default) or binutils .
 */
static int select_backend() {
    char *env_libaddr2line_backend = getenv("LIBADDR2LINE_BACKEND");

	if (env_libaddr2line_backend != NULL) {
		if (!strcmp(env_libaddr2line_backend, "elfutils")) {
			return USE_ELFUTILS;
		} else if (!strcmp(env_libaddr2line_backend, "binutils")) {
			return USE_BINUTILS;
		} else {
			return USE_ELFUTILS;
		}
	}
}

static int is_binary_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Unable to open file");
        return -1;
    }

    unsigned char buffer[512]; 
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);

    for (size_t i = 0; i < bytes_read; i++) {
        // If we find a non-printable and non-whitespace character, it's likely a binary file
        if (!isprint(buffer[i]) && !isspace(buffer[i])) {
            return 1; // Binary file
        }
    }
    return 0; // Text file
}

/** 
 * addr2line_init
 * 
 * Initialize the addr2line process with the given maps file.
 * This function sets up pipes for inter-process communication, forks a child
 * process to execute addr2line, and connects the pipes to the child's stdin and stdout.
 * 
 * @param filename Path either to the binary or to a dump of the /proc/self/maps.
 */
void addr2line_init(char *filename)
{
	// Hack to prevent recursive initialization when addr2line_init is called from a library constructor, and the forked child process triggers the constructor again
	char *addr2line_started = getenv("LIBADDR2LINE_STARTED");
	if (addr2line_started) return;
	putenv("LIBADDR2LINE_STARTED=1");

	// Check the backend to use
	backend = select_backend();

	// Check if the filename is a binary file or a maps file
	int is_binary = is_binary_file(filename);

	// Check for invalid backend/object combinations
	if ((backend == USE_BINUTILS) && (!is_binary))
	{
		fprintf(stderr, "ERROR: addr2line_init: binutils backend can only be used with binary files\n");
		exit(EXIT_FAILURE);	
	}

	// Creating pipes for communication between parent and child processes
	if (pipe(parent_write) == -1 || pipe(child_write) == -1)
	{
		perror("Failed to create pipes");
		exit(EXIT_FAILURE);
	}

	// Fork a child process to run addr2line as a continuous background process
	if (fork() == 0)
	{
		// In the child process
		close(parent_write[WRITE_END]); // Close unused 'write end' of the parent_write pipe
		close(child_write[READ_END]);   // Close unused 'read end' of the child_write pipe

		if ((dup2(parent_write[READ_END], STDIN_FILENO) == -1) || // Get stdin to read from from parent_write[READ_END] pipe
			(dup2(child_write[WRITE_END], STDOUT_FILENO) == -1))  // Redirect stdout to write to child_write[WRITE_END] pipe
		{
			perror("Failed to duplicate file descriptor");
			exit(EXIT_FAILURE);
		}
		
		// Close the duplicated file descriptors
		close(parent_write[READ_END]);
		close(child_write[WRITE_END]);

		// Set up arguments for execution
		char *argv_elfutils[] = { ELFUTILS_ADDR2LINE, (is_binary ? "-e" : "-M"), filename, "-C", "-f", "-i", NULL };
		char *argv_binutils[] = { BINUTILS_ADDR2LINE, "-e", filename, "-C", "-f", NULL };
		char **argv = (backend == USE_BINUTILS ? argv_binutils : argv_elfutils); 

		// Replaces the current process with addr2line
		execvp(backend, argv);
	}
	else 
	{
		// In the parent process
		close(parent_write[READ_END]); // Close unused 'read end' of the parent_write pipe
		close(child_write[WRITE_END]); // Close unused 'write end' of the child_write pipe

		// Open a FILE stream to read addr2line's output
		addr2line_fd = fdopen(child_write[READ_END], "r");
		if (addr2line_fd == NULL) {
			perror("fdopen failed");
			exit(EXIT_FAILURE);
		}
	}
}

/**
 * addr2line_translate
 * 
 * Translate a memory address into the corresponding function, file, line, and column
 * using addr2line
 * 
 * @param str 		The memory address to translate.
 * @param function 	Output buffer for the function name.
 * @param file 		Output buffer for the filename.
 * @param line		Output pointer for the line number.
 * @param column	Output pointer for the column number.
 */
void addr2line_translate(char *address, char **function, char **file, int *line, int *column)
{
	char buf[BUFSIZ];

	// Format the address pointer as a string and pass it to addr2line
	sprintf(buf, "%p\n", address); 
	write(parent_write[WRITE_END], buf, strlen(buf));

	// Read the function name from addr2line's output
	fgets(buf, sizeof(buf), addr2line_fd);
	if (buf[strlen(buf)-1] == '\n') {
		buf[strlen(buf)-1] = '\0'; // Remove the newline character
		*function = strdup(buf);
	} else {
		*function = strdup(UNKNOWN_FUNCTION);
	}

	// Read the filename, line number, and column number from addr2line's output
	fgets(buf, sizeof(buf), addr2line_fd);

	*line = *column = 0;
	if (backend == USE_ELFUTILS)
	{	
		// Parsing the column number
		char *last_colon = strrchr(buf, ':'); 
		if (last_colon != NULL)
		{
			*column = atoi(last_colon + 1);
			*last_colon = '\0'; 
		}
	}

	// Parsing the line number
	char *first_colon = strrchr(buf, ':');
	if (first_colon != NULL)
	{
		*line = atoi(first_colon + 1);
		*first_colon = '\0'; 
	}
	// Copying the filename
	*file = strdup(buf);

	fprintf(stderr, "function: %s file: %s line: %d column: %d\n", *function, *file, *line, *column);
}
