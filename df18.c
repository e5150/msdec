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
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "df18.h"
#include "es.h"
#include "tisb_c.h"
#include "tisb_f.h"
#include "mac.h"

/*  DF 18 - Extended squitter, supplementary
 *  [1] 3.1.2.8.7
 *  [3] B.3
 *
 *           1         2         3         4         5        8  11
 *  1234567890123456789012345678901234567890123456789012345...8...1
 *  DF   CF <                                                 >PI
 *          AA                      ME
 *  01234567012345670123456701234567012345670123456701234567...0...7
 *  0       1       2       3       4       5       6         11  13
 *
 */
struct ms_DF18_t *
mk_DF18(const uint8_t *msg) {
	struct ms_DF18_t *ret;
	uint32_t addr;

	ret = calloc(1, sizeof(struct ms_DF18_t));
	
	ret->DF = msg[0] >> 3;
	ret->PI = (msg[11] << 16)
	        | (msg[12] <<  8)
	        | (msg[13] <<  0);

	/*
	 * CF - Control field
	 * [1] 3.1.2.8.7.2
	 * [3] B.3.3
	 */
	ret->CF = msg[0] & 0x07;

	addr = (msg[1] << 16)
	     | (msg[2] <<  8)
	     | (msg[3] <<  0);

	ret->IMF = df18_IMF(msg);

	if (ret->IMF) {
		ret->AAu.MA.mode_a = decode_Mode_A(addr >> 12);
		ret->AAu.MA.track_file = addr & 0x000FFF;
	} else {
		ret->AAu.AA.ICAO = (ret->CF != 1);
		ret->AAu.AA.addr = addr;
	}

	ret->ES_TYPE.et = ES_RESERVED;

	switch (ret->CF) {
	case 0: /* ADS-B ES/NT */
	case 1: /* ADS-B ES/NT (non-ICAO) */
	case 6: /* ADS-B rebroadcast */
		mk_ES_TYPE(&ret->ES_TYPE, msg[4]);
		ret->aux = mk_extended_squitter(msg + 4, ret->ES_TYPE.et);
		break;
	case 2: /* Fine TIS-B */
		ret->aux = mk_TISB_fine(msg + 4);
		break;	
	case 3: /* Coarse TIS-B */
		ret->aux = mk_TISB_coarse(msg + 4);
		break;
	case 4: /* TIS-B management msg, TODO */
		break;
	case 5: /* TIS-B relay of non-ICAO ADS-B, TODO */
		break;
	case 7: /* Reserved */
		break;
	}

	return ret;
}

/*
 * DF 18 - Supplementary extended squitter
 * [1] 3.1.2.8.7
 */
void
pr_DF18(FILE *fp, struct ms_DF18_t *p, int v) {
	fprintf(fp, "DF18:Extended squitter, supplementary:TODO\n");

	if (v == 1) {
		pr_raw_header(fp);
		pr_raw(fp,  1,   5, p->DF, "DF");
		pr_raw(fp,  6,   8, p->CF, "CF");
		if (p->IMF) {
			pr_raw(fp,  9, 20, p->AAu.MA.mode_a, "AA/MA");
			pr_raw(fp, 21, 32, p->AAu.MA.track_file, "AA/TF");
		} else {
			pr_raw(fp,  9,  32, p->AAu.AA.addr, "AA");
		}
		pr_raw_subfield_header(fp, 33, 88, "ADS-B / TIS-B");
	} else {
		pr_CF(fp, p->CF);

		if (p->IMF) {
			pr_ID(fp, p->AAu.MA.mode_a);
			fprintf(fp, "Track file:%03x\n", p->AAu.MA.track_file);
		} else {
			pr_AA(fp, p->AAu.AA);
		}
	}


	switch (p->CF) {
	case 0:
	case 1:
	case 6:
		pr_extended_squitter(fp, p->aux, &p->ES_TYPE, v);
		break;
	case 2: /* Fine TIS-B */
		pr_TISB_fine(fp, p->aux, v);
		break;	
	case 3: /* Coarse TIS-B */
		pr_TISB_coarse(fp, p->aux, v);
		break;
	case 4: /* TIS-B management msg */
		fprintf(fp, "TIS-B management message:TODO\n");
		break;
	case 5: /* TIS-B relay */
		fprintf(fp, "TIS-B relay of ADS-B:TODO\n");
		break;
	case 7: /* Reserved */
		fprintf(fp, "CF=7:Reserved\n");
		break;
	}

	if (v == 1) {
		pr_raw_subfield_footer(fp);
		pr_raw(fp, 89, 112, p->PI, "PI");
		pr_raw_footer(fp);
	}

}
