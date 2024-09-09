#pragma once

// Defines

// Type definitions and structures

// Prototypes
int MPI_Init(int *argc, char ***argv);
int PMPI_Init(int *argc, char ***argv);

int MPI_Barrier(void *addr);

int MPI_Finalize();
int PMPI_Finalize();
