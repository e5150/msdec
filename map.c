/*
 * Map widget for msdec, based on the osm-gps-map widget.
 *
 * Copyright © Lars Lindqvist 2016 <lars.lindqvist@yandex.ru>
 * Copyright © Martijn Goedhart 2014 <goedhart.martijn@gmail.com>
 * Copyright © John Stowers 2009,2013 <john.stowers@gmail.com>
 * Copyright © Till Harbaum 2009 <till@harbaum.org>
 * Copyright © Marcus Bauer 2008 <marcus.bauer@gmail.com>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
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
 */

#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <gdk/gdk.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <libsoup/soup.h>

#include "map.h"
#include "sources.h"

#define ENABLE_DEBUG                (0)
#define EXTRA_BORDER                (0)
#define OSM_GPS_MAP_SCROLL_STEP     (10)
#define USER_AGENT                  "msdec/1.0"
#define DOWNLOAD_RETRIES            3
#define MAX_DOWNLOAD_TILES          256
#define DOT_RADIUS                  4.0

#define TILESIZE 256
#define MAX_ZOOM 20
#define MIN_ZOOM 0

#define MAX_TILE_ZOOM_OFFSET 10
#define MIN_TILE_ZOOM_OFFSET 0

#define URI_MARKER_X    "#X"
#define URI_MARKER_Y    "#Y"
#define URI_MARKER_Z    "#Z"
#define URI_MARKER_S    "#S"
#define URI_MARKER_Q    "#Q"
#define URI_MARKER_Q0   "#W"
#define URI_MARKER_YS   "#U"
#define URI_MARKER_R    "#R"

#define URI_HAS_X   (1 << 0)
#define URI_HAS_Y   (1 << 1)
#define URI_HAS_Z   (1 << 2)
#define URI_HAS_S   (1 << 3)
#define URI_HAS_Q   (1 << 4)
#define URI_HAS_Q0  (1 << 5)
#define URI_HAS_YS  (1 << 6)
#define URI_HAS_R   (1 << 7)

#define URI_FLAG_END (1 << 8)

/* equatorial radius in meters */
#define OSM_EQ_RADIUS   (6378137.0)


struct _OsmGpsMapPrivate {
	GHashTable *tile_queue;
	GHashTable *missing_tiles;
	GHashTable *tile_cache;

	int map_zoom;
	int max_zoom;
	int min_zoom;

	int tile_zoom_offset;

	int map_x;
	int map_y;

	/* Controls auto centering the map when a new GPS position arrives */
	gdouble map_auto_center_threshold;

	/* Latitude and longitude of the center of the map, in radians */
	gdouble center_rlat;
	gdouble center_rlon;

	gdouble home_rlat;
	gdouble home_rlon;

	guint max_tile_cache_size;
	/* Incremented at each redraw */
	guint redraw_cycle;
	/* ID of the idle redraw operation */
	guint idle_map_redraw;

	/* how we download tiles */
	SoupSession *soup_session;
	char *proxy_uri;

	/* where downloaded tiles are cached */
	char *tile_dir;
	char *tile_base_dir;
	char *cache_dir;

	int map_source;
	char *repo_uri;
	const char *repo_attrib;
	char *image_format;
	int uri_format;

	GSList *tracks;

	/* Used for storing the joined tiles */
	cairo_surface_t *pixmap;

	/* The tile painted when one cannot be found */
	GdkPixbuf *null_tile;

	/* For tracking click and drag */
	int drag_counter;
	int drag_mouse_dx;
	int drag_mouse_dy;
	int drag_start_mouse_x;
	int drag_start_mouse_y;
	int drag_start_map_x;
	int drag_start_map_y;
	int drag_limit;
	guint drag_expose_source;

	/* Properties for dragging a point with right mouse button. */
	OsmGpsMapPoint *drag_point;
	OsmGpsMapTrack *drag_track;

	/* for customizing the redering of the gps track */
	int ui_gps_point_inner_radius;
	int ui_gps_point_outer_radius;

	/* For storing keybindings */
	guint keybindings[OSM_GPS_MAP_KEY_MAX];

	/* flags controlling which features are enabled */
#if 0
	guint keybindings_enabled:1;
#endif
	guint map_auto_download_enabled:1;
	guint map_auto_center_enabled:1;
	guint trip_history_record_enabled:1;
	guint trip_history_show_enabled:1;
	guint show_home:1;

	/* state flags */
	guint is_disposed:1;
	guint is_constructed:1;
	guint is_dragging:1;
	guint is_button_down:1;
	guint is_fullscreen:1;
	guint is_dragging_point:1;
};

typedef struct {
	GdkPixbuf *pixbuf;
	/* We keep track of the number of the redraw cycle this tile was last used,
	 * so that osm_gps_map_purge_cache() can remove the older ones */
	guint redraw_cycle;
} OsmCachedTile;

typedef struct {
	/* The details of the tile to download */
	char *uri;
	char *folder;
	char *filename;
	OsmGpsMap *map;
	/* whether to redraw the map when the tile arrives */
	gboolean redraw;
	int ttl;
} OsmTileDownload;

enum {
	PROP_MAP_0,
	PROP_AUTO_CENTER,
	PROP_RECORD_TRIP_HISTORY,
	PROP_SHOW_TRIP_HISTORY,
	PROP_AUTO_DOWNLOAD,
	PROP_REPO_URI,
	PROP_PROXY_URI,
	PROP_TILE_CACHE_BASE_DIR,
	PROP_TILE_ZOOM_OFFSET,
	PROP_ZOOM,
	PROP_MAX_ZOOM,
	PROP_MIN_ZOOM,
	PROP_HOME_LAT,
	PROP_HOME_LON,
	PROP_LATITUDE,
	PROP_LONGITUDE,
	PROP_MAP_X,
	PROP_MAP_Y,
	PROP_TILES_QUEUED,
	PROP_MAP_SOURCE,
	PROP_IMAGE_FORMAT,
	PROP_DRAG_LIMIT,
	PROP_AUTO_CENTER_THRESHOLD,
	PROP_SHOW_HOME
};

G_DEFINE_TYPE(OsmGpsMap, osm_gps_map, GTK_TYPE_DRAWING_AREA);


GType
osm_gps_map_point_get_type(void) {
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static(g_intern_static_string("OsmGpsMapPoint"),
							(GBoxedCopyFunc) osm_gps_map_point_copy,
							(GBoxedFreeFunc) osm_gps_map_point_free);
	return our_type;
}

OsmGpsMapPoint *
osm_gps_map_point_new_degrees(double lat, double lon) {
	OsmGpsMapPoint *p = g_new0(OsmGpsMapPoint, 1);
	p->rlat = deg2rad(lat);
	p->rlon = deg2rad(lon);
	return p;
}

OsmGpsMapPoint *
osm_gps_map_point_new_radians(double rlat, double rlon) {
	OsmGpsMapPoint *p = g_new0(OsmGpsMapPoint, 1);
	p->rlat = rlat;
	p->rlon = rlon;
	return p;
}

void
osm_gps_map_point_get_degrees(OsmGpsMapPoint * point, double *lat, double *lon) {
	*lat = rad2deg(point->rlat);
	*lon = rad2deg(point->rlon);
}

void
osm_gps_map_point_get_radians(OsmGpsMapPoint * point, double *rlat, double *rlon) {
	*rlat = point->rlat;
	*rlon = point->rlon;
}

void
osm_gps_map_point_set_degrees(OsmGpsMapPoint * point, double lat, double lon) {
	point->rlat = deg2rad(lat);
	point->rlon = deg2rad(lon);
}

void
osm_gps_map_point_set_radians(OsmGpsMapPoint * point, double rlat, double rlon) {
	point->rlat = rlat;
	point->rlon = rlon;
}

OsmGpsMapPoint *
osm_gps_map_point_copy(const OsmGpsMapPoint * point) {
	OsmGpsMapPoint *result = g_new(OsmGpsMapPoint, 1);
	*result = *point;

	return result;
}

void
osm_gps_map_point_free(OsmGpsMapPoint * point) {
	g_free(point);
}

static gchar *replace_string(const gchar * src, const gchar * from, const gchar * to);
static gchar *replace_map_uri(OsmGpsMap * map, const gchar * uri, int zoom, int x, int y);
static void osm_gps_map_tile_download_complete(SoupSession * session, SoupMessage * msg, gpointer user_data);
static void osm_gps_map_download_tile(OsmGpsMap * map, int zoom, int x, int y, gboolean redraw);
static GdkPixbuf *osm_gps_map_render_tile_upscaled(OsmGpsMap * map, GdkPixbuf * tile, int tile_zoom, int zoom, int x, int y);

G_DEFINE_TYPE(OsmGpsMapTrack, osm_gps_map_track, G_TYPE_OBJECT)

enum {
	PROP_TRACK_0,
	PROP_VISIBLE,
	PROP_TRACK,
	PROP_LINE_WIDTH,
	PROP_CONTINUOUS,
	PROP_ALPHA,
	PROP_COLOR,
	PROP_HEADING
};

enum {
	POINT_ADDED,
	POINT_CHANGED,
	POINT_INSERTED,
	POINT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

struct _OsmGpsMapTrackPrivate {
	GSList *track;
	gboolean visible;
	gdouble linewidth;
	gdouble alpha;
	GdkColor color;
	gboolean continuous;
	gdouble heading;
};

#define DEFAULT_R   (0.6)
#define DEFAULT_G   (0)
#define DEFAULT_B   (0)
#define DEFAULT_A   (0.8)

static void
osm_gps_map_track_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	OsmGpsMapTrackPrivate *priv = OSM_GPS_MAP_TRACK(object)->priv;

	switch (property_id) {
	case PROP_VISIBLE:
		g_value_set_boolean(value, priv->visible);
		break;
	case PROP_CONTINUOUS:
		g_value_set_boolean(value, priv->continuous);
		break;
	case PROP_TRACK:
		g_value_set_pointer(value, priv->track);
		break;
	case PROP_HEADING:
		g_value_set_double(value, priv->heading);
		break;
	case PROP_LINE_WIDTH:
		g_value_set_double(value, priv->linewidth);
		break;
	case PROP_ALPHA:
		g_value_set_double(value, priv->alpha);
		break;
	case PROP_COLOR:
		g_value_set_boxed(value, &priv->color);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
osm_gps_map_track_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	OsmGpsMapTrackPrivate *priv = OSM_GPS_MAP_TRACK(object)->priv;

	switch (property_id) {
	case PROP_VISIBLE:
		priv->visible = g_value_get_boolean(value);
		break;
	case PROP_CONTINUOUS:
		priv->continuous = g_value_get_boolean(value);
		break;
	case PROP_TRACK:
		priv->track = g_value_get_pointer(value);
		break;
	case PROP_HEADING:
		priv->heading = g_value_get_double(value);
		break;
	case PROP_LINE_WIDTH:
		priv->linewidth = g_value_get_double(value);
		break;
	case PROP_ALPHA:
		priv->alpha = g_value_get_double(value);
		break;
	case PROP_COLOR:{
			GdkColor *c = g_value_get_boxed(value);
			priv->color.red = c->red;
			priv->color.green = c->green;
			priv->color.blue = c->blue;
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void
osm_gps_map_track_dispose(GObject * object) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(object));
	OsmGpsMapTrackPrivate *priv = OSM_GPS_MAP_TRACK(object)->priv;

	if (priv->track) {
		g_slist_foreach(priv->track, (GFunc) g_free, NULL);
		g_slist_free(priv->track);
		priv->track = NULL;
	}

	G_OBJECT_CLASS(osm_gps_map_track_parent_class)->dispose(object);
}

static void
osm_gps_map_track_finalize(GObject * object) {
	G_OBJECT_CLASS(osm_gps_map_track_parent_class)->finalize(object);
}

static void
osm_gps_map_track_class_init(OsmGpsMapTrackClass * klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(OsmGpsMapTrackPrivate));

	object_class->get_property = osm_gps_map_track_get_property;
	object_class->set_property = osm_gps_map_track_set_property;
	object_class->dispose = osm_gps_map_track_dispose;
	object_class->finalize = osm_gps_map_track_finalize;

	g_object_class_install_property(object_class,
		PROP_VISIBLE,
		g_param_spec_boolean("visible",
			"visible",
			"should this track be visible",
			TRUE,
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_TRACK,
		g_param_spec_pointer("track",
			"track",
			"list of points for the track",
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_HEADING,
		g_param_spec_double("heading",
			"heading",
			"heading in degrees",
			-1.0,  /* minimum property value */
			360.0, /* maximum property value */
			-1.0,
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_LINE_WIDTH,
		g_param_spec_double("line-width",
			"line-width",
			"width of the lines drawn for the track",
			0.0, /* minimum property value */
			100.0, /* maximum property value */
			4.0,
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_CONTINUOUS,
		g_param_spec_boolean("continuous",
			"continuous",
			"draw segments between points",
			TRUE,
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_ALPHA,
		g_param_spec_double("alpha",
			"alpha",
			"alpha transparency of the track",
			0.0, /* minimum property value */
			1.0, /* maximum property value */
			DEFAULT_A,
			G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_COLOR,
		g_param_spec_boxed("color",
			"color",
			"color of the track",
			GDK_TYPE_COLOR,
			G_PARAM_READABLE | G_PARAM_WRITABLE));

	signals[POINT_ADDED] = g_signal_new(
		"point-added",
		OSM_TYPE_GPS_MAP_TRACK,
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1, OSM_TYPE_GPS_MAP_POINT);

	signals[POINT_CHANGED] = g_signal_new(
		"point-changed",
		OSM_TYPE_GPS_MAP_TRACK,
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 1, G_TYPE_INT);

	signals[POINT_INSERTED] = g_signal_new(
		"point-inserted",
		OSM_TYPE_GPS_MAP_TRACK,
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	signals[POINT_REMOVED] = g_signal_new(
		"point-removed",
		OSM_TYPE_GPS_MAP_TRACK,
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
osm_gps_map_track_init(OsmGpsMapTrack * self) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE((self), OSM_TYPE_GPS_MAP_TRACK, OsmGpsMapTrackPrivate);

	self->priv->color.red = DEFAULT_R;
	self->priv->color.green = DEFAULT_G;
	self->priv->color.blue = DEFAULT_B;
}

void
osm_gps_map_track_add_point(OsmGpsMapTrack * track, const OsmGpsMapPoint * point) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	OsmGpsMapTrackPrivate *priv = track->priv;

	OsmGpsMapPoint *p = g_boxed_copy(OSM_TYPE_GPS_MAP_POINT, point);
	priv->track = g_slist_append(priv->track, p);
	g_signal_emit(track, signals[POINT_ADDED], 0, p);
}

void
osm_gps_map_track_remove_point(OsmGpsMapTrack * track, int pos) {
	OsmGpsMapTrackPrivate *priv = track->priv;
	gpointer pgl = g_slist_nth_data(priv->track, pos);
	priv->track = g_slist_remove(priv->track, pgl);
	g_signal_emit(track, signals[POINT_REMOVED], 0, pos);
}

int
osm_gps_map_track_n_points(OsmGpsMapTrack * track) {
	return g_slist_length(track->priv->track);
}

void
osm_gps_map_track_insert_point(OsmGpsMapTrack * track, OsmGpsMapPoint * np, int pos) {
	OsmGpsMapTrackPrivate *priv = track->priv;
	priv->track = g_slist_insert(priv->track, np, pos);
	g_signal_emit(track, signals[POINT_INSERTED], 0, pos);
}

OsmGpsMapPoint *
osm_gps_map_track_get_point(OsmGpsMapTrack * track, int pos) {
	OsmGpsMapTrackPrivate *priv = track->priv;
	return g_slist_nth_data(priv->track, pos);
}

GSList *
osm_gps_map_track_get_points(OsmGpsMapTrack * track) {
	g_return_val_if_fail(OSM_IS_GPS_MAP_TRACK(track), NULL);
	return track->priv->track;
}

void
osm_gps_map_track_set_visible(OsmGpsMapTrack * track, gboolean visible) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	track->priv->visible = visible;
}

gboolean
osm_gps_map_track_get_visible(OsmGpsMapTrack * track) {
	return track->priv->visible;
}

void
osm_gps_map_track_set_continuous(OsmGpsMapTrack * track, gboolean continuous) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	track->priv->continuous = continuous;
}

gboolean
osm_gps_map_track_get_continuous(OsmGpsMapTrack * track) {
	return track->priv->continuous;
}

void
osm_gps_map_track_set_heading(OsmGpsMapTrack * track, gdouble heading) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	track->priv->heading = heading;
}

gdouble
osm_gps_map_track_get_heading(OsmGpsMapTrack * track) {
	return track->priv->heading;
}

void
osm_gps_map_track_set_linewidth(OsmGpsMapTrack * track, gdouble linewidth) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	track->priv->linewidth = linewidth;
}

gdouble
osm_gps_map_track_get_linewidth(OsmGpsMapTrack * track) {
	return track->priv->linewidth;
}

void
osm_gps_map_track_set_color(OsmGpsMapTrack * track, GdkColor * color) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	track->priv->color.red = color->red;
	track->priv->color.green = color->green;
	track->priv->color.blue = color->blue;
}

void
osm_gps_map_track_get_color(OsmGpsMapTrack * track, GdkColor * color) {
	g_return_if_fail(OSM_IS_GPS_MAP_TRACK(track));
	color->red = track->priv->color.red;
	color->green = track->priv->color.green;
	color->blue = track->priv->color.blue;
}

double
osm_gps_map_track_get_length(OsmGpsMapTrack * track) {
	GSList *points = track->priv->track;
	double ret = 0;
	OsmGpsMapPoint *point_a = NULL;
	OsmGpsMapPoint *point_b = NULL;

	while (points) {
		point_a = point_b;
		point_b = points->data;
		if (point_a) {
			ret += acos(sin(point_a->rlat) * sin(point_b->rlat)
				  + cos(point_a->rlat) * cos(point_b->rlat)
				  * cos(point_b->rlon - point_a->rlon)) * 6371109;
		}
		points = points->next;
	}
	return ret;
}

OsmGpsMapTrack *
osm_gps_map_track_new(void) {
	return g_object_new(OSM_TYPE_GPS_MAP_TRACK, NULL);
}
static void
cached_tile_free(OsmCachedTile * tile) {
	g_object_unref(tile->pixbuf);
	g_slice_free(OsmCachedTile, tile);
}

/*
 * The (i)logb(x) function is equal to `floor(log(x) / log(2))` or
 * `floor(log2(x))`, but probably faster and also accepts negative values.
 * But this is only true when FLT_RADIX equals 2, which is on allmost all
 * machine.
 */
#if FLT_RADIX == 2
#define LOG2(x) (ilogb(x))
#else
#define LOG2(x) ((int)floor(log2(abs(x))))
#endif

double
deg2rad(double deg) {
	return (deg * M_PI / 180.0);
}

double
rad2deg(double rad) {
	return (rad / M_PI * 180.0);
}

int
lat2pixel(int zoom, double lat) {
	double lat_m;
	int pixel_y;

	lat_m = atanh(sin(lat));

	/* the formula is
	 *
	 * some more notes
	 * http://manialabs.wordpress.com/2013/01/26/converting-latitude-and-longitude-to-map-tile-in-mercator-projection/
	 *
	 * pixel_y = -(2^zoom * TILESIZE * lat_m) / 2PI + (2^zoom * TILESIZE) / 2
	 */
	pixel_y = -(int)((lat_m * TILESIZE * (1 << zoom)) / (2 * M_PI)) + ((1 << zoom) * (TILESIZE / 2));

	return pixel_y;
}

int
lon2pixel(int zoom, double lon) {
	int pixel_x;

	/* the formula is
	 *
	 * pixel_x = (2^zoom * TILESIZE * lon) / 2PI + (2^zoom * TILESIZE) / 2
	 */
	pixel_x = (int)((lon * TILESIZE * (1 << zoom)) / (2 * M_PI)) + ((1 << zoom) * (TILESIZE / 2));
	return pixel_x;
}

double
pixel2lon(double zoom, int pixel_x) {
	double lon;

	lon = ((pixel_x - (exp(zoom * M_LN2) * (TILESIZE / 2))) * 2 * M_PI) / (TILESIZE * exp(zoom * M_LN2));

	return lon;
}

double
pixel2lat(double zoom, int pixel_y) {
	double lat, lat_m;

	lat_m = (-(pixel_y - (exp(zoom * M_LN2) * (TILESIZE / 2))) * (2 * M_PI)) / (TILESIZE * exp(zoom * M_LN2));

	lat = asin(tanh(lat_m));

	return lat;
}

int
latlon2zoom(int pix_height, int pix_width, double lat1, double lat2, double lon1, double lon2) {
	double lat1_m = atanh(sin(lat1));
	double lat2_m = atanh(sin(lat2));
	int zoom_lon = LOG2((double)(2 * pix_width * M_PI) / (TILESIZE * (lon2 - lon1)));
	int zoom_lat = LOG2((double)(2 * pix_height * M_PI) / (TILESIZE * (lat2_m - lat1_m)));
	return MIN(zoom_lon, zoom_lat);
}

/*
 * Description:
 *   Find and replace text within a string.
 *
 * Parameters:
 *   src  (in) - pointer to source string
 *   from (in) - pointer to search text
 *   to   (in) - pointer to replacement text
 *
 * Returns:
 *   Returns a pointer to dynamically-allocated memory containing string
 *   with occurences of the text pointed to by 'from' replaced by with the
 *   text pointed to by 'to'.
 */
static gchar *
replace_string(const gchar * src, const gchar * from, const gchar * to) {
	size_t size = strlen(src) + 1;
	size_t fromlen = strlen(from);
	size_t tolen = strlen(to);

	/* Allocate the first chunk with enough for the original string. */
	gchar *value = g_malloc(size);

	/* We need to return 'value', so let's make a copy to mess around with. */
	gchar *dst = value;

	if (value != NULL) {
		for (;;) {
			/* Try to find the search text. */
			const gchar *match = g_strstr_len(src, size, from);
			if (match != NULL) {
				gchar *temp;
				/* Find out how many characters to copy up to the 'match'. */
				size_t count = match - src;

				/* Calculate the total size the string will be after the
				 * replacement is performed. */
				size += tolen - fromlen;

				temp = g_realloc(value, size);
				if (temp == NULL) {
					g_free(value);
					return NULL;
				}

				/* we'll want to return 'value' eventually, so let's point it
				 * to the memory that we are now working with.
				 * And let's not forget to point to the right location in
				 * the destination as well. */
				dst = temp + (dst - value);
				value = temp;

				/*
				 * Copy from the source to the point where we matched. Then
				 * move the source pointer ahead by the amount we copied. And
				 * move the destination pointer ahead by the same amount.
				 */
				g_memmove(dst, src, count);
				src += count;
				dst += count;

				/* Now copy in the replacement text 'to' at the position of
				 * the match. Adjust the source pointer by the text we replaced.
				 * Adjust the destination pointer by the amount of replacement
				 * text. */
				g_memmove(dst, to, tolen);
				src += fromlen;
				dst += tolen;
			} else {
				/*
				 * Copy any remaining part of the string. This includes the null
				 * termination character.
				 */
				strcpy(dst, src);
				break;
			}
		}
	}
	return value;
}

static void
map_convert_coords_to_quadtree_string(OsmGpsMap * map, gint x, gint y, gint zoomlevel,
				      gchar * buffer, const gchar initial, const gchar * const quadrant) {
	gchar *ptr = buffer;
	gint n;

	if (initial)
		*ptr++ = initial;

	for (n = zoomlevel - 1; n >= 0; n--) {
		gint xbit = (x >> n) & 1;
		gint ybit = (y >> n) & 1;
		*ptr++ = quadrant[xbit + 2 * ybit];
	}

	*ptr++ = '\0';
}


static gchar *
replace_map_uri(OsmGpsMap * map, const gchar * uri, int zoom, int x, int y) {
	OsmGpsMapPrivate *priv = map->priv;
	char *url;
	unsigned int i;
	char location[22];

	i = 1;
	url = g_strdup(uri);
	while (i < URI_FLAG_END) {
		char *s = NULL;
		char *old;

		old = url;
		switch (i & priv->uri_format) {
		case URI_HAS_X:
			s = g_strdup_printf("%d", x);
			url = replace_string(url, URI_MARKER_X, s);
			break;
		case URI_HAS_Y:
			s = g_strdup_printf("%d", y);
			url = replace_string(url, URI_MARKER_Y, s);
			break;
		case URI_HAS_Z:
			s = g_strdup_printf("%d", zoom);
			url = replace_string(url, URI_MARKER_Z, s);
			break;
		case URI_HAS_S:
			s = g_strdup_printf("%d", priv->max_zoom - zoom);
			url = replace_string(url, URI_MARKER_S, s);
			break;
		case URI_HAS_Q:
			map_convert_coords_to_quadtree_string(map, x, y, zoom, location, 't', "qrts");
			s = g_strdup_printf("%s", location);
			url = replace_string(url, URI_MARKER_Q, s);
			break;
		case URI_HAS_Q0:
			map_convert_coords_to_quadtree_string(map, x, y, zoom, location, '\0', "0123");
			s = g_strdup_printf("%s", location);
			url = replace_string(url, URI_MARKER_Q0, s);
			break;
		case URI_HAS_YS:
			g_warning("FOUND " URI_MARKER_YS " NOT IMPLEMENTED");
			break;
		case URI_HAS_R:
			s = g_strdup_printf("%d", g_random_int_range(0, 4));
			url = replace_string(url, URI_MARKER_R, s);
			break;
		default:
			s = NULL;
			break;
		}

		if (s) {
			g_free(s);
			g_free(old);
		}

		i = (i << 1);

	}

	return url;
}

static void
my_log_handler(const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data) {
	if (!(log_level & G_LOG_LEVEL_DEBUG) || ENABLE_DEBUG)
		g_log_default_handler(log_domain, log_level, message, user_data);
}

static double
osm_gps_map_get_scale_at_point(int zoom, double rlat, double rlon) {
	/* world at zoom 1 == 512 pixels */
	return cos(rlat) * M_PI * OSM_EQ_RADIUS / (1 << (7 + zoom));
}

static GSList *
gslist_remove_one_gobject(GSList ** list, GObject * gobj) {
	GSList *data = g_slist_find(*list, gobj);
	if (data) {
		g_object_unref(gobj);
		*list = g_slist_delete_link(*list, data);
	}
	return data;
}

static void
gslist_of_gobjects_free(GSList ** list) {
	if (list) {
		g_slist_foreach(*list, (GFunc) g_object_unref, NULL);
		g_slist_free(*list);
		*list = NULL;
	}
}

static void
gslist_of_data_free(GSList ** list) {
	if (list) {
		g_slist_foreach(*list, (GFunc) g_free, NULL);
		g_slist_free(*list);
		*list = NULL;
	}
}

static void
draw_red_rectangle(cairo_t * cr, double x, double y, double width, double height) {
	cairo_save(cr);
	cairo_set_source_rgb(cr, .3, .1, .1);
	cairo_rectangle(cr, x, y, width, height);
	cairo_fill(cr);
	cairo_restore(cr);
}

static void
draw_white_rectangle(cairo_t * cr, double x, double y, double width, double height) {
	cairo_save(cr);
	cairo_set_source_rgb(cr, .1, .1, .1);
	cairo_rectangle(cr, x, y, width, height);
	cairo_fill(cr);
	cairo_restore(cr);
}

static void
osm_gps_map_draw_home(OsmGpsMap *map, cairo_t * cr) {
	OsmGpsMapPrivate *priv = map->priv;
	int map_x0, map_y0;
	int x, y, r, NM;
	double scale;

	scale = 1852.0 / osm_gps_map_get_scale_at_point(priv->map_zoom, priv->home_rlat, priv->home_rlon);

	map_x0 = priv->map_x - EXTRA_BORDER;
	map_y0 = priv->map_y - EXTRA_BORDER;
	x = lon2pixel(priv->map_zoom, priv->home_rlon) - map_x0;
	y = lat2pixel(priv->map_zoom, priv->home_rlat) - map_y0;

	cairo_set_source_rgba(cr, 1.0, .2, .2, 1);
	cairo_arc(cr, x, y, 2, 0, 2 * M_PI);
	cairo_fill(cr);

	for (NM = 1; NM < 11; ++NM) {
		r = NM * scale;
		double clr = 1.0 - NM / 15.0;

		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba(cr, clr, clr, clr, .1);
		cairo_arc(cr, x, y, r, 0, 2 * M_PI);
		cairo_stroke(cr);
	}


	gtk_widget_queue_draw_area(GTK_WIDGET(map), x - r, y - r, r * 2, r * 2);
}

static void
osm_gps_map_blit_tile(OsmGpsMap * map, GdkPixbuf * pixbuf, cairo_t * cr, int offset_x, int offset_y,
		      int tile_zoom, int target_x, int target_y) {
	OsmGpsMapPrivate *priv = map->priv;
	int target_zoom = priv->map_zoom;

	if (tile_zoom == target_zoom) {
		g_debug("Blit @ %d,%d", offset_x, offset_y);
		/* draw pixbuf */
		gdk_cairo_set_source_pixbuf(cr, pixbuf, offset_x, offset_y);
		cairo_paint(cr);
	} else {
		/* get an upscaled version of the pixbuf */
		GdkPixbuf *pixmap_scaled = osm_gps_map_render_tile_upscaled(map, pixbuf, tile_zoom,
									    target_zoom, target_x, target_y);

		osm_gps_map_blit_tile(map, pixmap_scaled, cr, offset_x, offset_y, target_zoom, target_x, target_y);

		g_object_unref(pixmap_scaled);
	}
}

#define MSG_RESPONSE_BODY(a)    ((a)->response_body->data)
#define MSG_RESPONSE_LEN(a)     ((a)->response_body->length)
#define MSG_RESPONSE_LEN_FORMAT "%"G_GOFFSET_FORMAT

static void
osm_gps_map_tile_download_complete(SoupSession * session, SoupMessage * msg, gpointer user_data) {
	FILE *file;
	OsmTileDownload *dl = (OsmTileDownload *) user_data;
	OsmGpsMap *map = OSM_GPS_MAP(dl->map);
	OsmGpsMapPrivate *priv = map->priv;
	gboolean file_saved = FALSE;

	if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		/* save tile into cachedir if one has been specified */
		if (priv->cache_dir) {
			if (g_mkdir_with_parents(dl->folder, 0700) == 0) {
				file = g_fopen(dl->filename, "wb");
				if (file != NULL) {
					fwrite(MSG_RESPONSE_BODY(msg), 1, MSG_RESPONSE_LEN(msg), file);
					file_saved = TRUE;
					g_debug("Wrote " MSG_RESPONSE_LEN_FORMAT " bytes to %s", MSG_RESPONSE_LEN(msg),
						dl->filename);
					fclose(file);

				}
			} else {
				g_warning("Error creating tile download directory: %s", dl->folder);
			}
		}

		if (dl->redraw) {
			GdkPixbuf *pixbuf = NULL;

			/* if the file was actually stored on disk, we can simply */
			/* load and decode it from that file */
			if (priv->cache_dir) {
				if (file_saved) {
					pixbuf = gdk_pixbuf_new_from_file(dl->filename, NULL);
				}
			} else {
				GdkPixbufLoader *loader;
				char *extension = strrchr(dl->filename, '.');

				/* parse file directly from memory */
				if (extension) {
					loader = gdk_pixbuf_loader_new_with_type(extension + 1, NULL);
					if (!gdk_pixbuf_loader_write
					    (loader, (unsigned char *)MSG_RESPONSE_BODY(msg), MSG_RESPONSE_LEN(msg),
					     NULL)) {
						g_warning("Error: Decoding of image failed");
					}
					gdk_pixbuf_loader_close(loader, NULL);

					pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

					/* give up loader but keep the pixbuf */
					g_object_ref(pixbuf);
					g_object_unref(loader);
				} else {
					g_warning("Error: Unable to determine image file format");
				}
			}

			/* Store the tile into the cache */
			if (G_LIKELY(pixbuf)) {
				OsmCachedTile *tile = g_slice_new(OsmCachedTile);
				tile->pixbuf = pixbuf;
				tile->redraw_cycle = priv->redraw_cycle;
				/* if the tile is already in the cache (it could be one
				 * rendered from another zoom level), it will be
				 * overwritten */
				g_hash_table_insert(priv->tile_cache, dl->filename, tile);
				/* NULL-ify dl->filename so that it won't be freed, as
				 * we are using it as a key in the hash table */
				dl->filename = NULL;
			}
			osm_gps_map_map_redraw_idle(map);
		}
		g_hash_table_remove(priv->tile_queue, dl->uri);
		g_object_notify(G_OBJECT(map), "tiles-queued");

		g_free(dl->folder);
		g_free(dl->filename);
		g_free(dl);
	} else {
		if ((msg->status_code == SOUP_STATUS_NOT_FOUND) || (msg->status_code == SOUP_STATUS_FORBIDDEN)) {
			g_hash_table_insert(priv->missing_tiles, dl->uri, NULL);
			g_hash_table_remove(priv->tile_queue, dl->uri);
			g_object_notify(G_OBJECT(map), "tiles-queued");
		} else if (msg->status_code == SOUP_STATUS_CANCELLED) {
			g_hash_table_remove(priv->tile_queue, dl->uri);
			g_object_notify(G_OBJECT(map), "tiles-queued");
		} else {
			g_warning("Error downloading tile: %d - %s", msg->status_code, msg->reason_phrase);
			dl->ttl--;
			if (dl->ttl) {
				soup_session_requeue_message(session, msg);
				return;
			}

			g_hash_table_remove(priv->tile_queue, dl->uri);
			g_object_notify(G_OBJECT(map), "tiles-queued");
		}
	}

}

static void
osm_gps_map_download_tile(OsmGpsMap * map, int zoom, int x, int y, gboolean redraw) {
	SoupMessage *msg;
	OsmGpsMapPrivate *priv = map->priv;
	OsmTileDownload *dl = g_new0(OsmTileDownload, 1);

	if (zoom > 15)	/* This ain't a map program */
		return;

	dl->ttl = DOWNLOAD_RETRIES;
	dl->uri = replace_map_uri(map, priv->repo_uri, zoom, x, y);
	if (g_hash_table_lookup_extended(priv->tile_queue, dl->uri, NULL, NULL) ||
	    g_hash_table_lookup_extended(priv->missing_tiles, dl->uri, NULL, NULL)) {
		g_debug("Tile already downloading (or missing)");
		g_free(dl->uri);
		g_free(dl);
	} else {
		dl->folder = g_strdup_printf("%s%c%d%c%d%c",
					     priv->cache_dir, G_DIR_SEPARATOR,
					     zoom, G_DIR_SEPARATOR, x, G_DIR_SEPARATOR);
		dl->filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
					       priv->cache_dir, G_DIR_SEPARATOR,
					       zoom, G_DIR_SEPARATOR, x, G_DIR_SEPARATOR, y, priv->image_format);
		dl->map = map;
		dl->redraw = redraw;

		printf("Download tile: %d,%d z:%d\n\t%s --> %s\n", x, y, zoom, dl->uri, dl->filename);
		g_debug("Download tile: %d,%d z:%d\n\t%s --> %s", x, y, zoom, dl->uri, dl->filename);

		msg = soup_message_new(SOUP_METHOD_GET, dl->uri);
		if (msg) {
			g_hash_table_insert(priv->tile_queue, dl->uri, msg);
			g_object_notify(G_OBJECT(map), "tiles-queued");
			/* the soup session unrefs the message when the download finishes */
			soup_session_queue_message(priv->soup_session, msg, osm_gps_map_tile_download_complete, dl);
		} else {
			g_warning("Could not create soup message");
			g_free(dl->uri);
			g_free(dl->folder);
			g_free(dl->filename);
			g_free(dl);
		}
	}
}

static GdkPixbuf *
osm_gps_map_load_cached_tile(OsmGpsMap * map, int zoom, int x, int y) {
	OsmGpsMapPrivate *priv = map->priv;
	gchar *filename;
	GdkPixbuf *pixbuf = NULL;
	OsmCachedTile *tile;

	filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
				   priv->cache_dir, G_DIR_SEPARATOR,
				   zoom, G_DIR_SEPARATOR, x, G_DIR_SEPARATOR, y, priv->image_format);

	tile = g_hash_table_lookup(priv->tile_cache, filename);
	if (tile) {
		g_free(filename);
	} else {
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		if (pixbuf) {
			tile = g_slice_new(OsmCachedTile);
			tile->pixbuf = pixbuf;
			g_hash_table_insert(priv->tile_cache, filename, tile);
		} else {
			g_free(filename);
		}
	}

	/* set/update the redraw_cycle timestamp on the tile */
	if (tile) {
		tile->redraw_cycle = priv->redraw_cycle;
		pixbuf = g_object_ref(tile->pixbuf);
	}

	return pixbuf;
}

static GdkPixbuf *
osm_gps_map_find_bigger_tile(OsmGpsMap * map, int zoom, int x, int y, int *zoom_found) {
	GdkPixbuf *pixbuf;
	int next_zoom, next_x, next_y;

	if (zoom == 0)
		return NULL;
	next_zoom = zoom - 1;
	next_x = x / 2;
	next_y = y / 2;
	pixbuf = osm_gps_map_load_cached_tile(map, next_zoom, next_x, next_y);
	if (pixbuf)
		*zoom_found = next_zoom;
	else
		pixbuf = osm_gps_map_find_bigger_tile(map, next_zoom, next_x, next_y, zoom_found);
	return pixbuf;
}

static GdkPixbuf *
osm_gps_map_render_missing_tile_upscaled(OsmGpsMap * map, int zoom, int x, int y) {
	GdkPixbuf *pixbuf, *big;
	int zoom_big;

	big = osm_gps_map_find_bigger_tile(map, zoom, x, y, &zoom_big);
	if (!big)
		return NULL;

	g_debug("Found bigger tile (zoom = %d, wanted = %d)", zoom_big, zoom);

	pixbuf = osm_gps_map_render_tile_upscaled(map, big, zoom_big, zoom, x, y);
	g_object_unref(big);

	return pixbuf;
}

static GdkPixbuf *
osm_gps_map_render_tile_upscaled(OsmGpsMap * map, GdkPixbuf * big, int zoom_big, int zoom, int x, int y) {
	GdkPixbuf *pixbuf, *area;
	int area_size, area_x, area_y;
	int modulo;
	int zoom_diff;

	/* get a Pixbuf for the area to magnify */
	zoom_diff = zoom - zoom_big;

	g_debug("Upscaling by %d levels into tile %d,%d", zoom_diff, x, y);

	area_size = TILESIZE >> zoom_diff;
	modulo = 1 << zoom_diff;
	area_x = (x % modulo) * area_size;
	area_y = (y % modulo) * area_size;
	area = gdk_pixbuf_new_subpixbuf(big, area_x, area_y, area_size, area_size);
	pixbuf = gdk_pixbuf_scale_simple(area, TILESIZE, TILESIZE, GDK_INTERP_NEAREST);
	g_object_unref(area);
	return pixbuf;
}

static GdkPixbuf *
osm_gps_map_render_missing_tile(OsmGpsMap * map, int zoom, int x, int y) {
	/* maybe TODO: render from downscaled tiles, if the following fails */
	return osm_gps_map_render_missing_tile_upscaled(map, zoom, x, y);
}

static void
osm_gps_map_load_tile(OsmGpsMap * map, cairo_t * cr, int zoom, int x, int y, int offset_x, int offset_y) {
	OsmGpsMapPrivate *priv = map->priv;
	gchar *filename;
	GdkPixbuf *pixbuf;
	int zoom_offset = priv->tile_zoom_offset;
	int target_x, target_y;

	g_debug("Load virtual tile %d,%d (%d,%d) z:%d", x, y, offset_x, offset_y, zoom);

	if (zoom > MIN_ZOOM) {
		zoom -= zoom_offset;
		x >>= zoom_offset;
		y >>= zoom_offset;
	}

	target_x = x;
	target_y = y;

	g_debug("Load actual tile %d,%d (%d,%d) z:%d", x, y, offset_x, offset_y, zoom);

	filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
				   priv->cache_dir, G_DIR_SEPARATOR,
				   zoom, G_DIR_SEPARATOR, x, G_DIR_SEPARATOR, y, priv->image_format);

	/* try to get file from internal cache first */
	if (!(pixbuf = osm_gps_map_load_cached_tile(map, zoom, x, y)))
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

	if (pixbuf) {
		g_debug("Found tile %s", filename);
		osm_gps_map_blit_tile(map, pixbuf, cr, offset_x, offset_y, zoom, target_x, target_y);
		g_object_unref(pixbuf);
	} else {
		if (priv->map_auto_download_enabled) {
			osm_gps_map_download_tile(map, zoom, x, y, TRUE);
		}

		/* try to render the tile by scaling cached tiles from other zoom
		 * levels */
		pixbuf = osm_gps_map_render_missing_tile(map, zoom, x, y);
		if (pixbuf) {
			osm_gps_map_blit_tile(map, pixbuf, cr, offset_x, offset_y, zoom, target_x, target_y);
			g_object_unref(pixbuf);
		} else {
			/* prevent some artifacts when drawing not yet loaded areas. */
			/*   g_warning ("Error getting missing tile"); FIXME: is this a warning? */
			draw_white_rectangle(cr, offset_x, offset_y, TILESIZE, TILESIZE);
		}
	}
	g_free(filename);
}

static void
osm_gps_map_fill_tiles_pixel(OsmGpsMap * map, cairo_t * cr) {
	OsmGpsMapPrivate *priv = map->priv;
	GtkAllocation allocation;
	int i, j, tile_x0, tile_y0, tiles_nx, tiles_ny;
	int offset_xn = 0;
	int offset_yn = 0;
	int offset_x;
	int offset_y;

	g_debug("Fill tiles: %d,%d z:%d", priv->map_x, priv->map_y, priv->map_zoom);

	gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);

	offset_x = -priv->map_x % TILESIZE;
	offset_y = -priv->map_y % TILESIZE;
	if (offset_x > 0)
		offset_x -= TILESIZE;
	if (offset_y > 0)
		offset_y -= TILESIZE;

	offset_xn = offset_x + EXTRA_BORDER;
	offset_yn = offset_y + EXTRA_BORDER;

	tiles_nx = (allocation.width - offset_x) / TILESIZE + 1;
	tiles_ny = (allocation.height - offset_y) / TILESIZE + 1;

	tile_x0 = floor((double)priv->map_x / (double)TILESIZE);
	tile_y0 = floor((double)priv->map_y / (double)TILESIZE);

	for (i = tile_x0; i < (tile_x0 + tiles_nx); i++) {
		for (j = tile_y0; j < (tile_y0 + tiles_ny); j++) {
			if (j < 0 || i < 0 || i >= exp(priv->map_zoom * M_LN2) || j >= exp(priv->map_zoom * M_LN2)) {
				/* draw white in areas outside map (i.e. when zoomed right out) */
				draw_white_rectangle(cr, offset_xn, offset_yn, TILESIZE, TILESIZE);
			} else {
				osm_gps_map_load_tile(map,
						      cr,
						      priv->map_zoom,
						      i, j, offset_xn - EXTRA_BORDER, offset_yn - EXTRA_BORDER);
			}
			offset_yn += TILESIZE;
		}
		offset_xn += TILESIZE;
		offset_yn = offset_y + EXTRA_BORDER;
	}
}

static void
osm_gps_map_print_track(OsmGpsMap * map, OsmGpsMapTrack * track, cairo_t * cr) {
	OsmGpsMapPrivate *priv = map->priv;

	GSList *pt, *points;
	int x, y;
	int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
	gdouble lw, alpha, heading;
	int map_x0, map_y0;
	GdkColor color;

	g_object_get(track,
		"track", &points,
		"line-width", &lw,
		"alpha", &alpha,
		"heading", &heading,
		NULL);
	osm_gps_map_track_get_color(track, &color);
	gboolean continuous = osm_gps_map_track_get_continuous(track);

	if (!osm_gps_map_track_get_visible(track))
		return;

	if (points == NULL)
		return;

	cairo_set_line_width(cr, lw);
	cairo_set_source_rgba(cr, color.red/65535.0, color.green/65535.0, color.blue/65535.0, alpha);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

	map_x0 = priv->map_x - EXTRA_BORDER;
	map_y0 = priv->map_y - EXTRA_BORDER;

	int last_x = 0, last_y = 0;
	for (pt = points; pt != NULL; pt = pt->next) {
		OsmGpsMapPoint *tp = pt->data;

		x = lon2pixel(priv->map_zoom, tp->rlon) - map_x0;
		y = lat2pixel(priv->map_zoom, tp->rlat) - map_y0;

		/* first time through loop */
		if (pt == points)
			cairo_move_to(cr, x, y);

		if (continuous)
			cairo_line_to(cr, x, y);
		else
			cairo_rectangle(cr, x, y, 0.5, 0.5);

		cairo_stroke(cr);
		cairo_move_to(cr, x, y);

		max_x = MAX(x, max_x);
		min_x = MIN(x, min_x);
		max_y = MAX(y, max_y);
		min_y = MIN(y, min_y);

		last_x = x;
		last_y = y;
	}

	if (heading >= 0.0 && heading <= 360.0) {
		double h_rad = deg2rad(heading);
		int r = 3 * lw;
		cairo_set_source_rgba(cr, color.red/65535.0, color.green/65535.0, color.blue/65535.0, 1.0);
		cairo_move_to(cr, last_x - 1 * r * cos(h_rad), last_y - 1 * r * sin(h_rad));
		cairo_line_to(cr, last_x + 3 * r * sin(h_rad), last_y - 3 * r * cos(h_rad));
		cairo_line_to(cr, last_x + 1 * r * cos(h_rad), last_y + 1 * r * sin(h_rad));
		cairo_close_path(cr);

		cairo_fill_preserve(cr);

		cairo_set_line_width(cr, 1.0);
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.5);
		cairo_stroke(cr);
	}

	gtk_widget_queue_draw_area(GTK_WIDGET(map), min_x - lw, min_y - lw, max_x + (lw * 2), max_y + (lw * 2));

	cairo_stroke(cr);
}

/* Prints the gps trip history, and any other tracks */
static void
osm_gps_map_print_tracks(OsmGpsMap * map, cairo_t * cr) {
	GSList *tmp;
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->tracks) {
		tmp = priv->tracks;
		while (tmp != NULL) {
			osm_gps_map_print_track(map, OSM_GPS_MAP_TRACK(tmp->data), cr);
			tmp = g_slist_next(tmp);
		}
	}
}

static gboolean
osm_gps_map_purge_cache_check(gpointer key, gpointer value, gpointer user) {
	return (((OsmCachedTile *) value)->redraw_cycle != ((OsmGpsMapPrivate *) user)->redraw_cycle);
}

static void
osm_gps_map_purge_cache(OsmGpsMap * map) {
	OsmGpsMapPrivate *priv = map->priv;

	if (g_hash_table_size(priv->tile_cache) < priv->max_tile_cache_size)
		return;

	/* run through the cache, and remove the tiles which have not been used
	 * during the last redraw operation */
	g_hash_table_foreach_remove(priv->tile_cache, osm_gps_map_purge_cache_check, priv);
}

gboolean
osm_gps_map_map_redraw(OsmGpsMap * map) {
	cairo_t *cr;
	int w, h;
	OsmGpsMapPrivate *priv = map->priv;
	GtkWidget *widget = GTK_WIDGET(map);
	GtkAllocation all;

	priv->idle_map_redraw = 0;

	/* dont't redraw if we have not been shown yet */
	if (!priv->pixmap)
		return FALSE;

	/* the motion_notify handler uses priv->surface to redraw the area; if we
	 * change it while we are dragging, we will end up showing it in the wrong
	 * place. This could be fixed by carefully recompute the coordinates, but
	 * for now it's easier just to disable redrawing the map while dragging */
	if (priv->is_dragging)
		return FALSE;

	/* paint to the backing surface */
	cr = cairo_create(priv->pixmap);

	/* undo all offsets that may have happened when dragging */
	priv->drag_mouse_dx = 0;
	priv->drag_mouse_dy = 0;

	priv->redraw_cycle++;

	/* clear white background */
	gtk_widget_get_allocation(widget, &all);
	w = all.width;
	h = all.height;
	draw_white_rectangle(cr, 0, 0, w + EXTRA_BORDER * 2, h + EXTRA_BORDER * 2);
	osm_gps_map_fill_tiles_pixel(map, cr);

	if (priv->show_home && !isnan(priv->home_rlat) && !isnan(priv->home_rlon))
		osm_gps_map_draw_home(map, cr);

	osm_gps_map_print_tracks(map, cr);
	osm_gps_map_purge_cache(map);
	gtk_widget_queue_draw(GTK_WIDGET(map));

	cairo_destroy(cr);

	return FALSE;
}

void
osm_gps_map_map_redraw_idle(OsmGpsMap * map) {
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->idle_map_redraw == 0)
		priv->idle_map_redraw = g_idle_add((GSourceFunc) osm_gps_map_map_redraw, map);
}

/* call this to update center_rlat and center_rlon after
 * changin map_x or map_y */
static void
center_coord_update(OsmGpsMap * map) {

	GtkWidget *widget = GTK_WIDGET(map);
	OsmGpsMapPrivate *priv = map->priv;
	GtkAllocation allocation;

	gtk_widget_get_allocation(widget, &allocation);
	gint pixel_x = priv->map_x + allocation.width / 2;
	gint pixel_y = priv->map_y + allocation.height / 2;

	priv->center_rlon = pixel2lon(priv->map_zoom, pixel_x);
	priv->center_rlat = pixel2lat(priv->map_zoom, pixel_y);

	g_signal_emit_by_name(widget, "changed");
}

static void
on_gps_point_added(OsmGpsMapTrack * track, OsmGpsMapPoint * point, OsmGpsMap * map) {
	osm_gps_map_map_redraw_idle(map);
}
static void
on_track_changed(OsmGpsMapTrack * track, GParamSpec * pspec, OsmGpsMap * map) {
	osm_gps_map_map_redraw_idle(map);
}

static void
osm_gps_map_init(OsmGpsMap * object) {
	OsmGpsMapPrivate *priv;

	priv = G_TYPE_INSTANCE_GET_PRIVATE(object, OSM_TYPE_GPS_MAP, OsmGpsMapPrivate);
	object->priv = priv;

	priv->pixmap = NULL;

	priv->home_rlat = OSM_GPS_MAP_INVALID;
	priv->home_rlon = OSM_GPS_MAP_INVALID;
	priv->tracks = NULL;
	priv->drag_counter = 0;
	priv->drag_mouse_dx = 0;
	priv->drag_mouse_dy = 0;
	priv->drag_start_mouse_x = 0;
	priv->drag_start_mouse_y = 0;

	priv->uri_format = 0;
	priv->repo_uri = NULL;
	priv->repo_attrib = NULL;
	priv->map_source = -1;

	/* set the user agent */
	/*
	priv->soup_session = soup_session_async_new_with_options(SOUP_SESSION_USER_AGENT, USER_AGENT, NULL);
	*/
	priv->soup_session = soup_session_new();

	/* Hash table which maps tile d/l URIs to SoupMessage requests, the hashtable
	   must free the key, the soup session unrefs the message */
	priv->tile_queue = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	priv->missing_tiles = g_hash_table_new(g_str_hash, g_str_equal);

	/* memory cache for most recently used tiles */
	priv->tile_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) cached_tile_free);
	priv->max_tile_cache_size = 20;

	gtk_widget_add_events(GTK_WIDGET(object),
			      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			      GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	gtk_widget_set_can_focus(GTK_WIDGET(object), TRUE);

	g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_MASK, my_log_handler, NULL);

	/* setup signal handlers
	g_signal_connect(object, "key_press_event", G_CALLBACK(on_window_key_press), priv); */
}

static char *
osm_gps_map_get_cache_base_dir(OsmGpsMapPrivate * priv) {
	if (priv->tile_base_dir)
		return g_strdup(priv->tile_base_dir);
	return osm_gps_map_get_default_cache_directory();
}

static void
osm_gps_map_setup(OsmGpsMap * map) {
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->map_source >= sizeof(sources) / sizeof(sources[0]))
		priv->map_source = -1;

	if (priv->map_source < 0 && !priv->repo_uri)
		priv->map_source = 0;

	if (priv->image_format)
		g_free(priv->image_format);

	if (priv->map_source >= 0) {
		priv->repo_uri = g_strdup(sources[priv->map_source].uri);
		priv->repo_attrib = sources[priv->map_source].attrib;
		priv->image_format = g_strdup(sources[priv->map_source].format);
		priv->max_zoom = sources[priv->map_source].max_zoom;
		priv->min_zoom = sources[priv->map_source].min_zoom;
	} else {
		char *ext = NULL;
		if ((ext = strrchr(priv->repo_uri, '.'))) {
			size_t len, i;
			gboolean ok;
			
			len = strlen(++ext);
			ok = 0 < len && len < 5;
			for (i = 0; i < len; ++i) {
				if (!isalnum(ext[i])) {
					ok = FALSE;
				}
			}
			if (!ok)
				ext = NULL;
		}
		if (ext)
			priv->image_format = g_strdup(ext);
		else
			priv->image_format = g_strdup("data");
	}

	/* parse the source uri */
	priv->uri_format = 0;
	if (g_strrstr(priv->repo_uri, URI_MARKER_X))
		priv->uri_format |= URI_HAS_X;
	if (g_strrstr(priv->repo_uri, URI_MARKER_Y))
		priv->uri_format |= URI_HAS_Y;
	if (g_strrstr(priv->repo_uri, URI_MARKER_Z))
		priv->uri_format |= URI_HAS_Z;
	if (g_strrstr(priv->repo_uri, URI_MARKER_S))
		priv->uri_format |= URI_HAS_S;
	if (g_strrstr(priv->repo_uri, URI_MARKER_Q))
		priv->uri_format |= URI_HAS_Q;
	if (g_strrstr(priv->repo_uri, URI_MARKER_Q0))
		priv->uri_format |= URI_HAS_Q0;
	if (g_strrstr(priv->repo_uri, URI_MARKER_YS))
		priv->uri_format |= URI_HAS_YS;
	if (g_strrstr(priv->repo_uri, URI_MARKER_R))
		priv->uri_format |= URI_HAS_R;
	g_debug("URI Format: 0x%X", priv->uri_format);


	/* setup the tile cache */
	{
		char *base = osm_gps_map_get_cache_base_dir(priv);
		char *md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, priv->repo_uri, -1);
		g_free(priv->cache_dir);
		priv->cache_dir = g_strdup_printf("%s%c%s", base, G_DIR_SEPARATOR, md5);
		g_free(base);
		g_free(md5);
	}
	g_debug("Cache dir: %s", priv->cache_dir);

	/* check if we are being called for a second (or more) time in the lifetime
	   of the object, and if so, do some extra cleanup */
	if (priv->is_constructed) {
		g_debug("Setup called again in map lifetime");
		/* flush the ram cache */
		g_hash_table_remove_all(priv->tile_cache);

		/* adjust zoom if necessary */
		if (priv->map_zoom > priv->max_zoom)
			osm_gps_map_set_zoom(map, priv->max_zoom);

		if (priv->map_zoom < priv->min_zoom)
			osm_gps_map_set_zoom(map, priv->min_zoom);

		osm_gps_map_map_redraw_idle(map);
	}
}

static GObject *
osm_gps_map_constructor(GType gtype, guint n_properties, GObjectConstructParam * properties) {
	GObject *object;
	OsmGpsMap *map;

	/* always chain up to the parent constructor */
	object = G_OBJECT_CLASS(osm_gps_map_parent_class)->constructor(gtype, n_properties, properties);

	map = OSM_GPS_MAP(object);

	osm_gps_map_setup(map);
	map->priv->is_constructed = TRUE;

	return object;
}

static void
osm_gps_map_dispose(GObject * object) {
	OsmGpsMap *map = OSM_GPS_MAP(object);
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->is_disposed)
		return;

	priv->is_disposed = TRUE;

	soup_session_abort(priv->soup_session);
	g_object_unref(priv->soup_session);

	g_hash_table_destroy(priv->tile_queue);
	g_hash_table_destroy(priv->missing_tiles);
	g_hash_table_destroy(priv->tile_cache);

	gslist_of_gobjects_free(&priv->tracks);

	if (priv->pixmap)
		cairo_surface_destroy(priv->pixmap);

	if (priv->null_tile)
		g_object_unref(priv->null_tile);

	if (priv->idle_map_redraw != 0)
		g_source_remove(priv->idle_map_redraw);

	if (priv->drag_expose_source != 0)
		g_source_remove(priv->drag_expose_source);

	G_OBJECT_CLASS(osm_gps_map_parent_class)->dispose(object);
}

static void
osm_gps_map_finalize(GObject * object) {
	OsmGpsMap *map = OSM_GPS_MAP(object);
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->tile_dir)
		g_free(priv->tile_dir);

	g_free(priv->tile_base_dir);

	if (priv->cache_dir)
		g_free(priv->cache_dir);

	g_free(priv->image_format);
	g_free(priv->repo_uri);
	g_free(priv->proxy_uri);

	G_OBJECT_CLASS(osm_gps_map_parent_class)->finalize(object);
}

static void
osm_gps_map_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) {
	g_return_if_fail(OSM_IS_GPS_MAP(object));
	OsmGpsMap *map = OSM_GPS_MAP(object);
	OsmGpsMapPrivate *priv = map->priv;

	switch (prop_id) {
	case PROP_AUTO_CENTER:
		priv->map_auto_center_enabled = g_value_get_boolean(value);
		break;
	case PROP_RECORD_TRIP_HISTORY:
		priv->trip_history_record_enabled = g_value_get_boolean(value);
		break;
	case PROP_SHOW_TRIP_HISTORY:
		priv->trip_history_show_enabled = g_value_get_boolean(value);
		break;
	case PROP_AUTO_DOWNLOAD:
		priv->map_auto_download_enabled = g_value_get_boolean(value);
		break;
	case PROP_REPO_URI:
		g_free(priv->repo_uri);
		priv->repo_uri = g_value_dup_string(value);
		break;
	case PROP_PROXY_URI:
		if (g_value_get_string(value)) {
			g_free(priv->proxy_uri);
			priv->proxy_uri = g_value_dup_string(value);
			g_debug("Setting proxy server: %s", priv->proxy_uri);

			GValue val = { 0 };
			SoupURI *uri = soup_uri_new(priv->proxy_uri);
			g_value_init(&val, SOUP_TYPE_URI);
			g_value_take_boxed(&val, uri);
			g_object_set_property(G_OBJECT(priv->soup_session), SOUP_SESSION_PROXY_URI, &val);

		} else {
			g_free(priv->proxy_uri);
			priv->proxy_uri = NULL;
		}
		break;
	case PROP_TILE_CACHE_BASE_DIR:
		g_free(priv->tile_base_dir);
		priv->tile_base_dir = g_value_dup_string(value);
		break;
	case PROP_TILE_ZOOM_OFFSET:
		priv->tile_zoom_offset = g_value_get_int(value);
		break;
	case PROP_ZOOM:
		priv->map_zoom = g_value_get_int(value);
		break;
	case PROP_MAX_ZOOM:
		priv->max_zoom = g_value_get_int(value);
		break;
	case PROP_MIN_ZOOM:
		priv->min_zoom = g_value_get_int(value);
		break;
	case PROP_HOME_LAT:
		priv->home_rlat = deg2rad(g_value_get_double(value));
		break;
	case PROP_HOME_LON:
		priv->home_rlon = deg2rad(g_value_get_double(value));
		break;
	case PROP_MAP_X:
		priv->map_x = g_value_get_int(value);
		center_coord_update(map);
		break;
	case PROP_MAP_Y:
		priv->map_y = g_value_get_int(value);
		center_coord_update(map);
		break;
	case PROP_MAP_SOURCE:
		priv->map_source = g_value_get_int(value);
		break;
	case PROP_IMAGE_FORMAT:
		g_free(priv->image_format);
		priv->image_format = g_value_dup_string(value);
		break;
	case PROP_DRAG_LIMIT:
		priv->drag_limit = g_value_get_int(value);
		break;
	case PROP_AUTO_CENTER_THRESHOLD:
		priv->map_auto_center_threshold = g_value_get_double(value);
		break;
	case PROP_SHOW_HOME:
		priv->show_home = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
osm_gps_map_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) {
	g_return_if_fail(OSM_IS_GPS_MAP(object));
	OsmGpsMap *map = OSM_GPS_MAP(object);
	OsmGpsMapPrivate *priv = map->priv;

	switch (prop_id) {
	case PROP_AUTO_CENTER:
		g_value_set_boolean(value, priv->map_auto_center_enabled);
		break;
	case PROP_RECORD_TRIP_HISTORY:
		g_value_set_boolean(value, priv->trip_history_record_enabled);
		break;
	case PROP_SHOW_TRIP_HISTORY:
		g_value_set_boolean(value, priv->trip_history_show_enabled);
		break;
	case PROP_AUTO_DOWNLOAD:
		g_value_set_boolean(value, priv->map_auto_download_enabled);
		break;
	case PROP_REPO_URI:
		g_value_set_string(value, priv->repo_uri);
		break;
	case PROP_PROXY_URI:
		g_value_set_string(value, priv->proxy_uri);
		break;
	case PROP_TILE_CACHE_BASE_DIR:
		g_value_set_string(value, priv->tile_base_dir);
		break;
	case PROP_TILE_ZOOM_OFFSET:
		g_value_set_int(value, priv->tile_zoom_offset);
		break;
	case PROP_ZOOM:
		g_value_set_int(value, priv->map_zoom);
		break;
	case PROP_MAX_ZOOM:
		g_value_set_int(value, priv->max_zoom);
		break;
	case PROP_MIN_ZOOM:
		g_value_set_int(value, priv->min_zoom);
		break;
	case PROP_HOME_LAT:
		g_value_set_double(value, rad2deg(priv->home_rlat));
		break;
	case PROP_HOME_LON:
		g_value_set_double(value, rad2deg(priv->home_rlon));
		break;
	case PROP_LATITUDE:
		g_value_set_double(value, rad2deg(priv->center_rlat));
		break;
	case PROP_LONGITUDE:
		g_value_set_double(value, rad2deg(priv->center_rlon));
		break;
	case PROP_MAP_X:
		g_value_set_int(value, priv->map_x);
		break;
	case PROP_MAP_Y:
		g_value_set_int(value, priv->map_y);
		break;
	case PROP_TILES_QUEUED:
		g_value_set_int(value, g_hash_table_size(priv->tile_queue));
		break;
	case PROP_MAP_SOURCE:
		g_value_set_int(value, priv->map_source);
		break;
	case PROP_IMAGE_FORMAT:
		g_value_set_string(value, priv->image_format);
		break;
	case PROP_DRAG_LIMIT:
		g_value_set_int(value, priv->drag_limit);
		break;
	case PROP_AUTO_CENTER_THRESHOLD:
		g_value_set_double(value, priv->map_auto_center_threshold);
		break;
	case PROP_SHOW_HOME:
		g_value_set_boolean(value, priv->show_home);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
osm_gps_map_scroll_event(GtkWidget * widget, GdkEventScroll * event) {
	OsmGpsMap *map;
	OsmGpsMapPoint *pt;
	double lat, lon, c_lat, c_lon;

	map = OSM_GPS_MAP(widget);
	pt = osm_gps_map_point_new_degrees(0.0, 0.0);
	/* arguably we could use get_event_location here, but I'm not convinced it
	   is forward compatible to cast between GdkEventScroll and GtkEventButton */
	osm_gps_map_convert_screen_to_geographic(map, event->x, event->y, pt);
	osm_gps_map_point_get_degrees(pt, &lat, &lon);

	c_lat = rad2deg(map->priv->center_rlat);
	c_lon = rad2deg(map->priv->center_rlon);

	if ((event->direction == GDK_SCROLL_UP) && (map->priv->map_zoom < map->priv->max_zoom)) {
		lat = c_lat + ((lat - c_lat) / 2.0);
		lon = c_lon + ((lon - c_lon) / 2.0);
		osm_gps_map_set_center_and_zoom(map, lat, lon, map->priv->map_zoom + 1);
	} else if ((event->direction == GDK_SCROLL_DOWN) && (map->priv->map_zoom > map->priv->min_zoom)) {
		lat = c_lat + ((c_lat - lat) * 1.0);
		lon = c_lon + ((c_lon - lon) * 1.0);
		osm_gps_map_set_center_and_zoom(map, lat, lon, map->priv->map_zoom - 1);
	}

	osm_gps_map_point_free(pt);

	return FALSE;
}

static gboolean
osm_gps_map_button_press(GtkWidget * widget, GdkEventButton * event) {
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;

	priv->is_button_down = TRUE;
	priv->drag_counter = 0;
	priv->drag_start_mouse_x = (int)event->x;
	priv->drag_start_mouse_y = (int)event->y;
	priv->drag_start_map_x = priv->map_x;
	priv->drag_start_map_y = priv->map_y;

	return FALSE;
}

static gboolean
osm_gps_map_button_release(GtkWidget * widget, GdkEventButton * event) {
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;

	if (!priv->is_button_down)
		return FALSE;

	if (priv->is_dragging) {
		priv->is_dragging = FALSE;

		priv->map_x = priv->drag_start_map_x;
		priv->map_y = priv->drag_start_map_y;

		priv->map_x += (priv->drag_start_mouse_x - (int)event->x);
		priv->map_y += (priv->drag_start_mouse_y - (int)event->y);

		center_coord_update(map);

		osm_gps_map_map_redraw_idle(map);
	}

	if (priv->is_dragging_point) {
		priv->is_dragging_point = FALSE;
		osm_gps_map_convert_screen_to_geographic(map, event->x, event->y, priv->drag_point);
		g_signal_emit_by_name(priv->drag_track, "point-changed");
	}

	priv->drag_counter = -1;
	priv->is_button_down = FALSE;

	return FALSE;
}

static gboolean
osm_gps_map_idle_expose(GtkWidget * widget) {
	OsmGpsMapPrivate *priv = OSM_GPS_MAP(widget)->priv;
	priv->drag_expose_source = 0;
	gtk_widget_queue_draw(widget);
	return FALSE;
}

static gboolean
osm_gps_map_motion_notify(GtkWidget * widget, GdkEventMotion * event) {
	GdkModifierType state;
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;
	gint x, y;

/*
    GdkDeviceManager* manager = gdk_display_get_device_manager( gdk_display_get_default() );
    GdkDevice* pointer = gdk_device_manager_get_client_pointer( manager);
*/
	if (!priv->is_button_down)
		return FALSE;

	if (priv->is_dragging_point) {
		osm_gps_map_convert_screen_to_geographic(map, event->x, event->y, priv->drag_point);
		osm_gps_map_map_redraw_idle(map);
		return FALSE;
	}

	if (event->is_hint)
		gdk_window_get_pointer(event->window, &x, &y, &state);

	else {
		x = event->x;
		y = event->y;
		state = event->state;
	}

	/* are we being dragged */
	if (!(state & GDK_BUTTON1_MASK))
		return FALSE;

	if (priv->drag_counter < 0)
		return FALSE;

	/* not yet dragged far enough? */
	if (!priv->drag_counter &&
	    ((x - priv->drag_start_mouse_x) * (x - priv->drag_start_mouse_x) +
	     (y - priv->drag_start_mouse_y) * (y - priv->drag_start_mouse_y) < priv->drag_limit * priv->drag_limit))
		return FALSE;

	priv->drag_counter++;

	priv->is_dragging = TRUE;

	if (priv->map_auto_center_enabled)
		g_object_set(G_OBJECT(widget), "auto-center", FALSE, NULL);

	priv->drag_mouse_dx = x - priv->drag_start_mouse_x;
	priv->drag_mouse_dy = y - priv->drag_start_mouse_y;

	/* instead of redrawing directly just add an idle function */
	if (!priv->drag_expose_source)
		priv->drag_expose_source = g_idle_add((GSourceFunc) osm_gps_map_idle_expose, widget);

	return FALSE;
}

static gboolean
osm_gps_map_configure(GtkWidget * widget, GdkEventConfigure * event) {
	int w, h;
	GtkAllocation all;
	GdkWindow *window;
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;

	if (priv->pixmap)
		cairo_surface_destroy(priv->pixmap);

	gtk_widget_get_allocation(widget, &all);
	w = all.width;
	h = all.height;	
	window = gtk_widget_get_window(widget);

	priv->pixmap = gdk_window_create_similar_surface(window,
							 CAIRO_CONTENT_COLOR,
							 w + EXTRA_BORDER * 2, h + EXTRA_BORDER * 2);

	gint pixel_x = lon2pixel(priv->map_zoom, priv->center_rlon);
	gint pixel_y = lat2pixel(priv->map_zoom, priv->center_rlat);

	priv->map_x = pixel_x - w / 2;
	priv->map_y = pixel_y - h / 2;

	osm_gps_map_map_redraw(OSM_GPS_MAP(widget));

	g_signal_emit_by_name(widget, "changed");

	return FALSE;
}

static gboolean
osm_gps_map_draw(GtkWidget * widget, cairo_t * cr) {
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;

	if (!priv->drag_mouse_dx && !priv->drag_mouse_dy) {
		cairo_set_source_surface(cr, priv->pixmap, 0, 0);
	} else {
		cairo_set_source_surface(cr, priv->pixmap,
					 priv->drag_mouse_dx - EXTRA_BORDER,
					 priv->drag_mouse_dy - EXTRA_BORDER);
	}

	cairo_paint(cr);

	return FALSE;
}

static gboolean
osm_gps_map_expose(GtkWidget * widget, GdkEventExpose * event) {
	OsmGpsMap *map = OSM_GPS_MAP(widget);
	OsmGpsMapPrivate *priv = map->priv;
	GdkWindow *window = gtk_widget_get_window(widget);
	cairo_t *cr = gdk_cairo_create(window);
	gboolean ret;

/* TODO
	gdk_cairo_set_source_pixmap (cr, child->window, child->allocation.x, child->allocation.y);
	region = gdk_region_rectangle (&child->allocation);
	gdk_region_intersect (region, event->region);
	gdk_cairo_region (cr, region);
	cairo_clip (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_paint_with_alpha (cr, 0.5);
*/

	ret = osm_gps_map_draw(widget, cr);

/*	cairo_select_font_face(cr, "fixed", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL); */
	if (priv->repo_attrib) {
		cairo_text_extents_t ext;

		cairo_set_font_size(cr, 10);
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
		cairo_text_extents(cr, priv->repo_attrib, &ext);
		cairo_move_to(cr, widget->allocation.width - ext.width - 4, widget->allocation.height - 4);
		cairo_show_text(cr, priv->repo_attrib);
	}

	cairo_destroy(cr);
	return ret;
}

static void
osm_gps_map_class_init(OsmGpsMapClass * klass) {
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private(klass, sizeof(OsmGpsMapPrivate));

	object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = osm_gps_map_dispose;
	object_class->finalize = osm_gps_map_finalize;
	object_class->constructor = osm_gps_map_constructor;
	object_class->set_property = osm_gps_map_set_property;
	object_class->get_property = osm_gps_map_get_property;

	widget_class = GTK_WIDGET_CLASS(klass);
	widget_class->expose_event = osm_gps_map_expose;
	widget_class->configure_event = osm_gps_map_configure;
	widget_class->button_press_event = osm_gps_map_button_press;
	widget_class->button_release_event = osm_gps_map_button_release;
	widget_class->motion_notify_event = osm_gps_map_motion_notify;
	widget_class->scroll_event = osm_gps_map_scroll_event;

	/* default implementation of draw_gps_point */

	g_object_class_install_property(object_class,
		PROP_AUTO_CENTER,
		g_param_spec_boolean(
			"auto-center",
			"auto center",
			"map auto center",
			TRUE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_AUTO_CENTER_THRESHOLD,
		g_param_spec_double(
			"auto-center-threshold",
			"auto center threshold",
			"the amount of the window the gps point must move before auto centering",
			0.0,	/* minimum property value */
			1.0,	/* maximum property value */
			0.25,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_RECORD_TRIP_HISTORY,
		g_param_spec_boolean(
			"record-trip-history",
			"record trip history",
			"should all gps points be recorded in a trip history",
			TRUE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_SHOW_TRIP_HISTORY,
		g_param_spec_boolean(
			"show-trip-history",
			"show trip history",
			"should the recorded trip history be shown on the map",
			TRUE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_SHOW_HOME,
		g_param_spec_boolean(
			"show-home",
			"show home location",
			"draw red dot at home location",
			TRUE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_AUTO_DOWNLOAD,
		g_param_spec_boolean(
			"auto-download",
			"auto download",
			"map auto download",
			TRUE,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_REPO_URI,
		g_param_spec_string(
			"repo-uri",
			"repo uri",
			"Map source tile repository uri",
			NULL,
			G_PARAM_READABLE | G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_PROXY_URI,
		g_param_spec_string(
			"proxy-uri",
			"proxy uri",
			"HTTP proxy uri or NULL",
			NULL,
			G_PARAM_READABLE | G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_TILE_CACHE_BASE_DIR,
		g_param_spec_string(
			"tile-cache-base",
			"tile cache-base",
			"Base directory to which friendly and auto paths are appended",
			NULL,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_ZOOM,
		g_param_spec_int(
			"zoom",
			"zoom",
			"Map zoom level",
			MIN_ZOOM,	/* minimum property value */
			MAX_ZOOM,	/* maximum property value */
			3,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_TILE_ZOOM_OFFSET,
		g_param_spec_int(
			"tile-zoom-offset",
			"tile zoom-offset",
			"Number of zoom-levels to upsample tiles",
			MIN_TILE_ZOOM_OFFSET,	/* minimum propery value */
			MAX_TILE_ZOOM_OFFSET,	/* maximum propery value */
			0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_MAX_ZOOM,
		g_param_spec_int(
			"max-zoom",
			"max zoom",
			"Maximum zoom level",
			MIN_ZOOM, /* minimum property value */
			MAX_ZOOM, /* maximum property value */
			15,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_MIN_ZOOM,
		g_param_spec_int(
			"min-zoom",
			"min zoom",
			"Minimum zoom level",
			MIN_ZOOM,	/* minimum property value */
			MAX_ZOOM,	/* maximum property value */
			1,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_HOME_LAT,
		g_param_spec_double(
			"home-lat",
			"home lat",
			"Home latitude in degrees",
			-90.0, /* minimum property value */
			 90.0, /* maximum property value */
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_HOME_LON,
		g_param_spec_double(
			"home-lon",
			"home lon", "Home longitude in degrees",
			-180.0, /* minimum property value */
			 180.0, /* maximum property value */
			0.0,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_LATITUDE,
		g_param_spec_double(
			"latitude",
			"latitude",
			"Latitude in degrees",
			-90.0,	/* minimum property value */
			90.0,	/* maximum property value */
			0,
			G_PARAM_READABLE));

	g_object_class_install_property(object_class,
		PROP_LONGITUDE,
		g_param_spec_double(
			"longitude",
			"longitude",
			"Longitude in degrees",
			-180.0,	/* minimum property value */
			180.0,	/* maximum property value */
			0,
			G_PARAM_READABLE));

	g_object_class_install_property(object_class,
		PROP_MAP_X,
		g_param_spec_int(
			"map-x",
			"map-x",
			"Initial map x location",
			G_MININT,	/* minimum property value */
			G_MAXINT,	/* maximum property value */
			890,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_MAP_Y,
		g_param_spec_int(
			"map-y",
			"map-y",
			"Initial map y location",
			G_MININT,	/* minimum property value */
			G_MAXINT,	/* maximum property value */
			515,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_TILES_QUEUED,
		g_param_spec_int(
			"tiles-queued",
			"tiles-queued", 
			"The number of tiles currently waiting to download",
			G_MININT,	/* minimum property value */
			G_MAXINT,	/* maximum property value */
			0,
			G_PARAM_READABLE));

	g_object_class_install_property(object_class,
		PROP_MAP_SOURCE,
		g_param_spec_int(
			"map-source",
			"map source",
			"The map source ID",
			-1,       /* minimum property value */
			G_MAXINT, /* maximum property value */
			-1,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property(object_class,
		PROP_IMAGE_FORMAT,
		g_param_spec_string("image-format",
			"image format",
			"The map source tile repository image format (jpg, png)",
			"png",
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
		PROP_DRAG_LIMIT,
		g_param_spec_int(
			"drag-limit",
			"drag limit",
			"The number of pixels the user has to move the pointer in order to start dragging",
			0,        /* minimum property value */
			G_MAXINT, /* maximum property value */
			10,
			G_PARAM_READABLE |
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_signal_new("changed", OSM_TYPE_GPS_MAP,
		     G_SIGNAL_RUN_FIRST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

void
osm_gps_map_get_bbox(OsmGpsMap * map, OsmGpsMapPoint * pt1, OsmGpsMapPoint * pt2) {
	GtkAllocation allocation;
	OsmGpsMapPrivate *priv = map->priv;

	if (pt1 && pt2) {
		gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
		pt1->rlat = pixel2lat(priv->map_zoom, priv->map_y);
		pt1->rlon = pixel2lon(priv->map_zoom, priv->map_x);
		pt2->rlat = pixel2lat(priv->map_zoom, priv->map_y + allocation.height);
		pt2->rlon = pixel2lon(priv->map_zoom, priv->map_x + allocation.width);
	}
}

void
osm_gps_map_zoom_fit_bbox(OsmGpsMap * map, double latitude1, double latitude2, double longitude1, double longitude2) {
	GtkAllocation allocation;
	int zoom;
	gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
	zoom =
	 latlon2zoom(allocation.height, allocation.width, deg2rad(latitude1), deg2rad(latitude2), deg2rad(longitude1),
		     deg2rad(longitude2));
	osm_gps_map_set_center(map, (latitude1 + latitude2) / 2, (longitude1 + longitude2) / 2);
	osm_gps_map_set_zoom(map, zoom);
}

void
osm_gps_map_set_center_and_zoom(OsmGpsMap * map, double latitude, double longitude, int zoom) {
	osm_gps_map_set_center(map, latitude, longitude);
	osm_gps_map_set_zoom(map, zoom);
}

void
osm_gps_map_set_center(OsmGpsMap * map, double latitude, double longitude) {
	int pixel_x, pixel_y;
	OsmGpsMapPrivate *priv;
	GtkAllocation allocation;

	g_return_if_fail(OSM_IS_GPS_MAP(map));

	priv = map->priv;
	gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
	g_object_set(G_OBJECT(map), "auto-center", FALSE, NULL);

	priv->center_rlat = deg2rad(latitude);
	priv->center_rlon = deg2rad(longitude);

	pixel_x = lon2pixel(priv->map_zoom, priv->center_rlon);
	pixel_y = lat2pixel(priv->map_zoom, priv->center_rlat);

	priv->map_x = pixel_x - allocation.width / 2;
	priv->map_y = pixel_y - allocation.height / 2;

	osm_gps_map_map_redraw_idle(map);

	g_signal_emit_by_name(map, "changed");
}

void
osm_gps_map_set_zoom_offset(OsmGpsMap * map, int zoom_offset) {
	OsmGpsMapPrivate *priv;

	g_return_if_fail(OSM_GPS_MAP(map));
	priv = map->priv;

	if (zoom_offset != priv->tile_zoom_offset) {
		priv->tile_zoom_offset = zoom_offset;
		osm_gps_map_map_redraw_idle(map);
	}
}

int
osm_gps_map_set_zoom(OsmGpsMap * map, int zoom) {
	int width_center, height_center;
	OsmGpsMapPrivate *priv;
	GtkAllocation allocation;

	g_return_val_if_fail(OSM_IS_GPS_MAP(map), 0);
	priv = map->priv;

	if (zoom != priv->map_zoom) {
		gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
		width_center = allocation.width / 2;
		height_center = allocation.height / 2;

		/* update zoom but constrain [min_zoom..max_zoom] */
		priv->map_zoom = CLAMP(zoom, priv->min_zoom, priv->max_zoom);
		priv->map_x = lon2pixel(priv->map_zoom, priv->center_rlon) - width_center;
		priv->map_y = lat2pixel(priv->map_zoom, priv->center_rlat) - height_center;

		osm_gps_map_map_redraw_idle(map);

		g_signal_emit_by_name(map, "changed");
		g_object_notify(G_OBJECT(map), "zoom");
	}
	return priv->map_zoom;
}

int
osm_gps_map_zoom_in(OsmGpsMap * map) {
	g_return_val_if_fail(OSM_IS_GPS_MAP(map), 0);
	return osm_gps_map_set_zoom(map, map->priv->map_zoom + 1);
}

int
osm_gps_map_zoom_out(OsmGpsMap * map) {
	g_return_val_if_fail(OSM_IS_GPS_MAP(map), 0);
	return osm_gps_map_set_zoom(map, map->priv->map_zoom - 1);
}

GtkWidget *
osm_gps_map_new(void) {
	return g_object_new(OSM_TYPE_GPS_MAP, NULL);
}

void
osm_gps_map_scroll(OsmGpsMap * map, gint dx, gint dy) {
	OsmGpsMapPrivate *priv;

	g_return_if_fail(OSM_IS_GPS_MAP(map));
	priv = map->priv;

	priv->map_x += dx;
	priv->map_y += dy;
	center_coord_update(map);

	osm_gps_map_map_redraw_idle(map);
}

/*
 * Returns the scale of the map at the center, in meters/pixel.
 */
double
osm_gps_map_get_scale(OsmGpsMap * map) {
	OsmGpsMapPrivate *priv;

	g_return_val_if_fail(OSM_IS_GPS_MAP(map), OSM_GPS_MAP_INVALID);
	priv = map->priv;

	return osm_gps_map_get_scale_at_point(priv->map_zoom, priv->center_rlat, priv->center_rlon);
}

gchar *
osm_gps_map_get_default_cache_directory(void) {
	return g_build_filename(g_get_user_cache_dir(), "osmgpsmap", NULL);
}

#if 0
void
osm_gps_map_set_keyboard_shortcut(OsmGpsMap * map, OsmGpsMapKey_t key, guint keyval) {
	g_return_if_fail(OSM_IS_GPS_MAP(map));
	g_return_if_fail(key < OSM_GPS_MAP_KEY_MAX);

	map->priv->keybindings[key] = keyval;
	map->priv->keybindings_enabled = TRUE;
}
#endif
void
osm_gps_map_track_add(OsmGpsMap * map, OsmGpsMapTrack * track) {
	OsmGpsMapPrivate *priv;

	g_return_if_fail(OSM_IS_GPS_MAP(map));
	priv = map->priv;

	g_object_ref(track);
	g_signal_connect(track, "point-added", G_CALLBACK(on_gps_point_added), map);
	g_signal_connect(track, "notify", G_CALLBACK(on_track_changed), map);

	priv->tracks = g_slist_append(priv->tracks, track);
	osm_gps_map_map_redraw_idle(map);
}

void
osm_gps_map_track_remove_all(OsmGpsMap * map) {
	g_return_if_fail(OSM_IS_GPS_MAP(map));

	gslist_of_gobjects_free(&map->priv->tracks);
	osm_gps_map_map_redraw_idle(map);
}

gboolean
osm_gps_map_track_remove(OsmGpsMap * map, OsmGpsMapTrack * track) {
	GSList *data;

	g_return_val_if_fail(OSM_IS_GPS_MAP(map), FALSE);
	g_return_val_if_fail(track != NULL, FALSE);

	data = gslist_remove_one_gobject(&map->priv->tracks, G_OBJECT(track));
	osm_gps_map_map_redraw_idle(map);
	return data != NULL;
}

void
osm_gps_map_convert_screen_to_geographic(OsmGpsMap * map, gint pixel_x, gint pixel_y, OsmGpsMapPoint * pt) {
	OsmGpsMapPrivate *priv;
	int map_x0, map_y0;

	g_return_if_fail(OSM_IS_GPS_MAP(map));
	g_return_if_fail(pt);

	priv = map->priv;
	map_x0 = priv->map_x - EXTRA_BORDER;
	map_y0 = priv->map_y - EXTRA_BORDER;

	pt->rlat = pixel2lat(priv->map_zoom, map_y0 + pixel_y);
	pt->rlon = pixel2lon(priv->map_zoom, map_x0 + pixel_x);
}

void
osm_gps_map_convert_geographic_to_screen(OsmGpsMap * map, OsmGpsMapPoint * pt, gint * pixel_x, gint * pixel_y) {
	OsmGpsMapPrivate *priv;
	int map_x0, map_y0;

	g_return_if_fail(OSM_IS_GPS_MAP(map));
	g_return_if_fail(pt);

	priv = map->priv;
	map_x0 = priv->map_x - EXTRA_BORDER;
	map_y0 = priv->map_y - EXTRA_BORDER;

	if (pixel_x)
		*pixel_x = lon2pixel(priv->map_zoom, pt->rlon) - map_x0 + priv->drag_mouse_dx;
	if (pixel_y)
		*pixel_y = lat2pixel(priv->map_zoom, pt->rlat) - map_y0 + priv->drag_mouse_dy;
}

OsmGpsMapPoint *
osm_gps_map_get_event_location(OsmGpsMap * map, GdkEventButton * event) {
	OsmGpsMapPoint *p = osm_gps_map_point_new_degrees(0.0, 0.0);
	osm_gps_map_convert_screen_to_geographic(map, event->x, event->y, p);
	return p;
}

void
osm_gps_map_set_map_source(OsmGpsMap *map, gint src) {
	gint old;

	g_return_if_fail(OSM_IS_GPS_MAP(map));

	old = map->priv->map_source;
	if (old != src && src < sizeof(sources) / sizeof(sources[0])) {
		map->priv->map_source = src;
		osm_gps_map_setup(map);
	}
}

gint
osm_gps_map_get_map_source(OsmGpsMap *map) {
	g_return_val_if_fail(OSM_IS_GPS_MAP(map), -1);
	return map->priv->map_source;
}

void
osm_gps_map_move(OsmGpsMap *map, int dx, int dy) {
	g_return_if_fail(OSM_IS_GPS_MAP(map));

	map->priv->map_y += dy;
	map->priv->map_x += dx;
	center_coord_update(map);
	osm_gps_map_map_redraw_idle(map);
}

void
osm_gps_map_set_show_home(OsmGpsMap *map, gboolean sh) {
	g_return_if_fail(OSM_IS_GPS_MAP(map));
	map->priv->show_home = sh;
	osm_gps_map_map_redraw(map);
}

gboolean
osm_gps_map_get_show_home(OsmGpsMap *map) {
	g_return_val_if_fail(OSM_IS_GPS_MAP(map), FALSE);
	return map->priv->show_home;
}
