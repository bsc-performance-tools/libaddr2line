#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
//#include <libmpi.h>
#include <mpi.h>
#include <execinfo.h>

//External functions to initialize addr2line and translate addresses
extern void addr2line_init(char *);
extern void addr2line_translate(char *, char *, char *, int *, int *);

/**
 * Copies the contentsof one file to another.
 * 
 * @param to	The destination file path.
 * @param from 	The source file path.
 * @return 0	on success, -1 on failure.
 */
int copyfile(const char *to, const char *from)
{
	int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

	//Open the source file
    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

	// Open the destination file, ensuring it does not already exist
    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

	//Copy data from the source file to the destination file
    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

		//Write the data to the destination file
        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

	//Check if the read operation was successful
    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

	//Clean up file descriptors on error
    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

/**
 * Initialize the addr2line information for a specific MPI rank.
 * 
 * @param id	The MPI rank identifier (typically the process ID).
 */

void Initialize_addr2info(unsigned id)
{
	char copiedmaps[16];
	sprintf(copiedmaps, "rank_%d.maps", id);

	//Copy the memory maps file for the current process
	copyfile(copiedmaps, "/proc/self/maps");

	//Initialize addr2line with the copied maps file
	addr2line_init(copiedmaps);
}

/**
 * Constructor function that runs before the main function.
 * It ensures addr2line is initialized once for the process.
 */

__attribute__((constructor(200)))
void init()
{
	static int has_constructor_run = 0;
	if (!has_constructor_run)
	{
		has_constructor_run = 1;
		Initialize_addr2info(getpid());
	}
}

/**
 * Wrapper for MPI_Init that initializes the MPI environment and performs tracing.
 * 
 * @param argc	Pointer to the number of arguments.
 * @param argv	Pointer to the argument vector.
 * @return		The return value of PMPI_Init.
 */
int MPI_Init(int *argc, char ***argv)
{
	int rank, ret;

	ret = PMPI_Init(argc, argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	fprintf(stderr, "[TASK %d] Hello from tracing library %s!\n", rank, __func__);

	return ret;
}

/**
 * Wrapper for MPI_Barrier that synchronizes processes and performs tracing.
 * 
 * @param comm	The communicator over which the barrier is performed.
 * @return		The return value of PMPI_Barrier.
 */

int MPI_Barrier(MPI_Comm comm)
{
	int rank;

	// Get the MPI rank and print a tracing message with the rank
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	fprintf(stderr, "[TASK %d] Hello from tracing library %s!\n", rank, __func__);

	void *callstack[8];
	int   callstack_size;
	char *function, *file;
	int line, col;

#if 0
	callstack_size = backtrace(callstack, 8);

	for (int i = 0; i<callstack_size; i++)
	{
		fprintf(stderr, "[TASK %d] translated address %lx\n", rank, callstack[i]);
		//addr2line_translate(callstack[i], function, file, &line, &col);
		//fprintf(stderr, "as %s in file %s:%d:%d\n", function, file, line, col);
	}
#endif
#if 0
	Dl_info info;

	if (dladdr(&MPI_Barrier, &info));
	{
		fprintf(stderr, "[TASK %d] dladdr of %lx fname = %s\n", rank, &MPI_Barrier, info.dli_fname);
		addr2line_translate(info.dli_fbase, function, file, &line, &col);
		addr2line_translate(info.dli_saddr, function, file, &line, &col);
	}
#endif
#if 1
	// Translate the address pf a symbol using addr2line
	void *bar_real = dlsym(RTLD_DEFAULT, "bar");

	fprintf(stderr, "[TASK %d] address to translate %lx\n", rank, bar_real);
	addr2line_translate(bar_real, function, file, &line, &col);
	fprintf(stderr, "[TASK %d] address to translate %lx\n", rank, &MPI_Barrier);
	addr2line_translate(&MPI_Barrier, function, file, &line, &col);
#endif

	// Call to the original MPI_Barrier function
	return PMPI_Barrier(comm);
}

/**
 * Wrapper for MPI_Finalize that finalizes the MPI environment and performs tracing.
 * 
 * @return	The return value of PMPI_Finalize.
 */
int MPI_Finalize()
{
	int rank;

	// Get the MPI rank and print a tracing message with the rank
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	fprintf(stderr, "[TASK %d] Hello from tracing library %s!\n", rank, __func__);

	// Finalize MPI
	return PMPI_Finalize();
}
