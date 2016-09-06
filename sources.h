#ifndef _MS_MAP_SOURCES_H
#define _MS_MAP_SOURCES_H

struct osm_gps_map_source_t {
	const char *name;
	const char *uri;
	const char *attrib;
	const char *format;
	const char *attrib_full1;
	const char *attrib_full2;
	int min_zoom;
	int max_zoom;
};

#define URL_CCBYSA "https://creativecommons.org/licenses/by-sa/2.0/"
#define URL_CARTO "https://carto.com"
#define URL_OSM "http://openstreetmap.com"
#define URL_ODBL "http://opendatacommons.org/licenses/odbl/"
#define URL_OPNV "http://xn--pnvkarte-m4a.de/"

static const struct osm_gps_map_source_t sources[] = {
	{
		"Carto ‐ Dark Matter",
		"http://a.basemaps.cartocdn.com/dark_all/#Z/#X/#Y.png",
		"Map tiles © CARTO. Map data © OpenStreetMap contributors",
		"png",
		"Map tiles © <a href=\"" URL_CARTO "\">CARTO</a>.",
		"Map data © <a href=\"" URL_OSM "\">OpenStreetMap</a> contributors, <a href=\"" URL_ODBL "\">ODbL</a>",
		1, 15
	},{
		"Carto ‐ Positron",
		"http://a.basemaps.cartocdn.com/light_all/#Z/#X/#Y.png",
		"Map tiles © CARTO. Map data © OpenStreetMap contributors",
		"png",
		"Map tiles © <a href=\"" URL_CARTO "\">CARTO</a>.",
		"Map data © <a href=\"" URL_OSM "\">OpenStreetMap</a> contributors, <a href=\"" URL_ODBL "\">ODbL</a>",
		1, 15
	},{
		"OpenStreetMap",
		"http://tile.openstreetmap.org/#Z/#X/#Y.png",
		"Map tiles and data © OpenStreetMap contributors",
		"png",
		"Map tiles © <a href=\"" URL_OSM "\">OpenStreetMap</a>, <a href=\"" URL_CCBYSA "\">CC BY-SA</a>",
		"Map data © <a href=\"" URL_OSM "\">OpenStreetMap</a> contributors, <a href=\"" URL_ODBL "\">ODBl</a>",
		1, 15
	},{
		"ÖPNVKARTE",
		"http://tile.xn--pnvkarte-m4a.de/tilegen/#Z/#X/#Y.png",
		"Map tiles © ÖPNKARTE.de. Map data © OpenStreetMap contributors",
		"png",
		"Map tiles © <a href=\"" URL_OPNV "\">ÖPNVKARTE.de</a>, <a href=\"" URL_CCBYSA "\">CC BY-SA</a>",
		"Map data © <a href=\"" URL_OSM "\">OpenStreetMap</a> contributors, <a href=\"" URL_ODBL "\">ODBl</a>",
		1, 15
	}
};

#endif
