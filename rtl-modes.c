/*
 * rtl-modes - a Mode S message extractor for RTL-SDR devices.
 * Based on dump1090, originally written by Salvatore Sanfilippo.
 *
 * Copyright © 2016 Lars Lindqvist <lars.lindqvist at yandex.ru>
 * Copyright © 2014-2016 Oliver Jowett <oliver@mutability.co.uk>
 *  
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *
 *    Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
 *
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are
 *    met:
 *
 *     *  Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *     *  Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <rtl-sdr.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#include <arg.h>

#include "crc.h"
#include "util.h"
#include "es.h"

#define CONF_RTL_MODES
#include "config.h"

#define ICAO_CACHE_LEN           256
#define ICAO_CACHE_TTL           60

#define MODES_RTL_BUFFERS        15 /*  Number of RTL buffers */
#define MODES_RTL_BUF_SIZE      (16*16384) /*  256k */
#define MODES_MAG_BUF_SAMPLES   (MODES_RTL_BUF_SIZE / 2) /*  Each sample is 2 bytes */
#define MODES_MAG_BUFFERS        12 /*  Number of magnitude buffers (should be smaller than RTL_BUFFERS for flowcontrol to work) */

#define MODES_PREAMBLE_US        8 /*  microseconds = bits */
#define MODES_PREAMBLE_SAMPLES  (MODES_PREAMBLE_US       * 2)
#define MODES_PREAMBLE_SIZE     (MODES_PREAMBLE_SAMPLES  * sizeof(uint16_t))
#define MODES_LONG_MSG_BYTES     14
#define MODES_SHORT_MSG_BYTES    7
#define MODES_LONG_MSG_BITS     (MODES_LONG_MSG_BYTES    * 8)
#define MODES_SHORT_MSG_BITS    (MODES_SHORT_MSG_BYTES   * 8)
#define MODES_LONG_MSG_SAMPLES  (MODES_LONG_MSG_BITS     * 2)
#define MODES_SHORT_MSG_SAMPLES (MODES_SHORT_MSG_BITS    * 2)
#define MODES_LONG_MSG_SIZE     (MODES_LONG_MSG_SAMPLES  * sizeof(uint16_t))
#define MODES_SHORT_MSG_SIZE    (MODES_SHORT_MSG_SAMPLES * sizeof(uint16_t))

#define RTL_SAMPLE_RATE         2400000.0
#define RTL_FREQUENCY           1090000000
#define RTL_PPM_ERROR           52

static struct {
	FILE *fp;
	const char *filename;
	gid_t gid;
} mslog, msout;

static struct {
	uid_t uid;
	gid_t gid;
	bool daemonize;
} msd;

struct mag_buf {
	uint16_t *data;		/*  Magnitude data. Starts with Modes.trailing_samples worth of overlap from the previous block */
	unsigned length;	/*  Number of valid samples _after_ overlap. Total buffer length is buf->length + Modes.trailing_samples. */
	uint32_t dropped;	/*  Number of dropped samples preceding this buffer */
};

static struct {			/*  Internal state */
	pthread_t reader_thread;

	pthread_mutex_t data_mutex;	/*  Mutex to synchronize buffer access */
	pthread_cond_t data_cond;	/*  Conditional variable associated */

	struct mag_buf mag_buffers[MODES_MAG_BUFFERS];	/*  Converted magnitude buffers from RTL or file input */
	unsigned first_free_buffer;	/*  Entry in mag_buffers that will next be filled with input. */
	unsigned first_filled_buffer;	/*  Entry in mag_buffers that has valid data and will be demodulated next. If equal to next_free_buffer, there is no unprocessed data. */

	unsigned trailing_samples;	/*  extra trailing samples in magnitude buffers */

	int fd;			/*  --ifile option file descriptor */
	uint16_t *maglut;	/*  I/Q -> Magnitude lookup table */
	int exit;		/*  Exit from the main loop when true */

	/*  RTLSDR */
	int dev_index;
	int gain;
	int enable_agc;
	rtlsdr_dev_t *dev;

	/*  Configuration */
	int nfix_crc;		/*  Number of crc bit error(s) to correct */
	int check_crc;		/*  Only display messages with good CRC */
} Modes;

static struct {
	struct {
		time_t seen;
		uint32_t addr;
	} items[ICAO_CACHE_LEN];
	size_t i;
} icao_cache;

/*
 * Returns the time elapsed, in nanoseconds, from t1 to t2,
 * where t1 and t2 are 12MHz counters.
 */
static void normalize_timespec(struct timespec *ts);
static int scoreModesMessage(const uint8_t *msg, size_t valid_len);
static ssize_t decode_message(const uint8_t *msg);
static void demodulate2400(struct mag_buf *mag);

static void
icao_cache_add(uint32_t addr) {
	size_t i;

	for (i = 0; i < ICAO_CACHE_LEN; ++i) {
		if (icao_cache.items[icao_cache.i].addr == addr) {
			icao_cache.items[icao_cache.i].seen = time(NULL);
			return;
		}
	}

	icao_cache.items[icao_cache.i].addr = addr;
	icao_cache.items[icao_cache.i].seen = time(NULL);
	if (++icao_cache.i == ICAO_CACHE_LEN)
		icao_cache.i = 0;
}

static uint32_t
icao_cache_addr(uint32_t addr, uint32_t mask) {
	size_t i;

	for (i = 0; i < ICAO_CACHE_LEN; ++i) {
		if ((icao_cache.items[i].addr & mask) == (addr & mask)) {
			return icao_cache.items[i].addr;
		}
	}
	
	return addr;
}

static bool
icao_cache_seen(uint32_t addr) {
	time_t now = time(NULL);
	size_t i;

	for (i = 0; i < ICAO_CACHE_LEN; ++i) {
		uint32_t ca = icao_cache.items[i].addr;
		if (!ca)
			return false;
		if (ca == addr) {
			return now - icao_cache.items[i].seen <= ICAO_CACHE_TTL;
		}

	}
	return false;
}

int
fprintftime(FILE *fp, const char *str, ...) {
	va_list ap;
	char timestr[20];
	time_t t;
	struct tm *tmp;
	int ret = 0;

	t = time(NULL);
	if ((tmp = localtime(&t)) && strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", tmp)) {
		ret += fprintf(fp, "%s ", timestr);
	} else {
		ret += fprintf(fp, "--time-error-- ");
	}

	va_start(ap, str);
	ret += vfprintf(fp, str, ap);
	va_end(ap);

	return ret;
}

/*
 * 2.4MHz sampling rate version
 *
 * When sampling at 2.4MHz we have exactly 6 samples per 5 symbols.
 * Each symbol is 500ns wide, each sample is 416.7ns wide
 *
 * We maintain a phase offset that is expressed in units of 1/5 of a sample i.e. 1/6 of a symbol, 83.333ns
 * Each symbol we process advances the phase offset by 6 i.e. 6/5 of a sample, 500ns
 *
 * The correlation functions below correlate a 1-0 pair of symbols (i.e. manchester encoded 1 bit)
 * starting at the given sample, and assuming that the symbol starts at a fixed 0-5 phase offset within
 * m[0]. They return a correlation value, generally interpreted as >0 = 1 bit, <0 = 0 bit
 *
 * TODO check if there are better (or more balanced) correlation functions to use here
 *
 * nb: the correlation functions sum to zero, so we do not need to adjust for the DC offset in the input signal
 * (adding any constant value to all of m[0..3] does not change the result)
 */
static int
slice_phase0(uint16_t * m) {
	return 5 * m[0] - 3 * m[1] - 2 * m[2];
}

static int
slice_phase1(uint16_t * m) {
	return 4 * m[0] - m[1] - 3 * m[2];
}

static int
slice_phase2(uint16_t * m) {
	return 3 * m[0] + m[1] - 4 * m[2];
}

static int
slice_phase3(uint16_t * m) {
	return 2 * m[0] + 3 * m[1] - 5 * m[2];
}

static int
slice_phase4(uint16_t * m) {
	return m[0] + 5 * m[1] - 5 * m[2] - m[3];
}

static void
demodulate2400(struct mag_buf *mag) {
	uint8_t msg1[MODES_LONG_MSG_BYTES], msg2[MODES_LONG_MSG_BYTES], *msg;
	uint32_t j;

	uint8_t *bestmsg;
	int bestscore;

	uint16_t *m = mag->data;
	uint32_t mlen = mag->length;

	msg = msg1;

	for (j = 0; j < mlen; j++) {
		uint16_t *preamble = &m[j];
		int high;
		uint32_t base_signal, base_noise;
		int try_phase;
		ssize_t msglen;

		/*
		 * Look for a message starting at around sample 0 with phase offset 3..7

		 * Ideal sample values for preambles with different phase
		 * Xn is the first data symbol with phase offset N
		 *
		 * sample#: 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
		 * phase 3: 2/4\0/5\1 0 0 0 0/5\1/3 3\0 0 0 0 0 0 X4
		 * phase 4: 1/5\0/4\2 0 0 0 0/4\2 2/4\0 0 0 0 0 0 0 X0
		 * phase 5: 0/5\1/3 3\0 0 0 0/3 3\1/5\0 0 0 0 0 0 0 X1
		 * phase 6: 0/4\2 2/4\0 0 0 0 2/4\0/5\1 0 0 0 0 0 0 X2
		 * phase 7: 0/3 3\1/5\0 0 0 0 1/5\0/4\2 0 0 0 0 0 0 X3
		 */

		/* quick check: we must have a rising edge 0->1 and a falling edge 12->13 */
		if (!(preamble[0] < preamble[1] && preamble[12] > preamble[13]))
			continue;

		if (preamble[1] > preamble[2] &&
		    preamble[2] < preamble[3] && preamble[3] > preamble[4] &&
		    preamble[8] < preamble[9] && preamble[9] > preamble[10] &&
		    preamble[10] < preamble[11]) {
			high = (preamble[1] + preamble[3] + preamble[9] + preamble[11] + preamble[12]) / 4;
			base_signal = preamble[1] + preamble[3] + preamble[9];
			base_noise = preamble[5] + preamble[6] + preamble[7];
		} else if (preamble[1] > preamble[2] &&
			   preamble[2] < preamble[3] && preamble[3] > preamble[4] &&
			   preamble[8] < preamble[9] && preamble[9] > preamble[10] &&
			   preamble[11] < preamble[12]) {
			high = (preamble[1] + preamble[3] + preamble[9] + preamble[12]) / 4;
			base_signal = preamble[1] + preamble[3] + preamble[9] + preamble[12];
			base_noise = preamble[5] + preamble[6] + preamble[7] + preamble[8];
		} else if (preamble[1] > preamble[2] &&
			   preamble[2] < preamble[3] && preamble[4] > preamble[5] &&
			   preamble[8] < preamble[9] && preamble[10] > preamble[11] &&
			   preamble[11] < preamble[12]) {
			high =
			 (preamble[1] + preamble[3] + preamble[4] + preamble[9] + preamble[10] + preamble[12]) / 4;
			base_signal = preamble[1] + preamble[12];
			base_noise = preamble[6] + preamble[7];
		} else if (preamble[1] > preamble[2] &&
			   preamble[3] < preamble[4] && preamble[4] > preamble[5] &&
			   preamble[9] < preamble[10] && preamble[10] > preamble[11] &&
			   preamble[11] < preamble[12]) {
			high = (preamble[1] + preamble[4] + preamble[10] + preamble[12]) / 4;
			base_signal = preamble[1] + preamble[4] + preamble[10] + preamble[12];
			base_noise = preamble[5] + preamble[6] + preamble[7] + preamble[8];
		} else if (preamble[2] > preamble[3] &&
			   preamble[3] < preamble[4] && preamble[4] > preamble[5] &&
			   preamble[9] < preamble[10] && preamble[10] > preamble[11] &&
			   preamble[11] < preamble[12]) {
			high = (preamble[1] + preamble[2] + preamble[4] + preamble[10] + preamble[12]) / 4;
			base_signal = preamble[4] + preamble[10] + preamble[12];
			base_noise = preamble[6] + preamble[7] + preamble[8];
		} else {
			/* no suitable peaks */
			continue;
		}

		/* Check for enough signal, about 3.5dB SNR */
		if (base_signal * 2 < 3 * base_noise)
			continue;

		/* Check that the "quiet" bits 6,7,15,16,17 are actually quiet */
		if (preamble[5] >= high ||
		    preamble[6] >= high ||
		    preamble[7] >= high ||
		    preamble[8] >= high ||
		    preamble[14] >= high ||
		    preamble[15] >= high || preamble[16] >= high || preamble[17] >= high || preamble[18] >= high) {
			continue;
		}

		/*
		 * Try all phases
		 */
		bestmsg = NULL;
		bestscore = -2;
		for (try_phase = 4; try_phase <= 8; ++try_phase) {
			uint16_t *pPtr;
			int phase, i, score, bytelen;

			/*
			 * Decode all the next 112 bits, regardless of the actual message
			 * size. We'll check the actual message type later
			 */

			pPtr = &m[j + 19] + (try_phase / 5);
			phase = try_phase % 5;

			bytelen = MODES_LONG_MSG_BYTES;
			for (i = 0; i < bytelen; ++i) {
				uint8_t theByte = 0;

				switch (phase) {
				case 0:
					theByte =
					 (slice_phase0(pPtr) > 0 ? 0x80 : 0) |
					 (slice_phase2(pPtr + 2) > 0 ? 0x40 : 0) |
					 (slice_phase4(pPtr + 4) > 0 ? 0x20 : 0) |
					 (slice_phase1(pPtr + 7) > 0 ? 0x10 : 0) |
					 (slice_phase3(pPtr + 9) > 0 ? 0x08 : 0) |
					 (slice_phase0(pPtr + 12) > 0 ? 0x04 : 0) |
					 (slice_phase2(pPtr + 14) > 0 ? 0x02 : 0) |
					 (slice_phase4(pPtr + 16) > 0 ? 0x01 : 0);

					phase = 1;
					pPtr += 19;
					break;

				case 1:
					theByte =
					 (slice_phase1(pPtr) > 0 ? 0x80 : 0) |
					 (slice_phase3(pPtr + 2) > 0 ? 0x40 : 0) |
					 (slice_phase0(pPtr + 5) > 0 ? 0x20 : 0) |
					 (slice_phase2(pPtr + 7) > 0 ? 0x10 : 0) |
					 (slice_phase4(pPtr + 9) > 0 ? 0x08 : 0) |
					 (slice_phase1(pPtr + 12) > 0 ? 0x04 : 0) |
					 (slice_phase3(pPtr + 14) > 0 ? 0x02 : 0) |
					 (slice_phase0(pPtr + 17) > 0 ? 0x01 : 0);

					phase = 2;
					pPtr += 19;
					break;

				case 2:
					theByte =
					 (slice_phase2(pPtr) > 0 ? 0x80 : 0) |
					 (slice_phase4(pPtr + 2) > 0 ? 0x40 : 0) |
					 (slice_phase1(pPtr + 5) > 0 ? 0x20 : 0) |
					 (slice_phase3(pPtr + 7) > 0 ? 0x10 : 0) |
					 (slice_phase0(pPtr + 10) > 0 ? 0x08 : 0) |
					 (slice_phase2(pPtr + 12) > 0 ? 0x04 : 0) |
					 (slice_phase4(pPtr + 14) > 0 ? 0x02 : 0) |
					 (slice_phase1(pPtr + 17) > 0 ? 0x01 : 0);

					phase = 3;
					pPtr += 19;
					break;

				case 3:
					theByte =
					 (slice_phase3(pPtr) > 0 ? 0x80 : 0) |
					 (slice_phase0(pPtr + 3) > 0 ? 0x40 : 0) |
					 (slice_phase2(pPtr + 5) > 0 ? 0x20 : 0) |
					 (slice_phase4(pPtr + 7) > 0 ? 0x10 : 0) |
					 (slice_phase1(pPtr + 10) > 0 ? 0x08 : 0) |
					 (slice_phase3(pPtr + 12) > 0 ? 0x04 : 0) |
					 (slice_phase0(pPtr + 15) > 0 ? 0x02 : 0) |
					 (slice_phase2(pPtr + 17) > 0 ? 0x01 : 0);

					phase = 4;
					pPtr += 19;
					break;

				case 4:
					theByte =
					 (slice_phase4(pPtr) > 0 ? 0x80 : 0) |
					 (slice_phase1(pPtr + 3) > 0 ? 0x40 : 0) |
					 (slice_phase3(pPtr + 5) > 0 ? 0x20 : 0) |
					 (slice_phase0(pPtr + 8) > 0 ? 0x10 : 0) |
					 (slice_phase2(pPtr + 10) > 0 ? 0x08 : 0) |
					 (slice_phase4(pPtr + 12) > 0 ? 0x04 : 0) |
					 (slice_phase1(pPtr + 15) > 0 ? 0x02 : 0) |
					 (slice_phase3(pPtr + 17) > 0 ? 0x01 : 0);

					phase = 0;
					pPtr += 20;
					break;
				}

				msg[i] = theByte;
				if (i == 0) {
					switch (get_msgtype(msg[0])) {
					case 0:
					case 4:
					case 5:
					case 11:
						bytelen = MODES_SHORT_MSG_BYTES;
						break;

					case 16:
					case 17:
					case 18:
					case 20:
					case 21:
					case 24:
						break;

					default:
						bytelen = 1;	/* unknown DF, give up immediately */
						break;
					}
				}
			}

			/*
			 * Score the mode S message and see if it's any good.
			 */
			score = scoreModesMessage(msg, i);
			if (score > bestscore) {
				bestmsg = msg;
				bestscore = score;
				/*
				 * Swap to using the other buffer so we don't clobber our demodulated data
				 * (if we find a better result then we'll swap back, but that's OK because
				 * we no longer need this copy if we found a better one)
				 */
				msg = (msg == msg1) ? msg2 : msg1;
			}
		}

		/* Do we have a candidate? */
		if (bestscore < 0) {
			continue;
		}

		msglen = decode_message(bestmsg);
		if (msglen <= 0) {
			continue;
		}

		/*
		 * Skip over the message:
		 * (we actually skip to 8 bits before the end of the message,
		 *  because we can often decode two messages that *almost* collide,
		 *  where the preamble of the second message clobbered the last
		 *  few bits of the first message, but the message bits didn't
		 *  overlap)
		 */
		j += msglen * 12 / 5;

	}

}

static void
signal_handler(int sig) {
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		signal(sig, SIG_DFL);
		Modes.exit = 1;
		break;
	case SIGHUP:
		fprintftime(mslog.fp, "Reopening files\n");
		fclose(mslog.fp);
		fclose(msout.fp);
		if (!(mslog.fp = fopen(mslog.filename, "a")))
			exit(1);
		if (!(msout.fp = fopen(msout.filename, "a")))
			exit(1);
		setbuf(mslog.fp, NULL);
		fprintftime(mslog.fp, "Reopened files\n");
		break;
	default:
		signal(sig, SIG_DFL);
		break;
	}
}

static int
modesInit(void) {
	int i, q;

	pthread_mutex_init(&Modes.data_mutex, NULL);
	pthread_cond_init(&Modes.data_cond, NULL);

	Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * RTL_SAMPLE_RATE;

	if (((Modes.maglut = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256)) == NULL)) {
		fprintftime(mslog.fp, "FATAL: malloc: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < MODES_MAG_BUFFERS; ++i) {
		if ((Modes.mag_buffers[i].data =
		     calloc(MODES_MAG_BUF_SAMPLES + Modes.trailing_samples, sizeof(uint16_t))) == NULL) {
			fprintftime(mslog.fp, "FATAL: calloc: %s\n", strerror(errno));
			return -1;
		}

		Modes.mag_buffers[i].length = 0;
		Modes.mag_buffers[i].dropped = 0;
	}

	for (i = 0; i <= 255; i++) {
		for (q = 0; q <= 255; q++) {
			float fI, fQ, magsq;

			fI = (i - 127.5) / 127.5;
			fQ = (q - 127.5) / 127.5;
			magsq = fI * fI + fQ * fQ;
			if (magsq > 1)
				magsq = 1;

			Modes.maglut[le16toh((i * 256) + q)] = (uint16_t) round(sqrtf(magsq) * 65535.0);
		}
	}
	return 0;
}

static void
convert_samples(void *iq_data, uint16_t *mag_data, unsigned nsamples) {
	uint16_t *in = iq_data;
	unsigned i;
	uint16_t mag;

	for (i = 0; i < (nsamples >> 3); ++i) {
		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;

		mag = Modes.maglut[*in++];
		*mag_data++ = mag;
	}

	for (i = 0; i < (nsamples & 7); ++i) {
		mag = Modes.maglut[*in++];
		*mag_data++ = mag;
	}
}


static int
modesInitRTLSDR(void) {
	char vendor[256], product[256], serial[256];
	int *gains;
	int numgains;
	int device_count;
	int highest = -1;
	int i;

	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintftime(mslog.fp, "No supported RTLSDR devices found.\n");
		return -1;
	}

	for (i = 0; i < device_count; i++) {
		if (rtlsdr_get_device_usb_strings(i, vendor, product, serial) != 0) {
			fprintftime(mslog.fp, "%d: unable to read device details\n", i);
		} else {
			fprintftime(mslog.fp, "%d: %s, %s, SN: %s %s\n", i, vendor, product, serial,
				(i == Modes.dev_index) ? "(currently selected)" : "");
		}
	}

	if (rtlsdr_open(&Modes.dev, Modes.dev_index) < 0) {
		fprintftime(mslog.fp, "Error opening the RTLSDR device: %s\n", strerror(errno));
		return -1;
	}
	/* Set gain, frequency, sample rate, and reset the device */
	rtlsdr_set_tuner_gain_mode(Modes.dev, 1);

	numgains = rtlsdr_get_tuner_gains(Modes.dev, NULL);
	if (numgains <= 0) {
		fprintftime(mslog.fp, "Error getting tuner gains\n");
		return -1;
	}

	gains = malloc(numgains * sizeof(int));
	if (rtlsdr_get_tuner_gains(Modes.dev, gains) != numgains) {
		fprintftime(mslog.fp, "Error getting tuner gains\n");
		free(gains);
		return -1;
	}

	for (i = 0; i < numgains; ++i) {
		if (gains[i] > highest)
			highest = gains[i];
	}

	fprintftime(mslog.fp, "Max available gain is: %.2f dB\n", highest / 10.0);

	free(gains);

	if (rtlsdr_set_tuner_gain(Modes.dev, highest) < 0) {
		fprintftime(mslog.fp, "Error setting tuner gains\n");
		return -1;
	}
	rtlsdr_set_freq_correction(Modes.dev, RTL_PPM_ERROR);
	if (Modes.enable_agc)
		rtlsdr_set_agc_mode(Modes.dev, 1);
	rtlsdr_set_center_freq(Modes.dev, RTL_FREQUENCY);
	rtlsdr_set_sample_rate(Modes.dev, (unsigned)RTL_SAMPLE_RATE);

	rtlsdr_reset_buffer(Modes.dev);
	fprintftime(mslog.fp, "Gain reported by device: %.2f dB\n", rtlsdr_get_tuner_gain(Modes.dev) / 10.0);

	return 0;
}

/*
 * We use a thread reading data in background, while the main thread
 * handles decoding and visualization of data to the user.
 *
 * The reading thread calls the RTLSDR API to read data asynchronously, and
 * uses a callback to populate the data buffer.
 *
 * A Mutex is used to avoid races with the decoding thread.
 *
 */
static void
rtlsdrCallback(uint8_t *buf, uint32_t len, void *ctx) {
	struct mag_buf *outbuf;
	struct mag_buf *lastbuf;
	uint32_t slen;
	unsigned next_free_buffer;
	unsigned free_bufs;

	static int was_odd = 0;
	static int dropping = 0;

	(void)(ctx);

	/*  Lock the data buffer variables before accessing them */
	pthread_mutex_lock(&Modes.data_mutex);
	if (Modes.exit) {
		rtlsdr_cancel_async(Modes.dev);
	}

	next_free_buffer = (Modes.first_free_buffer + 1) % MODES_MAG_BUFFERS;
	outbuf = &Modes.mag_buffers[Modes.first_free_buffer];
	lastbuf = &Modes.mag_buffers[(Modes.first_free_buffer + MODES_MAG_BUFFERS - 1) % MODES_MAG_BUFFERS];
	free_bufs = (Modes.first_filled_buffer - next_free_buffer + MODES_MAG_BUFFERS) % MODES_MAG_BUFFERS;

	/*  Paranoia! Unlikely, but let's go for belt and suspenders here */

	if (len != MODES_RTL_BUF_SIZE) {
		fprintftime(mslog.fp,
			"weirdness: rtlsdr gave us a block with an unusual size (got %u bytes, expected %u bytes)\n",
			(unsigned)len, (unsigned)MODES_RTL_BUF_SIZE);

		if (len > MODES_RTL_BUF_SIZE) {
			/*  wat?! Discard the start. */
			unsigned discard = (len - MODES_RTL_BUF_SIZE + 1) / 2;
			outbuf->dropped += discard;
			buf += discard * 2;
			len -= discard * 2;
		}
	}

	if (was_odd) {
		/*  Drop a sample so we are in sync with I/Q samples again (hopefully) */
		++buf;
		--len;
		++outbuf->dropped;
	}

	was_odd = (len & 1);
	slen = len / 2;

	if (free_bufs == 0 || (dropping && free_bufs < MODES_MAG_BUFFERS / 2)) {
		/*  FIFO is full. Drop this block. */
		dropping = 1;
		outbuf->dropped += slen;
		pthread_mutex_unlock(&Modes.data_mutex);
		return;
	}

	dropping = 0;
	pthread_mutex_unlock(&Modes.data_mutex);

	/*  Copy trailing data from last block (or reset if not valid) */
	if (outbuf->dropped == 0 && lastbuf->length >= Modes.trailing_samples) {
		memcpy(outbuf->data, lastbuf->data + lastbuf->length - Modes.trailing_samples,
		       Modes.trailing_samples * sizeof(uint16_t));
	} else {
		memset(outbuf->data, 0, Modes.trailing_samples * sizeof(uint16_t));
	}

	/*  Convert the new data */
	outbuf->length = slen;
	convert_samples(buf, &outbuf->data[Modes.trailing_samples], slen);

	/*  Push the new data to the demodulation thread */
	pthread_mutex_lock(&Modes.data_mutex);

	Modes.mag_buffers[next_free_buffer].dropped = 0;
	Modes.mag_buffers[next_free_buffer].length = 0;	/* just in case */
	Modes.first_free_buffer = next_free_buffer;

	pthread_cond_signal(&Modes.data_cond);
	pthread_mutex_unlock(&Modes.data_mutex);
}

/*
 * We read data using a thread, so the main thread only handles decoding
 * without caring about data acquisition
 */
static void *
readerThreadEntryPoint(void *arg) {
	(void)(arg);

	if (Modes.fd < 0) {
		while (!Modes.exit) {
			rtlsdr_read_async(Modes.dev, rtlsdrCallback, NULL, MODES_RTL_BUFFERS, MODES_RTL_BUF_SIZE);

			if (!Modes.exit) {
				fprintftime(mslog.fp, "Warning: lost the connection to the RTLSDR device.\n");
				rtlsdr_close(Modes.dev);
				Modes.dev = NULL;

				do {
					sleep(5);
				} while (!Modes.exit && modesInitRTLSDR() < 0);
			}
		}

		if (Modes.dev != NULL) {
			rtlsdr_close(Modes.dev);
			Modes.dev = NULL;
		}
	}

	/*  Wake the main thread (if it's still waiting) */
	pthread_mutex_lock(&Modes.data_mutex);
	Modes.exit = 1;		/*  just in case */
	pthread_cond_signal(&Modes.data_cond);
	pthread_mutex_unlock(&Modes.data_mutex);

	pthread_exit(NULL);
}

/*
 * Score how plausible this ModeS message looks.
 * The more positive, the more reliable the message is

 * 1000: DF 0/4/5/16/24 with a CRC-derived address matching a known aircraft

 * 1800: DF17/18 with good CRC and an address matching a known aircraft
 * 1400: DF17/18 with good CRC and an address not matching a known aircraft
 *  900: DF17/18 with 1-bit error and an address matching a known aircraft
 *  700: DF17/18 with 1-bit error and an address not matching a known aircraft
 *  450: DF17/18 with 2-bit error and an address matching a known aircraft
 *  350: DF17/18 with 2-bit error and an address not matching a known aircraft

 * 1600: DF11 with IID==0, good CRC and an address matching a known aircraft
 *  800: DF11 with IID==0, 1-bit error and an address matching a known aircraft
 *  750: DF11 with IID==0, good CRC and an address not matching a known aircraft
 *  375: DF11 with IID==0, 1-bit error and an address not matching a known aircraft

 * 1000: DF11 with IID!=0, good CRC and an address matching a known aircraft
 *  500: DF11 with IID!=0, 1-bit error and an address matching a known aircraft

 * 1000: DF20/21 with a CRC-derived address matching a known aircraft
 *  500: DF20/21 with a CRC-derived address matching a known aircraft (bottom 16 bits only - overlay control in use)

 *   -1: message might be valid, but we couldn't validate the CRC against a known ICAO
 *   -2: bad message or unrepairable CRC error
 */
static int
scoreModesMessage(const uint8_t *msg, size_t valid_len) {
	uint8_t msgtype;
	uint32_t addr;
	uint32_t syn;
	size_t len;
	bool errs;
	struct {
		int old;
		int new;
	} scores = { -1, -1 };


	msgtype = get_msgtype(msg[0]);
	len = df_to_len(msgtype);

	if (valid_len < len)
		return -2;

	syn = crc_syndrome(msg, len);

	switch (msgtype) {
	case 0:
	case 4:
	case 5:
	case 16:
	case 24:
		return icao_cache_seen(syn) ? 1000 : -1;
	case 20:
	case 21:
		return icao_cache_seen(syn) ? 1000 : -2;
		/*
		if (icao_cache_seen(syn)) {
			return 1000;
		} else {
			addr = icao_cache_addr(syn, 0x00FFFF);
			return icao_cache_seen(addr) ? 800 : -1;
		}
		*/
	case 18:
		if (df18_IMF(msg)) {
			return icao_cache_seen(syn) ? 1000 : -1;
		}
		/* FALLTHROUGH */
	case 17:
		scores.old = 1800;
		scores.new = 1400;
		break;
	case 19:
		scores.old = 200;
		scores.new =  50;
		break;
	case 11:
		if (syn & 0x7F) {
			scores.old = 1000;
			scores.new = -100;
			syn &= 0xffff80;
		} else {
			scores.old = 1600;
			scores.new =  750;
		}
		break;
	default:
		return -2;
	}

	errs = syn != 0x000000;
	addr = (msg[1] << 16) | (msg[2] << 8) | (msg[3]);

	if (errs) {
		int eb = errorbit(len, syn);
		if (eb == -1)
			return -2;
		if (7 < eb && eb < 32) {
			addr ^= (1 << (31 - eb));
		}
	}
	if (icao_cache_seen(addr))
		return scores.old / (errs ? 2 : 1);
	else
		return scores.new / (errs ? 2 : 1);
}

/*
 * return length of message, in bits, if all OK
 *   -1: message might be valid, but we couldn't validate the CRC against a known ICAO
 *   -2: bad message or unrepairable CRC error
 */
static ssize_t
decode_message(const uint8_t *orig_msg) {
	uint8_t msg[MODES_LONG_MSG_BYTES];
	uint8_t msgtype;
	uint32_t addr;
	uint32_t syn;
	size_t len;
	size_t i;
	bool msg_has_addr;


	msgtype = get_msgtype(orig_msg[0]);
	len = df_to_len(msgtype);

	/*  Work on our local copy. */
	memcpy(msg, orig_msg, len);

	syn = crc_syndrome(msg, len);

	/*  Do checksum work and set fields that depend on the CRC */
	switch (msgtype) {
	case 0:
	case 4:
	case 5:
	case 16:
	case 24:
	case 20:
	case 21:
/*
		if (!icao_cache_seen(syn))
			syn = icao_cache_addr(syn, 0x00FFFF);
*/
		msg_has_addr = false;
		break;
	case 11:
		syn &= 0xFFFF80;
		msg_has_addr = true;
		break;
	case 17:
		msg_has_addr = true;
		break;
	case 18:
		msg_has_addr = !df18_IMF(msg);
		break;
	case 19:
		/* TODO */
		msg_has_addr = false;
		break;
	default:
		return -2;
	}


	if (msg_has_addr) {
		if (syn == 0x000000) {
			addr = (msg[1] << 16) | (msg[2] << 8) | (msg[3]);
			icao_cache_add(addr);
		} else {
			int eb = errorbit(len, syn);
			
			if (eb < 5) {
				return -2;
			}

			msg[eb / 8] ^= (1 << (7 - (eb % 8)));
			addr = (msg[1] << 16) | (msg[2] << 8) | (msg[3]);
		}
	} else {
		addr = syn;
	}

	if (!icao_cache_seen(addr)) {
		return -1;
	}

	fprintf(msout.fp, "DF%02d:%ld:", msgtype, time(NULL));
	for (i = 0; i < len; ++i) {
		fprintf(msout.fp, "%02X", msg[i]);
	}
	fprintf(msout.fp, ":%06X\n", addr);
	fflush(msout.fp);
	
	return len * 8;
}

/*
 * Normalize the value in ts so that ts->nsec lies in
 * [0,999999999]
 */
static void
normalize_timespec(struct timespec *ts) {
	if (ts->tv_nsec > 1000000000) {
		ts->tv_sec += ts->tv_nsec / 1000000000;
		ts->tv_nsec = ts->tv_nsec % 1000000000;
	} else if (ts->tv_nsec < 0) {
		long adjust = ts->tv_nsec / 1000000000 + 1;
		ts->tv_sec -= adjust;
		ts->tv_nsec = (ts->tv_nsec + 1000000000 * adjust) % 1000000000;
	}
}

static int
daemonize() {
	pid_t pid;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "%s: FATAL: fork: %s\n", argv0, strerror(errno));
		return -1;
	}
	if (pid > 0) {
		exit(0);
	}
	if (setsid() < 0) {
		fprintf(stderr, "%s: FATAL: setsid: %s\n", argv0, strerror(errno));
		return -1;
	}

	if (!(mslog.fp = fopen(mslog.filename, "a"))) {
		fprintf(stderr, "%s: FATAL: fopen %s: %s\n", argv0, mslog.filename, strerror(errno));
		return -1;
	}
	if (!(msout.fp = fopen(msout.filename, "a"))) {
		fprintftime(mslog.fp, "FATAL: fopen %s: %s\n", msout.filename, strerror(errno));
		return -1;
	}
	if (chdir("/") < 0) {
		fprintftime(mslog.fp, "FATAL: chdir /: %s\n", strerror(errno));
		return -1;
	}
	if (chown(mslog.filename, msd.uid, mslog.gid) == -1) {
		fprintftime(mslog.fp, "FATAL: chown %d:%d %s: %s\n",
		            msd.uid, mslog.gid, mslog.filename, strerror(errno));
		return -1;
	}
	if (chown(msout.filename, msd.uid, msout.gid) == -1) {
		fprintftime(mslog.fp, "FATAL: chown %d:%d %s: %s\n",
		            msd.uid, msout.gid, msout.filename, strerror(errno));
		return -1;
	}

	setbuf(mslog.fp, NULL);

	if (setgid(msd.gid) < 0) {
		fprintftime(mslog.fp, "FATAL: setgid %d: %s\n", msd.gid, strerror(errno));
		return -1;
	}
	if (setuid(msd.uid) < 0) {
		fprintftime(mslog.fp, "FATAL: setuid %d: %s\n", msd.uid, strerror(errno));
		return -1;
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	fprintftime(mslog.fp, "Daemon started.\n");

	return 0;
}

static void
usage() {
	printf("usage: %s [-D device index]\n", argv0);
	printf("usage: %s -d [-f logfile] [-o outfile] [-u uid] [-g gid] [-D device index]\n", argv0);
	exit(1);
}

int
main(int argc, char **argv) {
	memset(&Modes, 0, sizeof(Modes));
	memset(&icao_cache, 0, sizeof(icao_cache));
	Modes.check_crc = 1;
	Modes.fd = -1;

	memset(&msd,   0, sizeof(msd));
	memset(&mslog, 0, sizeof(mslog));
	memset(&msout, 0, sizeof(msout));

	msd.uid = default_uid;
	msd.gid = default_daemon_gid;

	mslog.filename = default_logfile;
	mslog.gid = default_log_gid;
	msout.filename = default_outfile;
	msout.gid = default_out_gid;

	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);

	ARGBEGIN {
	case 'f':
		mslog.filename = EARGF(usage());
		break;
	case 'o':
		msout.filename = EARGF(usage());
		break;
	case 'd':
		msd.daemonize = true;
		break;
	case 'D':
		Modes.dev_index = atoi(EARGF(usage()));
		break;
	case 'g':
		msd.gid = atoi(EARGF(usage()));
		break;
	case 'u':
		msd.uid = atoi(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND;

	if (msd.daemonize) {
		signal(SIGHUP, signal_handler);
		if (daemonize() < 0) {
			goto failed;
		}
	} else {
		mslog.fp = stderr;
		msout.fp = stdout;
	}

	if (modesInit() < 0) {
		goto failed;
	}

	if (modesInitRTLSDR() < 0) {
		goto failed;
	}

	int watchdogCounter = 10;	/*  about 1 second */

	/*  Create the thread that will read the data from the device. */
	pthread_mutex_lock(&Modes.data_mutex);
	pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

	while (Modes.exit == 0) {

		if (Modes.first_free_buffer == Modes.first_filled_buffer) {
			/*
			 * Wait for more data.
			 * We should be getting data every 50-60ms.
			 * Wait for max 100ms before we give up
			 * and do some background work.
			 */

			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += 100000000;
			normalize_timespec(&ts);

			/* This unlocks Modes.data_mutex, and waits for Modes.data_cond */
			pthread_cond_timedwait(&Modes.data_cond, &Modes.data_mutex, &ts);
		}
		/* Modes.data_mutex is locked, and possibly we have data. */

		if (Modes.first_free_buffer != Modes.first_filled_buffer) {
			/* FIFO is not empty, process one buffer. */

			struct mag_buf *buf;

			buf = &Modes.mag_buffers[Modes.first_filled_buffer];
			
			/*
			 * Process data after releasing the lock, so that the capturing
			 * thread can read data while we perform computationally expensive
			 * stuff at the same time.
			 */
			pthread_mutex_unlock(&Modes.data_mutex);

			demodulate2400(buf);

			/* Mark the buffer we just processed as completed. */
			pthread_mutex_lock(&Modes.data_mutex);
			Modes.first_filled_buffer = (Modes.first_filled_buffer + 1) % MODES_MAG_BUFFERS;
			pthread_cond_signal(&Modes.data_cond);
			pthread_mutex_unlock(&Modes.data_mutex);
			watchdogCounter = 10;
		} else {
			/* Nothing to process this time around. */
			pthread_mutex_unlock(&Modes.data_mutex);
			if (--watchdogCounter <= 0) {
				fprintftime(mslog.fp, "No data received from the dongle"
				                    "for a long time, it may have wedged.\n");
				watchdogCounter = 600;
			}
		}
		pthread_mutex_lock(&Modes.data_mutex);
	}

	pthread_mutex_unlock(&Modes.data_mutex);

	fprintftime(mslog.fp, "Waiting for receive thread termination\n"); 
	pthread_join(Modes.reader_thread, NULL);
	pthread_cond_destroy(&Modes.data_cond);
	pthread_mutex_destroy(&Modes.data_mutex);

	fprintftime(mslog.fp, "Normal exit.\n");

	pthread_exit(0);

failed:
	if (mslog.fp) {
		fprintftime(mslog.fp, "Abnormal exit.\n");
		fclose(mslog.fp);
	}
	if (msout.fp) {
		fclose(msout.fp);
	}
	return 1;
}
