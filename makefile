kilo: kilo.c
	$(CC) kilo.c -o kilo -g -Wall -Wextra -Wpedantic

clean:
	rm -rf kilo
	rm -rf kilo.dSYM
