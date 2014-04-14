CC=gcc
CFLAGS="-Wall"

debug:clean
	$(CC) $(CFLAGS) -g -o swarm main.c -lSDL -lm
stable:clean
	$(CC) $(CFLAGS) -o swarm main.c -lSDL -lm
clean:
	rm -vfr *~ swarm
