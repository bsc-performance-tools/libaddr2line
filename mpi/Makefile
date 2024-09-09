CC = gcc
MPICC = /home/snagarka/Apps/mpich/4.2.2/bin/mpicc

LIBCFLAGS = -O0 -g -fPIC -shared -rdynamic
APPCFLAGS = -O0 -g -rdynamic

#MPI_HOME=$(MPI_ROOT)

all: libmpitrace.so libmpi.so app

libmpitrace.so: libmpitrace.c
	$(MPICC) $(LIBCFLAGS) -I/usr/lib/x86_64-linux-gnu/openmpi/include -I. $^ -o $@ -L.. -laddr2line -ldl

libmpi.so: libmpi.c
	$(CC) $(LIBCFLAGS) $^ -o $@

app: app.c
	#$(CC) $(APPCFLAGS) -I. $^ -o $@ -L. -lmpi
	$(MPICC) $(APPCFLAGS) -I. $^ -o $@

#-Wl,-rpath,.

clean:
	@rm -f libmpitrace.so libmpi.so app
