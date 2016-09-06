/*
 * Copyright © Lars Lindqvist 2016 <lars.lindqvist@yandex.ru>
 * Copyright © John Stowers 2013 <john.stowers@gmail.com>
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

#ifndef _MS_MAP_H
#define _MS_MAP_H

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

#define OSM_TYPE_GPS_MAP_POINT  osm_gps_map_point_get_type()
typedef struct _OsmGpsMapPoint OsmGpsMapPoint;

struct _OsmGpsMapPoint {
	/* radians */
	double rlat;
	double rlon;
};

GType osm_gps_map_point_get_type(void)  G_GNUC_CONST;

OsmGpsMapPoint *osm_gps_map_point_new_degrees(double lat, double lon);
OsmGpsMapPoint *osm_gps_map_point_new_radians(double rlat, double rlon);
void osm_gps_map_point_get_degrees(OsmGpsMapPoint * point, double *lat, double *lon);
void osm_gps_map_point_get_radians(OsmGpsMapPoint * point, double *rlat, double *rlon);
void osm_gps_map_point_set_degrees(OsmGpsMapPoint * point, double lat, double lon);
void osm_gps_map_point_set_radians(OsmGpsMapPoint * point, double rlat, double rlon);
void osm_gps_map_point_free(OsmGpsMapPoint * point);
OsmGpsMapPoint *osm_gps_map_point_copy(const OsmGpsMapPoint * point);

double deg2rad(double deg);
double rad2deg(double rad);
int lat2pixel(int zoom, double lat);
int lon2pixel(int zoom, double lon);
double pixel2lon(double zoom, int pixel_x);
double pixel2lat(double zoom, int pixel_y);
int latlon2zoom(int pix_height, int pix_width, double lat1, double lat2, double lon1, double lon2);

#define OSM_TYPE_GPS_MAP_TRACK              osm_gps_map_track_get_type()
#define OSM_GPS_MAP_TRACK(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSM_TYPE_GPS_MAP_TRACK, OsmGpsMapTrack))
#define OSM_GPS_MAP_TRACK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), OSM_TYPE_GPS_MAP_TRACK, OsmGpsMapTrackClass))
#define OSM_IS_GPS_MAP_TRACK(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSM_TYPE_GPS_MAP_TRACK))
#define OSM_IS_GPS_MAP_TRACK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), OSM_TYPE_GPS_MAP_TRACK))
#define OSM_GPS_MAP_TRACK_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), OSM_TYPE_GPS_MAP_TRACK, OsmGpsMapTrackClass))
typedef struct _OsmGpsMapTrack OsmGpsMapTrack;
typedef struct _OsmGpsMapTrackClass OsmGpsMapTrackClass;
typedef struct _OsmGpsMapTrackPrivate OsmGpsMapTrackPrivate;

struct _OsmGpsMapTrack {
	GObject parent;

	OsmGpsMapTrackPrivate *priv;
};

struct _OsmGpsMapTrackClass {
	GObjectClass parent_class;
};

GType osm_gps_map_track_get_type(void) G_GNUC_CONST;

OsmGpsMapTrack *osm_gps_map_track_new(void);
void osm_gps_map_track_add_point(OsmGpsMapTrack * track, const OsmGpsMapPoint * point);
GSList *osm_gps_map_track_get_points(OsmGpsMapTrack * track);
void osm_gps_map_track_set_color(OsmGpsMapTrack * track, GdkColor * color);
void osm_gps_map_track_get_color(OsmGpsMapTrack * track, GdkColor * color);
void osm_gps_map_track_remove_point(OsmGpsMapTrack * track, int pos);
int  osm_gps_map_track_n_points(OsmGpsMapTrack * track);
void osm_gps_map_track_insert_point(OsmGpsMapTrack * track, OsmGpsMapPoint * np, int pos);
OsmGpsMapPoint *osm_gps_map_track_get_point(OsmGpsMapTrack * track, int pos);
double osm_gps_map_track_get_length(OsmGpsMapTrack * track);

void     osm_gps_map_track_set_visible(OsmGpsMapTrack *track, gboolean);
gboolean osm_gps_map_track_get_visible(OsmGpsMapTrack *track);
void     osm_gps_map_track_set_continuous(OsmGpsMapTrack * track, gboolean continuous);
gboolean osm_gps_map_track_get_continuous(OsmGpsMapTrack * track);
void     osm_gps_map_track_set_linewidth(OsmGpsMapTrack * track, gdouble linewidth);
gdouble  osm_gps_map_track_get_linewidth(OsmGpsMapTrack * track);
void     osm_gps_map_track_set_heading(OsmGpsMapTrack * track, gdouble heading);
gdouble  osm_gps_map_track_get_heading(OsmGpsMapTrack * track);



#define OSM_TYPE_GPS_MAP             (osm_gps_map_get_type ())
#define OSM_GPS_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OSM_TYPE_GPS_MAP, OsmGpsMap))
#define OSM_GPS_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), OSM_TYPE_GPS_MAP, OsmGpsMapClass))
#define OSM_IS_GPS_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OSM_TYPE_GPS_MAP))
#define OSM_IS_GPS_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), OSM_TYPE_GPS_MAP))
#define OSM_GPS_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), OSM_TYPE_GPS_MAP, OsmGpsMapClass))
typedef struct _OsmGpsMapClass OsmGpsMapClass;
typedef struct _OsmGpsMap OsmGpsMap;
typedef struct _OsmGpsMapPrivate OsmGpsMapPrivate;

struct _OsmGpsMapClass {
	GtkDrawingAreaClass parent_class;

	void (*draw_gps_point) (OsmGpsMap * map, cairo_t * cr);
};

struct _OsmGpsMap {
	GtkDrawingArea parent_instance;
	OsmGpsMapPrivate *priv;
};

typedef enum {
	OSM_GPS_MAP_KEY_FULLSCREEN,
	OSM_GPS_MAP_KEY_ZOOMIN,
	OSM_GPS_MAP_KEY_ZOOMOUT,
	OSM_GPS_MAP_KEY_UP,
	OSM_GPS_MAP_KEY_DOWN,
	OSM_GPS_MAP_KEY_LEFT,
	OSM_GPS_MAP_KEY_RIGHT,
	OSM_GPS_MAP_KEY_MAX
} OsmGpsMapKey_t;

#define OSM_GPS_MAP_INVALID         (0.0/0.0)

GType osm_gps_map_get_type(void) G_GNUC_CONST;
GtkWidget *osm_gps_map_new(void);
gchar *osm_gps_map_get_default_cache_directory(void);

void osm_gps_map_download_maps(OsmGpsMap * map, OsmGpsMapPoint * pt1, OsmGpsMapPoint * pt2, int zoom_start, int zoom_end);
void osm_gps_map_download_cancel_all(OsmGpsMap * map);
void osm_gps_map_get_bbox(OsmGpsMap * map, OsmGpsMapPoint * pt1, OsmGpsMapPoint * pt2);
void osm_gps_map_zoom_fit_bbox(OsmGpsMap * map, double latitude1, double latitude2, double longitude1, double longitude2);
void osm_gps_map_set_center_and_zoom(OsmGpsMap * map, double latitude, double longitude, int zoom);
void osm_gps_map_set_center(OsmGpsMap * map, double latitude, double longitude);
int  osm_gps_map_set_zoom(OsmGpsMap * map, int zoom);
void osm_gps_map_set_zoom_offset(OsmGpsMap * map, int zoom_offset);
int  osm_gps_map_zoom_in(OsmGpsMap * map);
int  osm_gps_map_zoom_out(OsmGpsMap * map);
void osm_gps_map_scroll(OsmGpsMap * map, gint dx, gint dy);
double osm_gps_map_get_scale(OsmGpsMap * map);
void osm_gps_map_set_keyboard_shortcut(OsmGpsMap * map, OsmGpsMapKey_t key, guint keyval);
void osm_gps_map_track_add(OsmGpsMap * map, OsmGpsMapTrack * track);
void osm_gps_map_track_remove_all(OsmGpsMap * map);
gboolean osm_gps_map_track_remove(OsmGpsMap * map, OsmGpsMapTrack * track);
void osm_gps_map_gps_add(OsmGpsMap * map, double latitude, double longitude, double heading);
void osm_gps_map_gps_clear(OsmGpsMap * map);
OsmGpsMapTrack *osm_gps_map_gps_get_track(OsmGpsMap * map);
void osm_gps_map_convert_screen_to_geographic(OsmGpsMap * map, gint pixel_x, gint pixel_y, OsmGpsMapPoint * pt);
void osm_gps_map_convert_geographic_to_screen(OsmGpsMap * map, OsmGpsMapPoint * pt, gint * pixel_x, gint * pixel_y);
OsmGpsMapPoint *osm_gps_map_get_event_location(OsmGpsMap * map, GdkEventButton * event);
gboolean osm_gps_map_map_redraw(OsmGpsMap * map);
void     osm_gps_map_map_redraw_idle(OsmGpsMap * map);
void     osm_gps_map_set_map_source(OsmGpsMap*, gint);
gint     osm_gps_map_get_map_source(OsmGpsMap*);
void     osm_gps_map_move(OsmGpsMap *map, int dx, int dy);
void     osm_gps_map_set_show_home(OsmGpsMap *map, gboolean sh);
gboolean osm_gps_map_get_show_home(OsmGpsMap *map);
#endif
