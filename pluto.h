#ifndef DEF_PLUTO_H
#define DEF_PLUTO_H
#include <iio.h>
#include <stdint.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))
#endif

typedef int gain_t;

enum {
	GAIN_FAST_ATTACK,
	GAIN_SLOW_ATTACK,
	GAIN_HYBRID,
	GAIN_INVALID
};
#define DEF_GAIN(g) (-((g)+1))

typedef struct {
	struct iio_context *ctx;
	struct iio_device *dev;
	struct iio_channel *rx_i;
	struct iio_channel *rx_q;
	struct iio_buffer  *rxbuf;
	int fd;
} pluto_t;

typedef struct {
	int16_t i;
	int16_t q;
} sample_t;

typedef int (*pluto_data_cb_t)(pluto_t *p, sample_t *samples, unsigned count);

//#define PLUTO_DATA_LEN             (16*16384)   /* 256k */
#define PLUTO_DATA_LEN             (64*16384)   /* 256k */
#define PLUTO_MAX_GAIN				70

#define PLUTO_DEF_URI "ip:pluto.local"

pluto_t* pluto_create(
	unsigned long frequency,
	unsigned long samplerate,
	gain_t gain,
	char *uri
);
void pluto_delete(pluto_t*p);
int pluto_set_freq(pluto_t *p, unsigned long frequency);

void pluto_stream(pluto_t *p, pluto_data_cb_t hdlr);
char *pluto_scan(void);

#endif
