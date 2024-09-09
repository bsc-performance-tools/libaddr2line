#include <stdio.h>
#include <unistd.h>

#define ELEMS 10000

extern void dashM_foo();

void do_work()
{
	int res = 0;
	for (int i = 0; i<10000; i++)
	{
		res += res+i;
		dashM_foo();
	}

	printf("Total = %d\n", res);
}

int main(int argc, char **argv)
{
	do_work();

	return 0;
}
