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

/**
 * select_backend
 * 
 * Check the environment variable LIBADDR2LINE_BACKEND to determine whether to use elfutils (default) or binutils .
 */
static int select_backend() 
{
    char *env_libaddr2line_backend = getenv("LIBADDR2LINE_BACKEND");

	if (env_libaddr2line_backend != NULL) {
#if defined(HAVE_ELFUTILS)
		if (!strcmp(env_libaddr2line_backend, "elfutils")) {
			return USE_ELFUTILS;
		}
#endif
#if defined(HAVE_BINUTILS)
		if (!strcmp(env_libaddr2line_backend, "binutils")) {
			return USE_BINUTILS;
		}
#endif
	}
	return DEFAULT_BACKEND;
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
 * addr2line_exec
 * 
 * Runs one or more addr2line processes for the given object, which can be either a
 * binary or a dump of the /proc/self/maps file. This function sets up pipes for 
 * inter-process communication, forks a child process to execute addr2line, and 
 * connects the pipes to the child's stdin and stdout.
 * 
 * @param object Path either to the binary or to a dump of the /proc/self/maps.
 * @param options Configuration options for the addr2line process.
 * @return addr2line_t structure containing the addr2line backend.
 */
addr2line_t * addr2line_exec(char *object, int options)
{
	addr2line_t *backend = NULL;
	
	// Hack to prevent our tracing libraries to trigger for the exec'd addr2line command
	if (options & OPTION_CLEAR_PRELOAD) unsetenv("LD_PRELOAD");

	// Allocate memory for the addr2line backend
	backend = malloc(sizeof(addr2line_t));
	if (backend == NULL) {
		fprintf(stderr, "ERROR: addr2line_exec: Out of memory\n");
		exit(EXIT_FAILURE);
	}

	// Store the input object
	backend->inputObject = strdup(object);

	// Check if the object is a binary file or a maps file
	int is_binary = is_binary_file(object);
	int is_mapping = !is_binary;

	// Set the configuration options
	backend->setOptions = options;
	char *env_non_persistent = getenv("LIBADDR2LINE_NON_PERSISTENT");
	if (env_non_persistent != NULL) {
		if (atoi(env_non_persistent) == 1) {
			backend->setOptions |= OPTION_NON_PERSISTENT;
		}
	}

	// Check the backend to use
	backend->useBackend = select_backend();

	// Parse the /proc/self/maps file if given
	backend->procMaps = NULL;
	if (is_mapping)
	{
		backend->procMaps = maps_parse_file(object);
	}

	// Determine the number of addr2line processes to spawn
	if ((is_mapping) && (backend->useBackend == USE_BINUTILS)) 
	{
		/*
		 * binutils can not take a /proc/self/maps file as input, 
		 * so we will instantiate individual addr2line processes for each 
		 * executable mapping in the /proc/self/maps file.
		 */
		int num_exec_entries = exec_mappings_size(backend->procMaps);
		backend->numProcesses = num_exec_entries;

		backend->processList = malloc(sizeof(addr2line_process_t) * num_exec_entries);
		if (backend->processList == NULL) {
			fprintf(stderr, "ERROR: addr2line_exec: Out of memory\n");
			exit(EXIT_FAILURE);
		}

		// Associate each executable mapping with its corresponding addr2line process
		maps_entry_t *exec_entry = exec_mappings(backend->procMaps);
		for (int i = 0; i < num_exec_entries; ++i) 
		{
			backend->processList[i].execMapping = exec_entry;
			exec_entry = next_exec_mapping(exec_entry);
		}
	}
	else 
	{
		/*
		* elfutils can directly process a /proc/self/maps file, allowing us to use a single addr2line instance.
		* Both elfutils and binutils can also handle a given binary file directly, which similarly requires only one addr2line instance.
		*/
		backend->processList = malloc(sizeof(addr2line_process_t));
		if (backend->processList == NULL) {
			fprintf(stderr, "ERROR: addr2line_exec: Out of memory\n");
			exit(EXIT_FAILURE);
		}
		backend->processList[0].execMapping = NULL; // No mapping for a single addr2line process
		backend->numProcesses = 1;
	}

	// Defer the fork until the first translation
	for (int i = 0; i < backend->numProcesses; ++i) 
	{
		/*	
		backend->processList[i].outputStream = NULL;
		backend->processList[i].parentWrite[0] = backend->processList[i].parentWrite[1] = -1;
		backend->processList[i].childWrite[0] = backend->processList[i].childWrite[1] = -1;
		*/
		backend->processList[i].isForked = 0;
	}

	return backend;
}

// XXX Fix this comment
// Spawn an addr2line process for each mapping (when using binutils with /proc/self/maps), or a single instance for other scenarios.

static addr2line_process_t * invoke_translator(addr2line_t *backend, void *address, char **adjusted_address_str)
{
	void *adjusted_address = address;
#warning "Ojito que adjusted_address es void ** y se usa con *adjusted_address en esta rutina, pero ya no es un parametro de I/O" 
	int is_binary = (backend->procMaps == NULL);
	static int fork_count = 1;

	addr2line_process_t *translator = &backend->processList[0];
	if (backend->numProcesses > 1)
	{
		// Find the mapping that contains the address
		for (int i = 0; i < backend->numProcesses; ++i)
		{
			addr2line_process_t *current_process = &backend->processList[i];
			if (address_in_mapping(current_process->execMapping, (unsigned long)address))
			{
				translator = current_process;
				adjusted_address = (void *)((unsigned long)address - translator->execMapping->start) + translator->execMapping->offset;
				break;
			}
		}
	}	
	char adjusted_address_endl[BUFSIZ];
	char adjusted_address_chomp[BUFSIZ];
	sprintf(adjusted_address_endl, "%p\n", adjusted_address); // Append '\n' when parent writes to child through pipe to unblock it
	sprintf(adjusted_address_chomp, "%p", adjusted_address); // Do not append '\n' when addr2line command receives the address directly

	if (!translator->isForked)
	{
		fprintf(stderr, "!!!!!!!! [DEBUG] FORK NEW ADDR2LINE %d\n", fork_count);
		fork_count ++;

		// Creating pipes for communication between parent and child processes
		if (pipe(translator->parentWrite) == -1 || pipe(translator->childWrite) == -1)
		{
			perror("Failed to create pipes");
			exit(EXIT_FAILURE);
		}

		// Fork a child process to run addr2line as a continuous background process
		if (fork() == 0)
		{
			// In the child process
			close(translator->parentWrite[WRITE_END]); // Close unused 'write end' of the parent_write pipe
			close(translator->childWrite[READ_END]);   // Close unused 'read end' of the child_write pipe

			if ((dup2(translator->parentWrite[READ_END], STDIN_FILENO) == -1) || // Get stdin to read from from parent_write[READ_END] pipe
				(dup2(translator->childWrite[WRITE_END], STDOUT_FILENO) == -1))  // Redirect stdout to write to child_write[WRITE_END] pipe
			{
				perror("Failed to duplicate file descriptor");
				exit(EXIT_FAILURE);
			}
			
			// Close the duplicated file descriptors
			close(translator->parentWrite[READ_END]);
			close(translator->childWrite[WRITE_END]);

			/*
			* Set up arguments for execution
			* elfutils uses a single addr2line process to handle either the specified binary (-e binary) or /proc/self/maps (-M maps_file).
			* binutils uses a single addr2line process for a specified binary (-e binary), or multiple processes for each executable mapping (-e mapping1, -e mapping2, etc.).
			*/
#if 0
			char adjusted_address_str[BUFSIZ];
			sprintf(adjusted_address_str, "%p", *adjusted_address); // Do not append '\n' to the address string here, as it corrupts the address read by the child process.
#endif
			char *argv_elfutils[] = { ELFUTILS_ADDR2LINE, "-C", "-f", "-i", (is_binary ? "-e" : "-M"), backend->inputObject, (backend->setOptions & OPTION_NON_PERSISTENT ? adjusted_address_chomp : NULL), NULL };
			char *argv_binutils[] = { BINUTILS_ADDR2LINE, "-C", "-f", "-e", (is_binary ? backend->inputObject : translator->execMapping->pathname), (backend->setOptions & OPTION_NON_PERSISTENT ? adjusted_address_chomp : NULL), NULL };
			char **argv = NULL;
	#if defined(HAVE_ELFUTILS)
			if (backend->useBackend == USE_ELFUTILS) {
				argv = argv_elfutils;			
				fprintf(stderr, "[DEBUG] argv => %s %s %s %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], adjusted_address_str);
			}
	#endif
	#if defined(HAVE_BINUTILS)
			if (backend->useBackend == USE_BINUTILS) {
				argv = argv_binutils;
				fprintf(stderr, "[DEBUG] argv => %s %s %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], adjusted_address_str);
			}
	#endif
			// Replaces the current process with addr2line backend
			execvp(argv[0], argv);
		}
		else 
		{
			// In the parent process

			// Keep forking with every new translation if non-persistent option is set
			if (!(backend->setOptions & OPTION_NON_PERSISTENT)) translator->isForked = 1; 

			close(translator->parentWrite[READ_END]); // Close unused 'read end' of the parent_write pipe
			close(translator->childWrite[WRITE_END]); // Close unused 'write end' of the child_write pipe

			// Open a FILE stream to read addr2line's backend output
			translator->outputStream = fdopen(translator->childWrite[READ_END], "r");
			if (translator->outputStream == NULL) {
				perror("fdopen failed");
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "!!!!!!!! [DEBUG] FD's %d %d %d\n", fileno(translator->outputStream), translator->parentWrite[WRITE_END], translator->childWrite[READ_END]);
		}
	}

	if (!(backend->setOptions & OPTION_NON_PERSISTENT))
	{
#if 0
		char adjusted_address_str[BUFSIZ];
		sprintf(adjusted_address_str, "%p\n", *adjusted_address); // Append '\n' to the address string here to unblock the child process
#endif
		write_with_retry(translator->parentWrite[WRITE_END], adjusted_address_endl, strlen(adjusted_address_endl));
	}

	*adjusted_address_str = strdup(adjusted_address_chomp);
	return translator;
}

static void free_translator(addr2line_t *backend, addr2line_process_t *translator)
{
	if (backend->setOptions & OPTION_NON_PERSISTENT)
	{
		// Clean up remaining descriptors 
		fclose(translator->outputStream);
		close(translator->parentWrite[WRITE_END]);
		close(translator->childWrite[READ_END]);
	} 
}

/**
 * addr2line_translate
 * 
 * Translate a memory address into the corresponding function, file, line, and column using addr2line.
 * 
 * @param backend   The handler of the running addr2line process
 * @param address	The memory address to translate.
 * @param function 	Output buffer for the function name.
 * @param file 		Output buffer for the filename.
 * @param line		Output pointer for the line number.
 * @param column	Output pointer for the column number.
 * @param mapping	Output buffer for the mapping name.
 */
void addr2line_translate(addr2line_t *backend, void *address, char **function, char **file, int *line, int *column, char **mapping_name)
{
	char *adjusted_address_str = NULL;
	char buf[BUFSIZ];

	addr2line_process_t *translator = invoke_translator(backend, address, &adjusted_address_str);

#if 0
	if (!(backend->setOptions & OPTION_NON_PERSISTENT))
	{
		fprintf(stderr, "[DEBUG] Writing to pipe: %p\n", adjusted_address);
		// Format the address pointer as a string and pass it to addr2line
		sprintf(adjusted_address_str, "%p\n", adjusted_address); // Append '\n' to the address string here to unblock the child process
		write_with_retry(translator->parentWrite[WRITE_END], adjusted_address_str, strlen(adjusted_address_str));
	}
	else fprintf(stderr, "[DEBUG] Skipping writing to pipe\n");
#endif

	// Read the function name from addr2line's output
	*function = NULL;
	if (fgets(buf, sizeof(buf), translator->outputStream) != NULL)
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
	if (fgets(buf, sizeof(buf), translator->outputStream) != NULL)
	{
#if defined(HAVE_ELFUTILS)
		// Parsing the column number (currently only available with elfutils)
		if (backend->useBackend == USE_ELFUTILS)
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
#warning "Ojito adjusted_address_str no vale nada ahora"
	if (*function == NULL) *function = strdup((backend->setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? adjusted_address_str : UNKNOWN_ADDRESS);
	if (*file == NULL) *file = strdup((backend->setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? adjusted_address_str : UNKNOWN_ADDRESS);

	// Get the mapping name
	if (translator->execMapping != NULL) 
	{
		// If the addr2line process is associated with a specific mapping, use the mapping name
		*mapping_name = strdup(translator->execMapping->pathname);
	}
	else
	{
		if (backend->procMaps != NULL)
		{
			// If the input is a maps file, find the mapping that contains the address
			maps_entry_t *entry = search_in_exec_mappings(backend->procMaps, (unsigned long)address);
			if (entry != NULL) *mapping_name = strdup(entry->pathname);
			else *mapping_name = strdup(maps_main_exec(backend->procMaps));
		}
		else
		{
			// If no maps file was given, use the input object as the mapping name
			*mapping_name = strdup(backend->inputObject);
		}
	}

	free(adjusted_address_str);
	free_translator(backend, translator);
}
