#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "addr2line.h"
#include "config.h"

#define UNKNOWN_ADDRESS "??"

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

// File descriptor for reading addr2line output
FILE *addr2lineOutputStream = NULL;	

// Pipes for communication between parent and child processes
int parentWrite[2]; // Parent writes to child
int childWrite[2];  // Child writes to parent

// User can override the default backend through the environment variable LIBADDR2LINE_BACKEND
int useBackend = DEFAULT_BACKEND;

// Selected configuration options
int setOptions = 0;

/**
 * select_backend
 * 
 * Check the environment variable LIBADDR2LINE_BACKEND to determine whether to use elfutils (default) or binutils .
 */
static void select_backend() 
{
    char *env_libaddr2line_backend = getenv("LIBADDR2LINE_BACKEND");

	if (env_libaddr2line_backend != NULL) {
#if defined(HAVE_ELFUTILS)
		if (!strcmp(env_libaddr2line_backend, "elfutils")) {
			useBackend = USE_ELFUTILS;
		}
#endif
#if defined(HAVE_BINUTILS)
		if (!strcmp(env_libaddr2line_backend, "binutils")) {
			useBackend = USE_BINUTILS;
		}
#endif
	}
}

/**
 * is_binary_file
 * 
 * Check if the given file is a binary file by reading the first 512 bytes and checking for non-printable characters.
 * 
 * @param filename Path to the file to check.
 */
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
 * write_with_retry
 * 
 * Safe write wrapper that retries the write operation until all data is written. 
 */
static ssize_t write_with_retry(int fd, const void *buf, size_t count) {
    ssize_t written = 0, result = 0;

    while (count > 0) {
        result = write(fd, buf, count);
        
        if (result < 0) {
            if (errno != EINTR) {
				// Retry the full write if EINTR, other errors are fatal				
                perror("write failed");
                exit(EXIT_FAILURE);
            }
        }
		else {
        	// Successfully wrote some data, continue writing the rest if the write was partial
        	written += result;
        	buf = (const char *)buf + result;
        	count -= result;
		}
    }
    return written;
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
void addr2line_init(char *filename, int options)
{
	// Hack to prevent recursive initialization when addr2line_init is called from a library constructor, and the forked child process triggers the constructor again
	char *addr2line_started = getenv("LIBADDR2LINE_STARTED");
	if (addr2line_started) return;
	putenv("LIBADDR2LINE_STARTED=1");

#warning "Make sure the tests now use the new options flag"
	// Set the configuration options
	setOptions = options;

	// Check the backend to use
	select_backend();

	// Check if the filename is a binary file or a maps file
	int is_binary = is_binary_file(filename);

#if defined(HAVE_BINUTILS)
	// Check for invalid backend/object combinations
	if ((useBackend == USE_BINUTILS) && (!is_binary))
	{
		fprintf(stderr, "ERROR: addr2line_init: binutils backend can only be used with binary files\n");
		exit(EXIT_FAILURE);	
	}
#endif

	// Creating pipes for communication between parent and child processes
	if (pipe(parentWrite) == -1 || pipe(childWrite) == -1)
	{
		perror("Failed to create pipes");
		exit(EXIT_FAILURE);
	}

	// Fork a child process to run addr2line as a continuous background process
	if (fork() == 0)
	{
		// In the child process
		close(parentWrite[WRITE_END]); // Close unused 'write end' of the parent_write pipe
		close(childWrite[READ_END]);   // Close unused 'read end' of the child_write pipe

		if ((dup2(parentWrite[READ_END], STDIN_FILENO) == -1) || // Get stdin to read from from parent_write[READ_END] pipe
			(dup2(childWrite[WRITE_END], STDOUT_FILENO) == -1))  // Redirect stdout to write to child_write[WRITE_END] pipe
		{
			perror("Failed to duplicate file descriptor");
			exit(EXIT_FAILURE);
		}
		
		// Close the duplicated file descriptors
		close(parentWrite[READ_END]);
		close(childWrite[WRITE_END]);

		// Set up arguments for execution
		char *argv_elfutils[] = { ELFUTILS_ADDR2LINE, "-C", "-f", "-i", (is_binary ? "-e" : "-M"), filename, NULL };
		char *argv_binutils[] = { BINUTILS_ADDR2LINE, "-C", "-f", "-e", filename, NULL };
		char **argv = NULL;
#if defined(HAVE_ELFUTILS)
		if (useBackend == USE_ELFUTILS) {
			argv = argv_elfutils;			
		}
#endif
#if defined(HAVE_BINUTILS)
		if (useBackend == USE_BINUTILS) {
			argv = argv_binutils;
		}
#endif

		// Replaces the current process with addr2line backend
		execvp(argv[0], argv);
	}
	else 
	{
		// In the parent process
		close(parentWrite[READ_END]); // Close unused 'read end' of the parent_write pipe
		close(childWrite[WRITE_END]); // Close unused 'write end' of the child_write pipe

		// Open a FILE stream to read addr2line's backend output
		addr2lineOutputStream = fdopen(childWrite[READ_END], "r");
		if (addr2lineOutputStream == NULL) {
			perror("fdopen failed");
			exit(EXIT_FAILURE);
		}
	}
}

/**
 * addr2line_translate
 * 
 * Translate a memory address into the corresponding function, file, line, and column using addr2line.
 * 
 * @param address	The memory address to translate.
 * @param function 	Output buffer for the function name.
 * @param file 		Output buffer for the filename.
 * @param line		Output pointer for the line number.
 * @param column	Output pointer for the column number.
 */
void addr2line_translate(void *address, char **function, char **file, int *line, int *column)
{
	char address_str[BUFSIZ];
	char buf[BUFSIZ];

	// Format the address pointer as a string and pass it to addr2line
	sprintf(address_str, "%p\n", address); 
	write_with_retry(parentWrite[WRITE_END], address_str, strlen(address_str));

	// Read the function name from addr2line's output
	*function = NULL;
	if (fgets(buf, sizeof(buf), addr2lineOutputStream) != NULL)
	{
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = '\0'; // Remove the newline character
		} 
		// Copying the function name only if has been translated
		if (strcmp(buf, UNKNOWN_ADDRESS) != 0) {
			*function = strdup(buf);
		}
	}

	// Read the filename, line number, and column number from addr2line's output
	*file = NULL;
	*line = *column = 0;
	if (fgets(buf, sizeof(buf), addr2lineOutputStream) != NULL)
	{
#if defined(HAVE_ELFUTILS)
		// Parsing the column number (currently only available with elfutils)
		if (useBackend == USE_ELFUTILS)
		{	
			char *last_colon = strrchr(buf, ':'); 
			if (last_colon != NULL)
			{
				*column = atoi(last_colon + 1);
				*last_colon = '\0'; 
			}
		}
#endif
		// Parsing the line number
		char *first_colon = strrchr(buf, ':');
		if (first_colon != NULL)
		{
			*line = atoi(first_colon + 1);
			*first_colon = '\0'; 
		}
		// Copying the filename only if has been translated
		if (strcmp(buf, UNKNOWN_ADDRESS) != 0) {
			*file = strdup(buf);
		}
	}

	// Make sure we return something for function and file when the translation fails
	if (*function == NULL) *function = strdup((setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? address_str : UNKNOWN_ADDRESS);
	if (*file == NULL) *file = strdup((setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? address_str : UNKNOWN_ADDRESS);
}
