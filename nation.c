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
#include "nation.h"
#include "flags.h"

/*
 * ICAO address allocation
 * [4] Table 9-1
 * ISO 3166-1 alpha-3
 */
static const struct ms_nation_t states[] = {
	{ 0x700000, 0xFFF000, "AFG", "Afghanistan", (void*)flag_AFG },
	{ 0x501000, 0xFFFC00, "ALB", "Albania", (void*)flag_ALB },
	{ 0x0A0000, 0xFF8000, "DZA", "Algeria", (void*)flag_DZA },
	{ 0x090000, 0xFFF000, "AGO", "Angola", (void*)flag_AGO },
	{ 0x0CA000, 0xFFFC00, "ATG", "Antigua and Barbuda", (void*)flag_ATG },
	{ 0xE00000, 0xFC0000, "ARG", "Argentina", (void*)flag_ARG },
	{ 0x600000, 0xFFFC00, "ARM", "Armenia", (void*)flag_ARM },
	{ 0x7C0000, 0xFC0000, "AUS", "Australia", (void*)flag_AUS },
	{ 0x440000, 0xFF8000, "AUT", "Austria", (void*)flag_AUT },
	{ 0x600800, 0xFFFC00, "AZE", "Azerbaijan", (void*)flag_AZE },
	{ 0x0A8000, 0xFFF000, "BHS", "Bahamas", (void*)flag_BHS },
	{ 0x894000, 0xFFF000, "BHR", "Bahrain", (void*)flag_BHR },
	{ 0x702000, 0xFFF000, "BGD", "Bangladesh", (void*)flag_BGD },
	{ 0x0AA000, 0xFFFC00, "BRB", "Barbados", (void*)flag_BRB },
	{ 0x510000, 0xFFFC00, "BLR", "Belarus", (void*)flag_BLR },
	{ 0x448000, 0xFF8000, "BEL", "Belgium", (void*)flag_BEL },
	{ 0x0AB000, 0xFFFC00, "BLZ", "Belize", (void*)flag_BLZ },
	{ 0x094000, 0xFFFC00, "BEN", "Benin", (void*)flag_BEN },
	{ 0x680000, 0xFFFC00, "BTN", "Bhutan", (void*)flag_BTN },
	{ 0xE94000, 0xFFF000, "BOL", "Bolivia", (void*)flag_BOL },
	{ 0x513000, 0xFFFC00, "BIH", "Bosnia and Herzegovina", (void*)flag_BIH },
	{ 0x030000, 0xFFFC00, "BWA", "Botswana", (void*)flag_BWA },
	{ 0xE40000, 0xFC0000, "BRA", "Brazil", (void*)flag_BRA },
	{ 0x895000, 0xFFFC00, "BRN", "Brunei Darussalam", (void*)flag_BRN },
	{ 0x450000, 0xFF8000, "BGR", "Bulgaria", (void*)flag_BGR },
	{ 0x09C000, 0xFFF000, "BFA", "Burkina Faso", (void*)flag_BFA },
	{ 0x032000, 0xFFF000, "BDI", "Burundi", (void*)flag_BDI },
	{ 0x70E000, 0xFFF000, "KHM", "Cambodia", (void*)flag_KHM },
	{ 0x034000, 0xFFF000, "CMR", "Cameroon", (void*)flag_CMR },
	{ 0xC00000, 0xFC0000, "CAN", "Canada", (void*)flag_CAN },
	{ 0x096000, 0xFFFC00, "CPV", "Cape Verde", (void*)flag_CPV },
	{ 0x06C000, 0xFFF000, "CAF", "Central African Republic", (void*)flag_CAF },
	{ 0x084000, 0xFFF000, "TCD", "Chad", (void*)flag_TCD },
	{ 0xE80000, 0xFFF000, "CHL", "Chile", (void*)flag_CHL },
	{ 0x780000, 0xFC0000, "CHN", "China", (void*)flag_CHN },
	{ 0x0AC000, 0xFFF000, "COL", "Colombia", (void*)flag_COL },
	{ 0x035000, 0xFFFC00, "COM", "Comoros", (void*)flag_COM },
	{ 0x036000, 0xFFF000, "COG", "Congo", (void*)flag_COG },
	{ 0x901000, 0xFFFC00, "COK", "Cook Islands", (void*)flag_COK },
	{ 0x0AE000, 0xFFF000, "CRI", "Costa Rica", (void*)flag_CRI },
	{ 0x038000, 0xFFF000, "CIV", "Côte d’Ivoire", (void*)flag_CIV },
	{ 0x501C00, 0xFFFC00, "HRV", "Croatia", (void*)flag_HRV },
	{ 0x0B0000, 0xFFF000, "CUB", "Cuba", (void*)flag_CUB },
	{ 0x4C8000, 0xFFFC00, "CYP", "Cyprus", (void*)flag_CYP },
	{ 0x498000, 0xFF8000, "CZE", "Czech Republic", (void*)flag_CZE },
	{ 0x720000, 0xFF8000, "PRK", "North Korea", (void*)flag_PRK },
	{ 0x08C000, 0xFFF000, "COD", "Congo-Kinshasa", (void*)flag_COD },
	{ 0x458000, 0xFF8000, "DNK", "Denmark", (void*)flag_DNK },
	{ 0x098000, 0xFFFC00, "DJI", "Djibouti", (void*)flag_DJI },
	{ 0x0C4000, 0xFFF000, "DOM", "Dominican Republic", (void*)flag_DOM },
	{ 0xE84000, 0xFFF000, "ECU", "Ecuador", (void*)flag_ECU },
	{ 0x010000, 0xFF8000, "EGY", "Egypt", (void*)flag_EGY },
	{ 0x0B2000, 0xFFF000, "SLV", "El Salvador", (void*)flag_SLV },
	{ 0x042000, 0xFFF000, "GNQ", "Equatorial Guinea", (void*)flag_GNQ },
	{ 0x202000, 0xFFFC00, "ERI", "Eritrea", (void*)flag_ERI },
	{ 0x511000, 0xFFFC00, "EST", "Estonia", (void*)flag_EST },
	{ 0x040000, 0xFFF000, "ETH", "Ethiopia", (void*)flag_ETH },
	{ 0xC88000, 0xFFF000, "FJI", "Fiji", (void*)flag_FJI },
	{ 0x460000, 0xFF8000, "FIN", "Finland", (void*)flag_FIN },
	{ 0x380000, 0xFC0000, "FRA", "France", (void*)flag_FRA },
	{ 0x03E000, 0xFFF000, "GAB", "Gabon", (void*)flag_GAB },
	{ 0x09A000, 0xFFF000, "GMB", "Gambia", (void*)flag_GMB },
	{ 0x514000, 0xFFFC00, "GEO", "Georgia", (void*)flag_GEO },
	{ 0x3C0000, 0xFC0000, "DEU", "Germany", (void*)flag_DEU },
	{ 0x044000, 0xFFF000, "GHA", "Ghana", (void*)flag_GHA },
	{ 0x468000, 0xFF8000, "GRC", "Greece", (void*)flag_GRC },
	{ 0x0CC000, 0xFFFC00, "GRD", "Grenada", (void*)flag_GRD },
	{ 0x0B4000, 0xFFF000, "GTM", "Guatemala", (void*)flag_GTM },
	{ 0x046000, 0xFFF000, "GIN", "Guinea", (void*)flag_GIN },
	{ 0x048000, 0xFFFC00, "GNB", "Guinea-Bissau", (void*)flag_GNB },
	{ 0x0B6000, 0xFFF000, "GUY", "Guyana", (void*)flag_GUY },
	{ 0x0B8000, 0xFFF000, "HTI", "Haiti", (void*)flag_HTI },
	{ 0x0BA000, 0xFFF000, "HND", "Honduras", (void*)flag_HND },
	{ 0x470000, 0xFF8000, "HUN", "Hungary", (void*)flag_HUN },
	{ 0x4CC000, 0xFFF000, "ISL", "Iceland", (void*)flag_ISL },
	{ 0x800000, 0xFC0000, "IND", "India", (void*)flag_IND },
	{ 0x8A0000, 0xFF8000, "IDN", "Indonesia", (void*)flag_IDN },
	{ 0x730000, 0xFF8000, "IRN", "Iran", (void*)flag_IRN },
	{ 0x728000, 0xFF8000, "IRQ", "Iraq", (void*)flag_IRQ },
	{ 0x4CA000, 0xFFF000, "IRL", "Ireland", (void*)flag_IRL },
	{ 0x738000, 0xFF8000, "ISR", "Israel", (void*)flag_ISR },
	{ 0x300000, 0xFC0000, "ITA", "Italy", (void*)flag_ITA },
	{ 0x0BE000, 0xFFF000, "JAM", "Jamaica", (void*)flag_JAM },
	{ 0x840000, 0xFC0000, "JPN", "Japan", (void*)flag_JPN },
	{ 0x740000, 0xFF8000, "JOR", "Jordan", (void*)flag_JOR },
	{ 0x683000, 0xFFFC00, "KAZ", "Kazakhstan", (void*)flag_KAZ },
	{ 0x04C000, 0xFFF000, "KEN", "Kenya", (void*)flag_KEN },
	{ 0xC8E000, 0xFFFC00, "KIR", "Kiribati", (void*)flag_KIR },
	{ 0x706000, 0xFFF000, "KWT", "Kuwait", (void*)flag_KWT },
	{ 0x601000, 0xFFFC00, "KGZ", "Kyrgyzstan", (void*)flag_KGZ },
	{ 0x708000, 0xFFF000, "LAO", "Laos", (void*)flag_LAO },
	{ 0x502C00, 0xFFFC00, "LVA", "Latvia", (void*)flag_LVA },
	{ 0x748000, 0xFF8000, "LBN", "Lebanon", (void*)flag_LBN },
	{ 0x04A000, 0xFFFC00, "LSO", "Lesotho", (void*)flag_LSO },
	{ 0x050000, 0xFFF000, "LBR", "Liberia", (void*)flag_LBR },
	{ 0x018000, 0xFF8000, "LBY", "Libya", (void*)flag_LBY },
	{ 0x503C00, 0xFFFC00, "LTU", "Lithuania", (void*)flag_LTU },
	{ 0x4D0000, 0xFFFC00, "LUX", "Luxembourg", (void*)flag_LUX },
	{ 0x054000, 0xFFF000, "MDG", "Madagascar", (void*)flag_MDG },
	{ 0x058000, 0xFFF000, "MWI", "Malawi", (void*)flag_MWI },
	{ 0x750000, 0xFF8000, "MYS", "Malaysia", (void*)flag_MYS },
	{ 0x05A000, 0xFFFC00, "MDV", "Maldives", (void*)flag_MDV },
	{ 0x05C000, 0xFFF000, "MLI", "Mali", (void*)flag_MLI },
	{ 0x4D2000, 0xFFFC00, "MLT", "Malta", (void*)flag_MLT },
	{ 0x900000, 0xFFFC00, "MHL", "Marshall Islands", (void*)flag_MHL },
	{ 0x05E000, 0xFFFC00, "MRT", "Mauritania", (void*)flag_MRT },
	{ 0x060000, 0xFFFC00, "MUS", "Mauritius", (void*)flag_MUS },
	{ 0x0D0000, 0xFF8000, "MEX", "Mexico", (void*)flag_MEX },
	{ 0x681000, 0xFFFC00, "FSM", "Micronesia", (void*)flag_FSM },
	{ 0x4D4000, 0xFFFC00, "MCO", "Monaco", (void*)flag_MCO },
	{ 0x682000, 0xFFFC00, "MNG", "Mongolia", (void*)flag_MNG },
	{ 0x516000, 0xFFFC00, "MNE", "Montenegro", (void*)flag_MNE },
	{ 0x020000, 0xFF8000, "MAR", "Morocco", (void*)flag_MAR },
	{ 0x006000, 0xFFF000, "MOZ", "Mozambique", (void*)flag_MOZ },
	{ 0x704000, 0xFFF000, "MMR", "Myanmar", (void*)flag_MMR },
	{ 0x201000, 0xFFFC00, "NAM", "Namibia", (void*)flag_NAM },
	{ 0xC8A000, 0xFFFC00, "NRU", "Nauru", (void*)flag_NRU },
	{ 0x70A000, 0xFFF000, "NPL", "Nepal", (void*)flag_NPL },
	{ 0x480000, 0xFF8000, "NLD", "Netherlands", (void*)flag_NLD },
	{ 0xC80000, 0xFF8000, "NZL", "New Zealand", (void*)flag_NZL },
	{ 0x0C0000, 0xFFF000, "NIC", "Nicaragua", (void*)flag_NIC },
	{ 0x062000, 0xFFF000, "NER", "Niger", (void*)flag_NER },
	{ 0x064000, 0xFFF000, "NGA", "Nigeria", (void*)flag_NGA },
	{ 0x478000, 0xFF8000, "NOR", "Norway", (void*)flag_NOR },
	{ 0x70C000, 0xFFFC00, "OMN", "Oman", (void*)flag_OMN },
	{ 0x760000, 0xFF8000, "PAK", "Pakistan", (void*)flag_PAK },
	{ 0x684000, 0xFFFC00, "PLW", "Palau", (void*)flag_PLW },
	{ 0x0C2000, 0xFFF000, "PAN", "Panama", (void*)flag_PAN },
	{ 0x898000, 0xFFF000, "PNG", "Papua New Guinea", (void*)flag_PNG },
	{ 0xE88000, 0xFFF000, "PRY", "Paraguay", (void*)flag_PRY },
	{ 0xE8C000, 0xFFF000, "PER", "Peru", (void*)flag_PER },
	{ 0x758000, 0xFF8000, "PHL", "Philippines", (void*)flag_PHL },
	{ 0x488000, 0xFF8000, "POL", "Poland", (void*)flag_POL },
	{ 0x490000, 0xFF8000, "PRT", "Portugal", (void*)flag_PRT },
	{ 0x06A000, 0xFFFC00, "QAT", "Qatar", (void*)flag_QAT },
	{ 0x718000, 0xFF8000, "KOR", "South Korea", (void*)flag_KOR },
	{ 0x504C00, 0xFFFC00, "MDA", "Moldova", (void*)flag_MDA },
	{ 0x4A0000, 0xFF8000, "ROU", "Romania", (void*)flag_ROU },
	{ 0x100000, 0xF00000, "RUS", "Russian Federation", (void*)flag_RUS },
	{ 0x06E000, 0xFFF000, "RWA", "Rwanda", (void*)flag_RWA },
	{ 0xC8C000, 0xFFFC00, "LCA", "Saint Lucia", (void*)flag_LCA },
	{ 0x0BC000, 0xFFFC00, "VCT", "Saint Vincent and the Grenadines", (void*)flag_VCT },
	{ 0x902000, 0xFFFC00, "WSM", "Samoa", (void*)flag_WSM },
	{ 0x500000, 0xFFFC00, "SMR", "San Marino", (void*)flag_SMR },
	{ 0x09E000, 0xFFFC00, "STP", "Sao Tome and Principe", (void*)flag_STP },
	{ 0x710000, 0xFF8000, "SAU", "Saudi Arabia", (void*)flag_SAU },
	{ 0x070000, 0xFFF000, "SEN", "Senegal", (void*)flag_SEN },
	{ 0x074000, 0xFFFC00, "SYC", "Seychelles", (void*)flag_SYC },
	{ 0x076000, 0xFFFC00, "SLE", "Sierra Leone", (void*)flag_SLE },
	{ 0x768000, 0xFF8000, "SGP", "Singapore", (void*)flag_SGP },
	{ 0x505C00, 0xFFFC00, "SVK", "Slovakia", (void*)flag_SVK },
	{ 0x506C00, 0xFFFC00, "SVN", "Slovenia", (void*)flag_SVN },
	{ 0x897000, 0xFFFC00, "SLB", "Solomon Islands", (void*)flag_SLB },
	{ 0x078000, 0xFFF000, "SOM", "Somalia", (void*)flag_SOM },
	{ 0x008000, 0xFF8000, "ZAF", "South Africa", (void*)flag_ZAF },
	{ 0x340000, 0xFC0000, "ESP", "Spain", (void*)flag_ESP },
	{ 0x770000, 0xFF8000, "LKA", "Sri Lanka", (void*)flag_LKA },
	{ 0x07C000, 0xFFF000, "SDN", "Sudan", (void*)flag_SDN },
	{ 0x0C8000, 0xFFF000, "SUR", "Suriname", (void*)flag_SUR },
	{ 0x07A000, 0xFFFC00, "SWZ", "Swaziland", (void*)flag_SWZ },
	{ 0x4A8000, 0xFF8000, "SWE", "Sweden", (void*)flag_SWE },
	{ 0x4B0000, 0xFF8000, "CHE", "Switzerland", (void*)flag_CHE },
	{ 0x778000, 0xFF8000, "SYR", "Syria", (void*)flag_SYR },
	{ 0x515000, 0xFFFC00, "TJK", "Tajikistan", (void*)flag_TJK },
	{ 0x880000, 0xFF8000, "THA", "Thailand", (void*)flag_THA },
	{ 0x512000, 0xFFFC00, "MKD", "FYR Macedonia", (void*)flag_MKD },
	{ 0x088000, 0xFFF000, "TGO", "Togo", (void*)flag_TGO },
	{ 0xC8D000, 0xFFFC00, "TON", "Tonga", (void*)flag_TON },
	{ 0x0C6000, 0xFFF000, "TTO", "Trinidad and Tobago", (void*)flag_TTO },
	{ 0x028000, 0xFF8000, "TUN", "Tunisia", (void*)flag_TUN },
	{ 0x4B8000, 0xFF8000, "TUR", "Turkey", (void*)flag_TUR },
	{ 0x601800, 0xFFFC00, "TKM", "Turkmenistan", (void*)flag_TKM },
	{ 0x068000, 0xFFF000, "UGA", "Uganda", (void*)flag_UGA },
	{ 0x508000, 0xFF8000, "UKR", "Ukraine", (void*)flag_UKR },
	{ 0x896000, 0xFFF000, "ARE", "United Arab Emirates", (void*)flag_ARE },
	{ 0x400000, 0xFC0000, "GBR", "United Kingdom", (void*)flag_GBR },
	{ 0x080000, 0xFFF000, "TZA", "Tanzania", (void*)flag_TZA },
	{ 0xA00000, 0xF00000, "USA", "United States of America", (void*)flag_USA },
	{ 0xE90000, 0xFFF000, "URY", "Uruguay", (void*)flag_URY },
	{ 0x507C00, 0xFFFC00, "UZB", "Uzbekistan", (void*)flag_UZB },
	{ 0xC90000, 0xFFFC00, "VUT", "Vanuatu", (void*)flag_VUT },
	{ 0x0D8000, 0xFF8000, "VEN", "Venezuela", (void*)flag_VEN },
	{ 0x888000, 0xFF8000, "VNM", "Viet Nam", (void*)flag_VNM },
	{ 0x890000, 0xFFF000, "YEM", "Yemen", (void*)flag_YEM },
	{ 0x4C0000, 0xFF8000, "SRB", "Serbia", (void*)flag_SRB },
	{ 0x08A000, 0xFFF000, "ZMB", "Zambia", (void*)flag_ZMB },
	{ 0x004000, 0xFFFC00, "ZWE", "Zimbabwe", (void*)flag_ZWE },

	{ 0xF00000, 0xFF8000, "tmp", "ICAO ‐ Temporary", (void*)flag_unk },
	{ 0x899000, 0xFFFC00, "iss", "ICAO ‐ Special / Safety", (void*)flag_unk },
	{ 0xF09000, 0xFFFC00, "iss", "ICAO ‐ Special / Safety", (void*)flag_unk },
	/*
	 * [4] 4.3
	 */
	{ 0xF80000, 0x200000, "afi", "AFI region", (void*)flag_unk },
	{ 0xF80000, 0x210000, "sam", "SAM region", (void*)flag_unk },
	{ 0xF00000, 0x500000, "eur", "EUR / NAT region", (void*)flag_unk },
	{ 0xF80000, 0x600000, "mid", "MID region", (void*)flag_unk },
	{ 0xF80000, 0x680000, "asi", "ASIA region", (void*)flag_unk },
	{ 0xF00000, 0x900000, "nam", "NAM / PAC region", (void*)flag_unk },
	{ 0xFC0000, 0xEC0000, "car", "CAR region", (void*)flag_unk },

	{ 0x000000, 0x000000, "unk", "Unknown state", (void*)flag_unk },
};

const struct ms_nation_t *
icao_addr_to_nation(uint32_t addr) {
	uint32_t len, i;

	len = sizeof(states) / sizeof(states[0]);

	for (i = 0; i < len; ++i) {
		if ((addr & states[i].mask) == states[i].code) {
			return states + i;
		}
	}
	/*
	 * NOTREACHED
	 * Last state will match everything
	 */
	return states + len - 1;
}

const char *
icao_addr_to_state(uint32_t addr) {
	uint32_t i;

	for (i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
		if ((addr & states[i].mask) == states[i].code) {
			return states[i].name;
		}
	}
	return 0;
}

const char *
icao_addr_to_iso3(uint32_t addr) {
	uint32_t i;

	for (i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
		if ((addr & states[i].mask) == states[i].code) {
			return states[i].iso3;
		}
	}
	return 0; 

}
