#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define READ_END 0
#define WRITE_END  1

// Pipe file descriptors for communication between parent and child parent

int parent_write[2];		//Parent wrtes to child
int child_write[2];			//Child writes to parent
FILE *addr2line_fd = NULL;	// File descriptor for reading addr2line output

int use_elfutils = 0;
void check_toolset_preference() {
    char *env = getenv("USE_ELFUTILS");
    if (env && strcmp(env, "1") == 0) {
        use_elfutils = 1;
    }
}
/** 
 * Initialize the addr2line provess with the given maps file.
 * This function sets up pipes for inter-process communication, forks a child
 * process to execute addr2line, and connects the pipes to the child's stdin and stdout.
 * 
 * @param maps		Path to the maps file for addr2line.
 */

//void addr2line_init(char *binary)
void addr2line_init(char *maps)
{
	check_toolset_preference();
	char *addr2line_started = getenv("ADDR2LINE_STARTED");

	//If addr2line is already started return early
	if (addr2line_started)
	{
	    return;
	}
	else
	{
		fprintf(stderr, "%s : Initializing addr2line\n", __func__);

		//Creating pipes for communication between parent and child processes
		if(pipe(parent_write) == -1 || pipe(child_write) == -1)
		{
			perror("Failed to create pipes");
			exit(EXIT_FAILURE);
		}

		//Fork a child process
		if (fork() == 0)
		{
			//In the child process
			close(parent_write[WRITE_END]);				// Close unused 'write end' of the parent_write pipe
			close(child_write[READ_END]);				// Close unused 'read end' of the child_write pipe

			
			if(dup2(parent_write[READ_END], STDIN_FILENO) == -1 ||			//Get stdin to read from from parent_write[READ_END] pipe
				dup2(child_write[WRITE_END], STDOUT_FILENO) == -1)		//Redirect stdout to write to child_write[WRITE_END] pipe
			{
				perror("Failed to duplicate file descriptor");
				exit(EXIT_FAILURE);
			}
			
			//Close the duplicated file descriptors
			close(parent_write[READ_END]);
			close(child_write[WRITE_END]);

			
			char *tool = use_elfutils ? "/home/snagarka/Projects/elfutils/bin/eu-addr2line" : "/home/snagarka/Projects/binutils/bin/addr2line";
			//Set up arguments for execution
			char *argv[] = { tool, "-M", maps, "-f", "-C", NULL };

			//Set environment variables to indicate addr2line has started
			putenv("ADDR2LINE_STARTED=1");

			//Replaces the current process with addr2line
			execvp(tool, argv);
		}
		else 
		{
			//In the parent process
			close(parent_write[READ_END]);		// Close unused 'read end' of the parent_write pipe
			close(child_write[WRITE_END]);		// Close unused 'write end' of the child_write pipe

			// Open a FILE stream to read the child's output
			addr2line_fd = fdopen(child_write[READ_END], "r");
			if (addr2line_fd == NULL) {
				perror("fdopen failed");
				exit(EXIT_FAILURE);
			}
		}
	}
}

/**
 * Translate a memory address into the corresponding function, file, line, and column
 * using addr2line
 * 
 * @param str 		The memory address to translate.
 * @param function 	Output buffer for the function name.
 * @param file 		Output buffer for the filename.
 * @param line		Output pointer for the line number.
 * @param col		Output pointer for the column number.
 */

void addr2line_translate(char *str, char *function, char *file, int *line, int *col)
{
	char buf[BUFSIZ];

	// Write the address to the addr2line process via the pipe.
	sprintf(buf, "%p\n", str); // Should this be p or s?
	
	fprintf(stderr, "%s : Writing %s to PIPE\n", __func__, buf);
	write(parent_write[WRITE_END], buf, strlen(buf));

	// Read the function name from addr2line's output
	fgets(buf, sizeof(buf), addr2line_fd);
	fprintf(stderr, "\tfirst fgets is  %s", buf);
	if (buf[strlen(buf)-1] == '\n') {
		strncpy(function, buf, strlen(buf)-1);		// Remove the newline character
	} else {
		function = "??";
	}

	// Read the filename, line number, and column number from addr2line's output
	fgets(buf, sizeof(buf), addr2line_fd);
	fprintf(stderr, "\tsecond fgets is  %s", buf);

	if (use_elfutils) {	

		// Parsing the column number
		char *last_colon = rindex(buf, ':'); 
		*col = atoi(last_colon + 1);
		*last_colon = '\0'; 

		// Parsing the line number
		char *first_colon = rindex(buf, ':');
		*line = atoi(first_colon + 1);
		*first_colon = '\0'; 

		// Copying the filename
		if (strlen(buf) < BUFSIZ) { 
			strncpy(file, buf, BUFSIZ);
			file[BUFSIZ - 1] = '\0'; 
		}
		fprintf(stderr, "function: %s file: %s line: %d col: %d\n", function, file, *line, *col);

	}

	#if 0
		if (buf[strlen(buf)-1] == '\n') {
			// Do exhaustive error checking in parsing, use explode? sscanf?
			char *line_str = index(buf, ':');
			*line = atoi(line_str + 1);
			line_str = index(line_str + 1, ':');
			*col = atoi(line_str + 1);
			*line_str = '\0';
			strcpy(file, buf);
		} else {
			file = "??";
			*line = 0;
			*col = 0;
		}

		fprintf(stderr, "function: %s file: %s line: %d col: %d\n", function, file, *line, *col);
	#endif
}

int main(int argc, char **argv)
{
	char function[BUFSIZ];
	char file[BUFSIZ];
	int line, col;

	//addr2line_init("/apps/GPP/BSCTOOLS/extrae/latest/openmpi_4_1_5/lib/libmpitrace.so");

	//Initialize addr2line with the current process's memory map
	addr2line_init("/proc/self/maps");

	//Translate addresses to readable format
	addr2line_translate("00000000000cc9b0", function, file, &line, &col);
	addr2line_translate("0000000000042ff0", function, file, &line, &col);

	return 0;
}
