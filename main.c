#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
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

void write_i16(int fd, sample_t *samples, unsigned count)
{
	write(fd, samples, sizeof(sample_t)*count);
}

int iq_cb(pluto_t UNUSED *p, sample_t *samples, unsigned count)
{
	//write_i16(out_fd, samples, count);
	write_x64(out_fd, samples, count);
	samplecount += count;

	return capture_stop;
}

static struct option long_options[] = {
	{"frequency",	required_argument, 0, 'f'}, 
	{"samplerate",	required_argument, 0, 's'}, 
	{"output-file",	required_argument, 0, 'o'}, 
	{"gain",	required_argument, 0, 'g'}, 
	{"uri",	required_argument, 0, 'u'}, 
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
	struct timeval tv, tv2;
	double laps, rate;
	char *uri = NULL;
	pluto_t *pluto;

	while ((c = getopt_long(argc, argv, "f:s:o:g:u:",long_options, &option_index)) != -1)
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
		case 'u':
			uri = strdup(optarg);
			break;
		case '?':
			usage(*argv);
			break;
		default:
			break;
		}
	}

	if (!uri)
		uri = pluto_scan();

	if (frequency == 0 || samplerate == 0 || filename == NULL)
	{
		printf("Missing required arguments\n");
		usage(*argv);
	}

	if((out_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC,0660))<0)
	{
		perror("open");
		return 1;
	}

	signal(2, sigint);
	if(!(pluto = pluto_create(frequency,samplerate,gain,uri)))
	{
		return 1;
	}
	gettimeofday(&tv, NULL);
	pluto_stream(pluto, iq_cb);
	gettimeofday(&tv2, NULL);
	pluto_delete(pluto);
	close(out_fd);
	laps = (tv2.tv_sec*1000000+tv2.tv_usec) - (tv.tv_sec*1000000+tv.tv_usec) ;
	rate = samplecount/laps;

	printf("Written %.1fMsamples in %.1f sec (%.1fMsps = %.1fMbps)\n", samplecount/1e6, laps/1e6, rate,(rate*24));

	return 0;
}



