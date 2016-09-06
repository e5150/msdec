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

#include "bds_61.h"
#include "bds_30.h"

/*
 * BDS 61:1
 * [3] Table B-2-97a
 * (BDS 61:2 is handled as BDS 30)
 */
struct ms_BDS_61_1_t *
mk_BDS_61_1(const uint8_t *msg) {
	struct ms_BDS_61_1_t *ret;

	ret = calloc(1, sizeof(struct ms_BDS_61_1_t));

	ret->emergency_state = msg[1] >> 5;

	return ret;
}

void
pr_BDS_61_1(FILE *fp, const struct ms_BDS_61_1_t *p, int v) {
	if (v == 1) {
		pr_raw(fp, 1, 5, p->emergency_state, "E/PS");
	} else {
		fprintf(fp, "BDS:6,1:1:Aircraft status ‐ Emergency / Priority status\n");
		pr_EMERG(fp, p->emergency_state);
	}
}

void
pr_BDS_61_2(FILE *fp, const struct ms_BDS_30_t *p, int v) {
	if (v == 1) {
		pr_raw(fp, 0, 0, 0, "TODO");
	} else {
		fprintf(fp, "BDS:6,1:2:Aircraft status ‐ ACAS RA broadcast\n");
		pr_ACAS_RA(fp, p, v);
	}
}
