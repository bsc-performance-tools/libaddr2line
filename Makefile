CFLAGS=-g -O0

libaddr2line.so: addr2line.c
	gcc -shared -fPIC $(CFLAGS) $^ -o $@

libfoo.so: foo.c
	gcc -shared -fPIC $(CFLAGS) $^ -o $@

app: app.c libfoo.so
	gcc $(CFLAGS) $^ -o $@ -L. -lfoo -Wl,-rpath=. -lfoo

all: libaddr2line.so libfoo.so app

clean:
	@rm -f app libfoo.so libaddr2line.so
