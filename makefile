CFLAGS := -g -Wall -Wextra -Wpedantic

kilo: kilo.c util.o append-buffer.o editor.o
	$(CC) append-buffer.o util.o editor.o kilo.c -o kilo $(CFLAGS)

append-buffer.o: append-buffer.c
	$(CC) -c append-buffer.c $(CFLAGS)

editor.o: editor.c
	$(CC) -c editor.c $(CFLAGS)

util.o: util.c
	$(CC) -c util.c $(CFLAGS)

clean:
	rm -rf kilo *.o
	rm -rf kilo.dSYM
