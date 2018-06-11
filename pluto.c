#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <ad9361.h>
#include "pluto.h"

void pluto_delete(pluto_t*p)
{
	printf("* Destroying pluto\n");
	if (p->rxbuf) { iio_buffer_destroy(p->rxbuf); }

	printf("* Disabling streaming channels\n");
	if (p->rx_i) { iio_channel_disable(p->rx_i); }
	if (p->rx_q) { iio_channel_disable(p->rx_q); }

	printf("* Destroying context\n");
	if (p->ctx) { iio_context_destroy(p->ctx); }

	free(p);
}

int pluto_set_freq(pluto_t *p, unsigned long frequency)
{
	int rc;
	struct iio_channel* lo_chn;

	lo_chn = iio_device_find_channel(
			iio_context_find_device(p->ctx, "ad9361-phy"),
			"altvoltage0", true);
	if ((rc=iio_channel_attr_write_longlong(lo_chn, "frequency", frequency)) < 0)
	{
		printf("Unable to set frequency");
		return rc;
	}
	return 0;
}

char *gain_str[] = {
	[GAIN_SLOW_ATTACK] = "slow_attack",
	[GAIN_FAST_ATTACK] = "fast_attack",
	[GAIN_HYBRID] = "hybrid",
};

int pluto_set_gain(pluto_t *p, gain_t gain)
{
	struct iio_channel* phy_chn;
	int rc;
	char *mode;
	unsigned gain_auto;

	phy_chn = iio_device_find_channel(
				iio_context_find_device(p->ctx, "ad9361-phy"),
				"voltage0", false);
	if (gain < 0)
	{
		gain_auto = -(gain+1);
		if (gain_auto >= GAIN_INVALID)
		{
			fprintf(stderr,"invalid gain %d\n", gain);
			return -1;
		}
		mode = gain_str[gain_auto];
		printf("* Setting gain_control_mode %s\n", mode); 
		if((rc = iio_channel_attr_write(phy_chn,"gain_control_mode", mode))<0)
		{
			fprintf(stderr,"unable to set gain_control_mode\n");
			return rc;
		}
	}
	else
	{
		if(gain>PLUTO_MAX_GAIN)
			gain=PLUTO_MAX_GAIN;
		if ((rc = iio_channel_attr_write(phy_chn,"gain_control_mode","manual"))<0)
		{
			fprintf(stderr, "unable to set gain control_mode");
			return rc;
		}
		if((rc=iio_channel_attr_write_longlong(phy_chn, "hardwaregain", gain))<0)
		{
			fprintf(stderr, "unable to set hardwaregain");
			return rc;
		}
	}
	return 0;
}

int pluto_set_samplerate(pluto_t *p, unsigned long samplerate)
{
	int rc;
	struct iio_channel* phy_chn;

	phy_chn = iio_device_find_channel
		(iio_context_find_device(p->ctx, "ad9361-phy"),
		"voltage0", false);
	if ((rc=iio_channel_attr_write_longlong(phy_chn, "rf_bandwidth", samplerate))<0)
	{
		fprintf(stderr, "Unable to write rf_bandwidth: %d", rc);
		return rc;
	}
	if ((rc=iio_channel_attr_write_longlong(phy_chn, "sampling_frequency", samplerate))<0)
	{
		fprintf(stderr, "Unable to write sampling_frequency: %d", rc);
		return rc;
	}
	if ((rc=ad9361_set_bb_rate(iio_context_find_device(p->ctx, "ad9361-phy"), samplerate))<0)
	{
		fprintf(stderr, "Unable to set bb rate: %d", rc);
		return rc;
	}
	return 0;
}

int pluto_set_rx(pluto_t *p)
{
	int rc;
	struct iio_channel* phy_chn;

	phy_chn = iio_device_find_channel(
				iio_context_find_device(p->ctx, "ad9361-phy"),
				"voltage0", false);

	if ((rc=iio_channel_attr_write(phy_chn, "rf_port_select", "A_BALANCED"))<0)
	{
		fprintf(stderr, "Unable to write rf_port_select: %d",rc);
		return rc;
	}
	return 0;
}


char *pluto_scan(void)
{
	struct iio_scan_context *ctx;
	struct iio_context_info **list;
	ssize_t list_size, i;
	const char *desc, *uri;
	char *retval = NULL;

	if ((ctx = iio_create_scan_context("usb", 0)))
	{
		if((list_size=iio_scan_context_get_info_list (ctx, &list)) >= 0)
		{
			for(i=0;i<list_size && !retval ;i++)
			{
				desc = iio_context_info_get_description(list[i]);
				uri = iio_context_info_get_uri(list[i]);
				if (strstr(desc, "PlutoSDR"))
				{
					printf("Using '%s' : %s\n", uri, desc);
					retval = strdup(uri);
				}
			}
			iio_context_info_list_free(list);
		}
		iio_scan_context_destroy(ctx);
	}
	return retval;
}

pluto_t* pluto_create(
	unsigned long frequency, 
	unsigned long samplerate,
	int gain,
	char *uri)
{
	int rc;
	pluto_t *p;

	if (!uri)
		uri = PLUTO_DEF_URI;

	if (!(p = (pluto_t*)malloc(sizeof(*p))))
		return NULL;
	memset(p, 0, sizeof(*p));

	/* Create context */
	printf("Creating context with uri %s\n", uri);
	if(!(p->ctx = iio_create_context_from_uri(uri)))
	{
		fprintf(stderr,"Unable to create context\n");
		goto err;
	}
	if (!iio_context_get_devices_count(p->ctx)) {
		fprintf(stderr, "No plutosdr found.\n");
		goto err;
	}
	if (!(p->dev = iio_context_find_device(p->ctx, "cf-ad9361-lpc")))
	{
		fprintf(stderr, "Error opening lpc: %s\n",
				strerror(errno));
		goto err;
	}

	if ((rc=pluto_set_rx(p)))
		goto err;
	if ((rc=pluto_set_samplerate(p, samplerate)))
		goto err;
	if ((rc=pluto_set_freq(p, frequency)))
		goto err;

	printf("* Initializing AD9361 IIO streaming channels\n");

	if (!(p->rx_i = iio_device_find_channel(p->dev, "voltage0", false)))
	{
		fprintf(stderr, "Voltage I not found");
	}
	if (!(p->rx_q = iio_device_find_channel(p->dev, "voltage1", false)))
	{
		fprintf(stderr, "Voltage Q not found");
	}

	printf("* Enabling IIO streaming channels\n");

	iio_channel_enable(p->rx_i);
	iio_channel_enable(p->rx_q);

	if ((rc=pluto_set_gain(p, gain)))
		goto err;

	printf("* Creating non-cyclic IIO buffers (%d)\n", PLUTO_DATA_LEN/2);

	p->rxbuf=iio_device_create_buffer(p->dev, PLUTO_DATA_LEN/2, false);
	p->fd = iio_buffer_get_poll_fd(p->rxbuf);

	if (!p->rxbuf) {
		perror("Could not create RX buffer");
	}
	return p;
err:
	free(p);
	return NULL;

}

void pluto_stream(pluto_t *p, pluto_data_cb_t hdlr)
{
	void *start,*end;
	int rc;

	do
	{
		//printf("refill?");
		iio_buffer_refill(p->rxbuf);
		start = iio_buffer_first(p->rxbuf, p->rx_i);
		end = iio_buffer_end(p->rxbuf);
		//printf("hdlr %d samples\n", ((unsigned)(end-start))/sizeof(sample_t));
		rc=hdlr(p, (sample_t*)start, ((unsigned)(end-start))/sizeof(sample_t));
	} while(!rc);
}
