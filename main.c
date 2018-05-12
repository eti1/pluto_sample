#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include "pluto.h"

#define UNUSED  __attribute__((unused))

static volatile int capture_stop = 0;
static unsigned long long samplecount = 0;
static int out_fd;

typedef struct {
	float i;
	float q;
} x64_t;
typedef struct {
	float q;
	float i;
} c64_t;

void write_x64(int fd, sample_t *samples, unsigned count)
{
	unsigned int i;
	static x64_t buf[0x1000];

	i = 0;
	while (i<count)
	{
		buf[i&0xfff].i = (float)(samples[i].i)/2048.f;
		buf[i&0xfff].q = (float)(samples[i].q)/2048.f;
		i++;
		if (!(i&0xfff))
		{
			write(fd, buf, sizeof(buf));
		}
	}
	i&=0xfff;
	if(i)
	{
			write(fd, buf, sizeof(buf[0])*i);
	}
}

int iq_cb(pluto_t UNUSED *p, sample_t *samples, unsigned count)
{
	write_x64(out_fd, samples, count);
	samplecount += count;

	return capture_stop;
}

static struct option long_options[] = {
	{"frequency",	required_argument, 0, 'f'}, 
	{"samplerate",	required_argument, 0, 's'}, 
	{"output-file",	required_argument, 0, 'o'}, 
	{"gain",	required_argument, 0, 'g'}, 
};

void usage(char*s)
{
	unsigned i;
	printf("Usage: %s [options]\n", s);
	for(i=0;i<ARRAY_SIZE(long_options);i++)
	{
		printf("  -%c --%s\n", long_options[i].val, long_options[i].name);
	}
	exit(1);
}

void sigint(int UNUSED n)
{
	capture_stop = 1;
}

int main(int argc, char **argv)
{
	extern char *optarg;
	int option_index = 0;
	char c;
	unsigned long samplerate = 1000000, frequency = 954800000;
	unsigned long gain = 40;
	char *filename = NULL;
	pluto_t *pluto;

	while ((c = getopt_long(argc, argv, "f:s:o:g",long_options, &option_index)) != -1)
	{
		switch(c)
		{
		case 'f':
			frequency = strtoul(optarg, NULL, 10);
			break;
		case 's':
			samplerate = strtoul(optarg, NULL, 10);
			break;
		case 'o':
			filename = strdup(optarg);
			break;
		case 'g':
			gain = strtol(optarg, NULL, 10);
			break;
		case '?':
			usage(*argv);
			break;
		default:
			break;
		}
	}

	if (frequency == 0 || samplerate == 0 || filename == NULL)
	{
		printf("Missing required arguments\n");
		usage(*argv);
	}

	if((out_fd = open(filename, O_WRONLY|O_CREAT,0660))<0)
	{
		perror("open");
		return 1;
	}

	signal(2, sigint);
	pluto = pluto_create(frequency,samplerate,gain);
	pluto_stream(pluto, iq_cb);
	pluto_delete(pluto);
	close(out_fd);

	printf("written %lld samples\n", samplecount);

	return 0;
}



