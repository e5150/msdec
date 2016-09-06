/*
 * Copyright © 2016 Lars Lindqvist <lars.lindqvist at yandex.ru>
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
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bds_f2.h"
#include "mac.h"
#include "fields.h"

struct ms_BDS_F2_t *
mk_BDS_F2(const uint8_t *msg) {
	struct ms_BDS_F2_t *ret;

	ret = calloc(1, sizeof(struct ms_BDS_F2_t));

	ret->TYPE = msg[0] >> 3;

	if (ret->TYPE != 1) {
		/* Unassigned */
		return ret;
	}

	ret->M1CF = msg[0] & 0x02;

	ret->modes[0].status = msg[0] & 0x04;
	ret->modes[0].raw = ((msg[0] & 0x01) << 12)
	                  |  (msg[1] << 4)
	                  |  (msg[2] >> 4);
	ret->modes[0].val = decode_ID(ret->modes[0].raw);

	ret->modes[1].status = msg[2] & 0x08;
	ret->modes[1].raw = ((msg[2] & 0x03) << 10)
	                  |  (msg[3] << 2)
	                  |  (msg[4] >> 6);
	ret->modes[1].val = decode_ID(ret->modes[1].raw);
	
	ret->modes[2].status = msg[4] & 0x20;
	ret->modes[2].raw = ((msg[4] & 0x1F) << 8)
	                   |  msg[5];
	ret->modes[2].val = decode_ID(ret->modes[2].raw);

	ret->reserved = msg[6];

	return ret;
}

void
pr_BDS_F2(FILE *fp, const struct ms_BDS_F2_t *p, int v) {
	if (v == 1) {
		pr_raw(fp, 0, 0, 0, "BDS:F,2 TODO");
	} else {
		fprintf(fp, "BDS:F,2:Military applications\n");
		fprintf(fp, "Mode1:%04d:valid:%s:cf:%d\n",
			p->modes[0].val, YESNO(p->modes[0].status), p->M1CF ? 4 : 2);
		fprintf(fp, "Mode2:%04d:valid:%s\n",
			p->modes[1].val, YESNO(p->modes[1].status));
		fprintf(fp, "ModeA:%04d:valid:%s\n",
			p->modes[2].val, YESNO(p->modes[2].status));
	}
}
