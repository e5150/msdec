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
#include <stdio.h>

const char *
get_comp_point(double v) {
	if (v > 360.0 || v < 0.0)
		return "X";
	if (v <   5.63) return "N";
	if (v <  16.88) return "NbE";
	if (v <  28.13) return "NNE";
	if (v <  39.38) return "NEbN";
	if (v <  50.63) return "NE";
	if (v <  61.88) return "NEbE";
	if (v <  73.13) return "ENE";
	if (v <  84.38) return "EbN";
	if (v <  95.63) return "E";
	if (v < 106.88) return "EbS";
	if (v < 118.13) return "ESE";
	if (v < 129.38) return "SEbE";
	if (v < 140.63) return "SE";
	if (v < 151.88) return "SEbS";
	if (v < 163.13) return "SSE";
	if (v < 174.38) return "SbE";
	if (v < 185.63) return "S";
	if (v < 196.88) return "SbW";
	if (v < 208.13) return "SSW";
	if (v < 219.38) return "SWbS";
	if (v < 230.63) return "SW";
	if (v < 241.88) return "SWbW";
	if (v < 253.13) return "WSW";
	if (v < 264.38) return "WbS";
	if (v < 275.63) return "W";
	if (v < 286.88) return "WbN";
	if (v < 298.13) return "WNW";
	if (v < 309.38) return "NWbW";
	if (v < 320.63) return "NW";
	if (v < 331.88) return "NWbN";
	if (v < 343.13) return "NNW";
	if (v < 354.38) return "NbW";
	return "N";
}

int
fill_angle_str(char *str, size_t len, double r) {
	int deg, min;
	double sec;

	if (r < 0) {
		r = -r;
	}

	deg = (int)r;
	min = (int)((r - deg) * 60.0);
	sec = (r - deg - min / 60.0) * 3600.0;
	return snprintf(str, len - 1,
	                "%d°%d′%.2f″",
	                deg, min, sec);

}
