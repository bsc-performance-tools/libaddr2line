#include <stdio.h>

# define weak_alias(name, aliasname) _weak_alias (name, aliasname)
# define _weak_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)));

int PMPI_Init(int *argc, char ***argv) {
    
    fprintf(stderr, "Hello from real MPI library!\n");
    return 0;
}

weak_alias(PMPI_Init, MPI_Init);

int PMPI_Finalize(){

    fprintf(stderr, "Hello from real MPI Finalize!\n");
    return 0;
}

weak_alias(PMPI_Finalize, MPI_Finalize);
