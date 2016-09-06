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
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#include "cpr.h"
#include "fields.h"

/*
 * Compact position reporting
 * [3] A.2.6
 */
static uint8_t NL(double);
static int32_t mod(int32_t, int32_t);
static const uint8_t NZ = 15;

/*
 * Locally unambiguous position
 * [3] A.2.6.5
 */
int
decode_cpr_local(const struct ms_CPR_t *cpr,
                 double lat_s, double lon_s,
                 double *ret_lat, double *ret_lon,
                 bool i)
{
	double Dlat, Dlon;
	double Rlat, Rlon;
	int j, m;
	uint32_t YZ, XZ;

	YZ = cpr->lat;
	XZ = cpr->lon;


	/*
	 * a) Dlat: Latitude zone size 
	 */
	Dlat = (cpr->surface ? 90.0 : 360.0)  / (4 * NZ - i);

	/*
	 * b) j: Latitude zone index
	 */
	j = floor(lat_s / Dlat)
	  + floor( (mod(lat_s, Dlat) / Dlat) - (YZ / pow(2, cpr->Nb)) + 0.5 );

	/*
	 * c) Rlat: Decoded position latitude
	 */
	Rlat = Dlat * (j + YZ / pow(2, cpr->Nb));
	if (Rlat >= 270.0)
		Rlat -= 360.0;
	
	if (Rlat < -90.0 || Rlat > +90.0)
		return -1;

	/*
	 * d) Dlon: Longitude zone size
	 */
	if (NL(Rlat) == i)
		Dlon = (cpr->surface ? 90.0 : 360.0);
	else
		Dlon = (cpr->surface ? 90.0 : 360.0) / (NL(Rlat) - i);

	/*
	 * e) m: Longitude zone index 
	 */
	m = floor(lon_s / Dlon)
	  + floor( (mod(lon_s, Dlon) / Dlon) - (XZ / pow(2, cpr->Nb)) + 0.5);

	/*
	 * f) Rlon: Decoded position longitude
	 */
	Rlon = Dlon * (m + XZ / pow(2, cpr->Nb));
	if (Rlon >= 270.0)
		Rlon -= 360.0;
	
	/* Make caller happy */
	*ret_lat = Rlat;
	*ret_lon = Rlon;

	return 0;
}

/*
 * Globally unambiguous position
 * [3] A.2.6.7
 *
 */
int
decode_cpr_global(const struct ms_CPR_t *odd,
                  const struct ms_CPR_t *even,
                  double *ret_lat, double *ret_lon,
                  bool i)
{
	double Dlat0, Dlat1, Dlon;
	double Rlat0, Rlat1, Rlon;
	int j, m;
	int nl0, nl1, nli, ni;
	uint32_t XZi, Nb;
	uint32_t YZ0, XZ0;
	uint32_t YZ1, XZ1;

	
	if (odd->Nb != even->Nb
	|| odd->surface != even->surface) {
		/* TODO ? */
		return -1;
	}

	Nb = odd->Nb;

	YZ0 = even->lat;
	XZ0 = even->lon;
	YZ1 = odd->lat;
	XZ1 = odd->lon;

	/*
	 * a) Latitude index size
	 */
	Dlat0 = (odd->surface ? 90.0 : 360.0) / (4 * NZ - 0);
	Dlat1 = (odd->surface ? 90.0 : 360.0) / (4 * NZ - 1);
	
	/*
	 * b) Latitude index
	 */
	j = floor(0.5 + (59 * YZ0 - 60 * YZ1) / pow(2.0, Nb));

	/*
	 * c) Decoded latitude
	 */
	Rlat0 = Dlat0 * (YZ0 / pow(2.0, Nb) + mod(j, 60));
	Rlat1 = Dlat1 * (YZ1 / pow(2.0, Nb) + mod(j, 59));

	if (Rlat0 >= 270.0)
		Rlat0 -= 360.0;
	if (Rlat1 >= 270.0)
		Rlat0 -= 360.0;
	
	/*
	 * d) Messages must be from same zone
	 */
	if ((nl0 = NL(Rlat0)) != (nl1 = NL(Rlat1))) {
		return -1;
	}


	/*
	 * e) Longitude zone size based on most recent msg.
	 */
	if (i) {
		nli = nl1;
		XZi = XZ1;
	} else {
		nli = nl0;
		XZi = XZ0;
	}
	ni = nli - i < 1 ? 1 : nli - i; 

	Dlon = (odd->surface ? 90.0 : 360.0) / (ni);

	/*
	 * f) Longitude index.
	 */
	m = floor( 0.5  +  (XZ0 * (nli - 1)  -  XZ1 * nli) / pow(2, Nb) );

	/*
	 * g) Global decoded longitude.
	 */
	Rlon = Dlon * ( XZi / pow(2, Nb)  +  mod(m, ni));

	/*
	 * h) Reasonableness test… TODO :(
	 */

	*ret_lat = i ? Rlat1 : Rlat0;
	*ret_lon = Rlon;
	return 0;
	
}

static int32_t
mod(int32_t dividend, int32_t divisor) {
	int32_t rem = dividend % divisor;

	if (rem > 0)
		return rem;
	else
		return rem + divisor;
}

/*
 * NL-function
 * [3] A.2.6.2 f)
 * [3] Table C-4
 */
static uint8_t
NL(double lat) {
	if (lat < 0)
		lat = -lat;

	if (lat < 10.4704713) return 59;
	if (lat < 14.8281744) return 58;
	if (lat < 18.1862636) return 57;
	if (lat < 21.0293949) return 56;
	if (lat < 23.5450449) return 55;
	if (lat < 25.8292471) return 54;
	if (lat < 27.9389871) return 53;
	if (lat < 29.9113569) return 52;
	if (lat < 31.7720971) return 51;
	if (lat < 33.5399344) return 50;
	if (lat < 35.2289960) return 49;
	if (lat < 36.8502511) return 48;
	if (lat < 38.4124189) return 47;
	if (lat < 39.9225668) return 46;
	if (lat < 41.3865183) return 45;
	if (lat < 42.8091401) return 44;
	if (lat < 44.1945495) return 43;
	if (lat < 45.5462672) return 42;
	if (lat < 46.8673325) return 41;
	if (lat < 48.1603913) return 40;
	if (lat < 49.4277644) return 39;
	if (lat < 50.6715017) return 38;
	if (lat < 51.8934247) return 37;
	if (lat < 53.0951615) return 36;
	if (lat < 54.2781747) return 35;
	if (lat < 55.4437844) return 34;
	if (lat < 56.5931876) return 33;
	if (lat < 57.7274735) return 32;
	if (lat < 58.8476378) return 31;
	if (lat < 59.9545928) return 30;
	if (lat < 61.0491777) return 29;
	if (lat < 62.1321666) return 28;
	if (lat < 63.2042748) return 27;
	if (lat < 64.2661652) return 26;
	if (lat < 65.3184531) return 25;
	if (lat < 66.3617101) return 24;
	if (lat < 67.3964677) return 23;
	if (lat < 68.4232202) return 22;
	if (lat < 69.4424263) return 21;
	if (lat < 70.4545107) return 20;
	if (lat < 71.4598647) return 19;
	if (lat < 72.4588454) return 18;
	if (lat < 73.4517744) return 17;
	if (lat < 74.4389342) return 16;
	if (lat < 75.4205626) return 15;
	if (lat < 76.3968439) return 14;
	if (lat < 77.3678946) return 13;
	if (lat < 78.3337408) return 12;
	if (lat < 79.2942823) return 11;
	if (lat < 80.2492321) return 10;
	if (lat < 81.1980135) return 9;
	if (lat < 82.1395698) return 8;
	if (lat < 83.0719944) return 7;
	if (lat < 83.9917356) return 6;
	if (lat < 84.8916619) return 5;
	if (lat < 85.7554162) return 4;
	if (lat < 86.5353700) return 3;
	if (lat < 87.0000000) return 2;
	return 1;
}
