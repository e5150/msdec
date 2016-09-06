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

/*
 * Decoding of Mode A and Mode C
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mac.h"

static const uint32_t C1_mask = (1 << 11);
static const uint32_t A1_mask = (1 << 10);
static const uint32_t C2_mask = (1 << 9);
static const uint32_t A2_mask = (1 << 8);
static const uint32_t C4_mask = (1 << 7);
static const uint32_t A4_mask = (1 << 6);
static const uint32_t B1_mask = (1 << 5);
static const uint32_t D1_mask = (1 << 4);
static const uint32_t B2_mask = (1 << 3);
static const uint32_t D2_mask = (1 << 2);
static const uint32_t B4_mask = (1 << 1);
static const uint32_t D4_mask = (1 << 0);

static uint32_t
decode_gray(uint32_t gray) {
	uint32_t i;

	for (i = gray >> 1; i != 0; i >>= 1)
		gray ^= i;

	return gray;
}

/*
 * AC - Altitude Code
 * [1] 3.1.2.6.5.4
 *
 * C1 A1 C2 A2 C4 A4 (mb) B1 qb B2 D2 B4 D4
 * 11 10  9  8  7  6       5  4  3  2  1  0
 *
 */
int32_t
decode_AC(uint16_t raw, bool has_M) {
	bool M;
	bool Q = raw & 0x10;
	uint32_t mc;
	
	if (has_M) {
		mc = ((raw & 0x1F80) >> 1) | (raw & 0x3F);
		M = raw & 0x40;
	} else {
		mc = raw;
		M = false;
	}

	
	if (mc == 0) {
		return AC_INVALID;
	}
	if (M) {
		return AC_M_RESERVED;
	}

	if (Q) {
		/* 25 ft incr */
		uint16_t tmp = ((mc & 0xFE0) >> 1) | (mc & 0x0F);
		return 25 * tmp - 1000;
	} else {
		/* 100 ft incr */
		uint16_t n5h_gc = 0;
		int16_t n1h_c = 0;
		int n5h = 0;
		int n1h = 0;

		/*
		 * 500-increments are, from msb to lsb:
		 * D2 D4 A1 A2 A4 B1 B2 B4
		 */
		if (mc & B4_mask) n5h_gc |= 0x0001;
		if (mc & B2_mask) n5h_gc |= 0x0002;
		if (mc & B1_mask) n5h_gc |= 0x0004;
		if (mc & A4_mask) n5h_gc |= 0x0008;
		if (mc & A2_mask) n5h_gc |= 0x0010;
		if (mc & A1_mask) n5h_gc |= 0x0020;
		if (mc & D4_mask) n5h_gc |= 0x0040;
		if (mc & D2_mask) n5h_gc |= 0x0080;

		n5h = decode_gray(n5h_gc);

		/*
		 * The 100-increments are encoded in:
		 * C1 C2 C4.
		 */
		if (mc & C4_mask) n1h_c |= 0x0001;
		if (mc & C2_mask) n1h_c |= 0x0002;
		if (mc & C1_mask) n1h_c |= 0x0004;

		/*
		 * By the altitude code table in
		 * [1] Appendix to chapter 3
		 * C1 C2 C4 is not gray encoded,
		 * but gives values according to:
		 */
		switch (n1h_c) { /* C1 C2 C4 */
		case 1:
			n1h = 1; /*  0  0  1 */
			break;
		case 3:
			n1h = 2; /*  0  1  1 */
			break;
		case 2:
			n1h = 3; /*  0  1  0 */
			break;
		case 6:
			n1h = 4; /*  1  1  0 */
			break;
		case 4:
			n1h = 5; /*  1  0  0 */
			break;
		default:
			return AC_INVALID;
		}

		if (mc & B4_mask)
			n1h = 6 - n1h;

		return (500 * n5h + 100 * n1h - 1300);
	}
}

uint16_t
decode_Mode_A(uint16_t x) {
	uint16_t A, B, C, D;

	A = 0;
	if (x & A4_mask) A += 4;
	if (x & A2_mask) A += 2;
	if (x & A1_mask) A += 1;
	B = 0;
	if (x & B4_mask) B += 4;
	if (x & B2_mask) B += 2;
	if (x & B1_mask) B += 1;
	C = 0;
	if (x & C4_mask) C += 4;
	if (x & C2_mask) C += 2;
	if (x & C1_mask) C += 1;
	D = 0;
	if (x & D4_mask) D += 4;
	if (x & D2_mask) D += 2;
	if (x & D1_mask) D += 1;

	return 01000 * A + 0100 * B + 010 * C + D;
}

/*
 * ID - Identity, Mode A
 * [1] 3.1.2.6.7.1
 * 
 * Removes the X/0-bit from 13 bit Mode A codes.
 *
 * C1 A1 C2 A2 C4 A4 -- B1 D1 B2 D2 B4 D4
 * 12 11 10  9  8  7  6  5  4  3  2  1  0
 */
uint16_t
decode_ID(uint16_t ID) {
	uint16_t ma = ((ID & 0x1F80) >> 1) | (ID & 0x3F);
	return decode_Mode_A(ma);
}
