#include <stdio.h>
//#include <libmpi.h>
//-x LD_PRELOAD=./libmpitrace.so
#include <mpi.h>

void bar()
{
	fprintf(stderr, "Hello from the main application!\n");
}

int main(int argc, char **argv)
{
	bar();

	printf("bar  = %lx\n", &bar);
	printf("MPI_Barrier = %lx\n", &MPI_Barrier);

	MPI_Init(&argc, &argv);

	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Finalize();

	return 0;
}
