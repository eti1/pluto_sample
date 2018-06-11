CFLAGS?=-O3 -g -Wall -W
LDLIBS+=-liio -lad9361
PROGNAME=main

all: pluto_sample

%.o: %.c
	$(CC) $(CFLAGS) -c $<

pluto_sample: main.o pluto.o 
	$(CC) $(CFLAGS) -g -o main $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o main
