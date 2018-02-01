CFLAGS?=-O2 -g -Wall -W 
LDLIBS+=-liio -lad9361
PROGNAME=main

all: main

%.o: %.c
	$(CC) $(CFLAGS) -c $<

main: main.o pluto.o 
	$(CC) -g -o main $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o main
