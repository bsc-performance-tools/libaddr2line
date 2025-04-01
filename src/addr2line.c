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

static addr2line_t * addr2line_init(char *object, maps_t *maps, int options);

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
 * Initializes one or more addr2line processes for the given object, which can be 
 * either a binary or a dump of the /proc/self/maps file. This function parses
 * the maps file if given, determines the number of addr2line processes to spawn,
 * and initializes structures, leaving the actual fork deferred until the first 
 * translation.
 * 
 * @param object Path either to the binary or to a dump of the /proc/self/maps.
 * @param options Configuration options for the addr2line process.
 * @return Pointer to the addr2line backend handler.
 */

addr2line_t * addr2line_init_file(char *object, int options)
{
	return addr2line_init(object, NULL, options);
}

addr2line_t * addr2line_init_maps(maps_t *parsed_maps, int options)
{
	return addr2line_init(maps_path(parsed_maps), parsed_maps, options);
}

static addr2line_t * addr2line_init(char *object, maps_t *parsed_maps, int options)
{
	addr2line_t *backend = NULL;
	
	// Hack to prevent our tracing libraries to trigger for the exec'd addr2line command
	if (options & OPTION_CLEAR_PRELOAD) unsetenv("LD_PRELOAD");

	// Allocate memory for the addr2line backend
	backend = malloc(sizeof(addr2line_t));
	if (backend == NULL) {
		fprintf(stderr, "ERROR: addr2line_init: Out of memory\n");
		exit(EXIT_FAILURE);
	}

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

	int is_binary, is_mapping;
	// Check if the input is a binary file, a maps file, or a parsed maps object
	if (parsed_maps != NULL) {
		// Save the given parsed maps object
		is_binary = 0;
		is_mapping = 1;
		backend->inputObject = strdup(maps_path(parsed_maps));
		backend->procMaps = parsed_maps;
	}
	else {
		// Check if the object is a binary file or a maps file and store it
		is_binary = is_binary_file(object);
		is_mapping = !is_binary;
		backend->inputObject = strdup(object);

		// Parse the /proc/self/maps file if given
		backend->procMaps = NULL;
		if (is_mapping)	{
			backend->procMaps = maps_parse_file(object, NULL, 0);
		}
	}

	// Determine the number of addr2line processes to spawn
	if ((is_mapping) && (backend->useBackend == USE_BINUTILS)) 
	{
		/*
		 * binutils can not take a /proc/self/maps file as input, 
		 * so we will instantiate individual addr2line processes for each 
		 * executable mapping in the maps file.
		 */
		fprintf(stderr, "addr2line_init: binutils backend with maps file\n");
		int num_exec_entries = exec_mappings_size(backend->procMaps);
		backend->numProcesses = num_exec_entries;

		backend->processList = malloc(sizeof(addr2line_process_t) * num_exec_entries);
		if (backend->processList == NULL) {
			fprintf(stderr, "ERROR: addr2line_init: Out of memory\n");
			exit(EXIT_FAILURE);
		}

		// Associate one addr2line process to each executable mapping 
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
		backend->processList[0].execMapping = NULL; // No mapping associated for a single addr2line process
		backend->numProcesses = 1;
	}

	// Defer the fork until the first translation
	for (int i = 0; i < backend->numProcesses; ++i) 
	{
		backend->processList[i].isForked = 0;
	}

	return backend;
}

/**
 * adjust_address
 * 
 * Iterate over the addr2line processes to find the one whose mapping contains the address.
 * Then adjust the address to the mapping offset, and return the adjusted address, as well
 * as the addr2line process to use for the translation. The manual offsetting is only necessary
 * for shared libraries under binutils (see comments below for details).
 * 
 * @param backend Pointer to the addr2line backend handler.
 * @param address Address to adjust (if required).
 * @param translator Pointer to the addr2line process handler.
 */
static void * adjust_address(addr2line_t *backend, void *address, addr2line_process_t **translator)
{
	void *adjusted_address = address;
	
	// Only binutils require manual adjustment of the address to the mapping offset.
	if ((backend->useBackend == USE_BINUTILS) && (backend->procMaps))
	{
		// If multiple addr2line processes are used, find the one whose mapping contains the address
		for (int i = 0; i < backend->numProcesses; ++i)
		{
			addr2line_process_t *current_process = &backend->processList[i];

			// The check for mapping_is_position_independent relies entirely on libmagic and is quite heuristic
			if ((address_in_mapping(current_process->execMapping, (unsigned long)address)) && (!mapping_is_at_fixed_base_address(current_process->execMapping)))
			{
				/* TL;DR: Adjust the address to the mapping offset.
				 *        This applies both to shared libraries and -fPIE/-pie executables, regardless of ASLR.
				 *        Offsets do not apply to -no-pie executables that are mapped to a fixed base address (usually 0x400000).
				 *
				 * The following example illustrates the /proc/self/maps for a -no-pie executable linked with a shared library.
				 * We have observed that for a given binary or shared library, there are multiple mappings in the maps file, 
				 * with only one being executable [--x-], which is not the first entry in the list, e.g.:
				 *
				 * 00400000-00403000 r--p 00000000 00:2f 391035422   my_main
				 * 00403000-0047a000 r-xp 00003000 00:2f 391035422   my_main
				 * 0047a000-0049e000 r--p 0007a000 00:2f 391035422   my_main
				 * ...
				 * 7f9c93cf7000-7f9c93d01000 r--p 00000000 00:35 391035434   libshared.so
				 * 7f9c93d01000-7f9c93d78000 r-xp 0000a000 00:35 391035434   libshared.so
				 * 7f9c93d78000-7f9c93d9c000 r--p 00081000 00:35 391035434   libshared.so
				 * 
				 * The addresses that we are capturing belong to the executable mappings [--x-], for instance, 
				 * 0x403e46, which belongs to the executable mapping of my_main; and 0x7f9c93d01e32, which
				 * belongs to the executable mapping of libshared.so. For the latter, we adjust the address
				 * by substracting the base address of the mapping (7f9c93d01000) and adding the offset (0000a000).
				 * The resulting adjusted address (0xae32) can be successfully translated:
				 * 
				 * > binutils-addr2line -e libshared.so 0xae32
				 * bye_world
				 * /home/user/libshared.c:13
				 * 
				 * However, if the same offset operation is applied to an address from the main binary, 
				 * the translation fails. In the example, the adjusted address would be computed as:
				 * 0x403e46 (original) - 00403000 (start) + 00003000 (offset) = 0x3e46
				 * 
				 * Then, the translation fails:
				 * > binutils-addr2line -e my_main 0x3e46
				 * ??
				 * ??:0
				 * 
				 * For the addresses from -no-pie executables that belong to the main binary, we need to leave them 
				 * unchanged for the translation to work:
				 * 
				 * > binutils-addr2line -e my_main 0x403e46 
				 * hello_world
				 * /home/user/my_main.c:42
				 * 
				 */

				/*
				 * Regardless of -fPIE / -no-pie, note that the first entry in the maps file for the main binary is mapped at offset zero:
				 *                               
				 * 00400000-00403000 r--p 00000000 00:2f 391035422   my_main <= The first mapping for the main binary is offset zero
				 * 00403000-0047a000 r-xp 00003000 00:2f 391035422   my_main <= Address 0x403e46 belongs to this mapping
				 *  
				 * It's not clear if some extra offsetting would be necessary if the first entry offset was above zero. 
				 * We leave this long explanation as a note for future reference, in case some translations fail.
				 */
				*translator = current_process;
				adjusted_address = absolute_to_relative(current_process->execMapping, address);
				return adjusted_address;
			}
		}
	}
	// Default to the first addr2line process and leave the address unchanged
	*translator = &backend->processList[0]; 
	return address;
}

/**
 * invoke_translator
 * 
 * Invokes the addr2line backend to translate the given address. This function
 * determines the addr2line process to use based on the mapping offset, and forks
 * a child process to run addr2line. The forked process runs as a continuous 
 * background process, unless OPTION_NON_PERSISTENT is set, in which case the
 * process is spawned and ended for each translation. 
 * 
 * Depending on the backend used, multiple addr2line processes are spawned (one 
 * for each mapping when using binutils with a maps file), or just a single instance 
 * for other scenarios.
 * 
 * @param backend Pointer to the addr2line backend handler.
 * @param address Address to translate.
 * @param[out] adjusted_address_str Address string passed to the addr2line command after adjusting it to the mapping offset if needed.
 */
static addr2line_process_t * invoke_translator(addr2line_t *backend, void *address, void **adjusted_address_ptr, char **adjusted_address_str)
{
	addr2line_process_t *translator = NULL;
	int is_binary = (backend->procMaps == NULL);
	void *adjusted_address = adjust_address(backend, address, &translator);

	// Format the address string to be passed to the addr2line command
	char adjusted_address_endl[BUFSIZ];
	char adjusted_address_chomp[BUFSIZ];
	sprintf(adjusted_address_endl, "%p\n", adjusted_address); // Append '\n' when parent writes to child through pipe to unblock it
	sprintf(adjusted_address_chomp, "%p", adjusted_address); // Do not append '\n' when addr2line command receives the address directly

	// Fork the addr2line process if not already forked, or if non-persistent option is set
	if ((!translator->isForked) || (backend->setOptions & OPTION_NON_PERSISTENT))
	{
		// Create pipes for communication between parent and child processes
		if (pipe(translator->parentWrite) == -1 || pipe(translator->childWrite) == -1)
		{
			perror("Failed to create pipes");
			exit(EXIT_FAILURE);
		}

		if (fork() == 0)
		{
			// In the child process

			// Close unused ends of the pipes
			close(translator->parentWrite[WRITE_END]);
			close(translator->childWrite[READ_END]);

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
			* If the non-persistent option is set, the address is passed directly to the addr2line command and the process will end after the translation.
			* Otherwise, the process will stall in a read loop until the address is passed later through the pipe.
			*/
			char **argv = NULL;
#if defined(HAVE_ELFUTILS)
			char *argv_elfutils[] = { ELFUTILS_ADDR2LINE, "-C", "-f", "-i", (is_binary ? "-e" : "-M"), backend->inputObject, (backend->setOptions & OPTION_NON_PERSISTENT ? adjusted_address_chomp : NULL), NULL };
			if (backend->useBackend == USE_ELFUTILS) {
				argv = argv_elfutils;			
			}
#endif
#if defined(HAVE_BINUTILS)
			char *argv_binutils[] = { BINUTILS_ADDR2LINE, "-C", "-f", "-e", (is_binary ? backend->inputObject : translator->execMapping->pathname), (backend->setOptions & OPTION_NON_PERSISTENT ? adjusted_address_chomp : NULL), NULL };
			if (backend->useBackend == USE_BINUTILS) {
				argv = argv_binutils;
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
		}
	}

	// If the addr2line process is persistent, pass now the address to translate to the background process
	if (!(backend->setOptions & OPTION_NON_PERSISTENT))
	{
		write_with_retry(translator->parentWrite[WRITE_END], adjusted_address_endl, strlen(adjusted_address_endl));
	}

	// Return the adjusted address string that was passed to addr2line
	*adjusted_address_ptr = adjusted_address;
	*adjusted_address_str = strdup(adjusted_address_chomp);
	return translator;
}

/**
 * free_translator
 * 
 * Close the pipes associated with the given addr2line process, only if flagged as non-persistent!
 * 
 * @param backend Pointer to the addr2line backend handler.
 * @param translator Pointer to the addr2line process handler.
 */
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
 * Translate a memory address into the corresponding function, file, line, column (elfutils only) and mapping (if maps file was given).
 * 
 * @param backend  The handler of the running addr2line process
 * @param address  The memory address to translate.
 * @param code_loc The structure to store the translation results.
 */

void addr2line_translate(addr2line_t *backend, void *address, code_loc_t *code_loc)
{
	char buf[BUFSIZ];
	void *adjusted_address_ptr = NULL;
	char *adjusted_address_str = NULL;
	int translated = 0; 

	// Select the addr2line process to use and invoke it
	addr2line_process_t *translator = invoke_translator(backend, address, &adjusted_address_ptr, &adjusted_address_str);
	code_loc->adjusted_address = adjusted_address_ptr;

	// Read the function name from addr2line's output
	code_loc->function = NULL;
	if (fgets(buf, sizeof(buf), translator->outputStream) != NULL)
	{
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = '\0'; // Remove the newline character
		} 
		// Copying the function name only if has been translated
		if (strcmp(buf, UNKNOWN_ADDRESS) != 0) {
			code_loc->function = strdup(buf);
			translated = 1;
		}
	}

	// Read the filename, line number, and column number from addr2line's output
	code_loc->file = NULL;
	code_loc->line = code_loc->column = 0;
	if (fgets(buf, sizeof(buf), translator->outputStream) != NULL)
	{
#if defined(HAVE_ELFUTILS)
		// Parsing the column number (currently only available with elfutils)
		if (backend->useBackend == USE_ELFUTILS)
		{	
			char *last_colon = strrchr(buf, ':'); 
			if (last_colon != NULL)
			{
				code_loc->column = atoi(last_colon + 1);
				*last_colon = '\0'; 
				if (code_loc->column > 0) translated = 1;
			}
		}
#endif
		// Parsing the line number
		char *first_colon = strrchr(buf, ':');
		if (first_colon != NULL)
		{
			code_loc->line = atoi(first_colon + 1);
			*first_colon = '\0'; 
			if (code_loc->line > 0) translated = 1;
		}
		// Copying the filename only if has been translated
		if (strcmp(buf, UNKNOWN_ADDRESS) != 0) {
			code_loc->file = strdup(buf);
			translated = 1;
		}
	}
	code_loc->translated = translated;

	// Make sure we return something for function and file when the translation fails
	if (code_loc->function == NULL) code_loc->function = strdup((backend->setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? adjusted_address_str : UNKNOWN_ADDRESS);
	if (code_loc->file == NULL) code_loc->file = strdup((backend->setOptions & OPTION_KEEP_UNRESOLVED_ADDRESSES) ? adjusted_address_str : UNKNOWN_ADDRESS);

	// Get the mapping name
	if (translator->execMapping != NULL) {
		// If the addr2line process is associated with a specific mapping (binutils), use that mapping name
		code_loc->mapping_name = strdup(translator->execMapping->pathname);
	}
	else if (backend->procMaps != NULL) {
		// If the input was a maps file (elfutils), find the mapping that contains the address
		maps_entry_t *entry = search_in_exec_mappings(backend->procMaps, (unsigned long)address);
		if (entry != NULL) code_loc->mapping_name = strdup(entry->pathname);
		else code_loc->mapping_name = strdup(maps_main_binary(backend->procMaps));
	}
	else {
		// If no maps file was given (binutils/elfutils), use the input binary as the mapping name only when the translation was successful
		if (translated) code_loc->mapping_name = strdup(backend->inputObject);
		else code_loc->mapping_name = strdup(UNKNOWN_ADDRESS);
	}

	// Free resources
	free(adjusted_address_str);
	free_translator(backend, translator);
}

/**
 * addr2line_close
 * 
 * Close the addr2line backend and free all resources.
 * 
 * @param backend Pointer to the addr2line backend handler.
 */
void addr2line_close(addr2line_t *backend)
{
	if (backend->procMaps != NULL) maps_free(backend->procMaps);
	free(backend->inputObject);
	for (int i = 0; i < backend->numProcesses; ++i)	{
		// Clear the non-persistent flag to force closing all descriptors
		backend->setOptions &= ~OPTION_NON_PERSISTENT;
		free_translator(backend, &backend->processList[i]);
	}
	free(backend->processList);
	free(backend);
}
