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


#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <poll.h>
#include <gdk/gdkkeysyms.h>

#include <arg.h>

#include "map.h"
#include "compass.h"
#include "config.h"
#include "histogram.h"
#include "aircraft.h"
#include "message.h"
#include "util.h"
#include "parse.h"
#include "sources.h"

static int allocs = 0;
static int frees = 0;

static struct {
	OsmGpsMap *map;
	GtkWidget *win;
	GtkWidget *sb;
	GtkTreeModelFilter *f;
	GtkListStore *store;
	GtkTreeView *treeview;
	GtkTextBuffer *tbuf;

	bool show_list;
	GtkWidget *list;
	GtkWidget *text;

	int  map_source;
	bool new_source;

	struct {
		GtkTreeViewColumn *flag;
		GtkTreeViewColumn *addr;
		GtkTreeViewColumn *name;
		GtkTreeViewColumn *id;
		GtkTreeViewColumn *alt;
		GtkTreeViewColumn *vel;
		GtkTreeViewColumn *head;
		GtkTreeViewColumn *vert;
		GtkTreeViewColumn *msgs;
		GtkTreeViewColumn *seen;
	} column;


	double scale;

	struct ms_aircraft_t *sel;

	struct {
		int seen_list;
		int seen_active;
		int msgs_list;
		bool plot_inactive;
		bool plot_previous;
	} filter;

	struct {
		GtkFrame *frame;
		GtkLabel *reg;
		GtkLabel *vrate;
		GtkLabel *coord;
		GtkLabel *squawk;
		GtkLabel *vel;
		GtkLabel *heading;
		GtkLabel *msgs;
		GtkLabel *seen;
	} info;

	int message_cache;
	bool show_home;
	double home_lat;
	double home_lon;
} gui;

struct ms_aircraft_t *aircrafts = NULL;

enum {
	COL_SEL_CURR = 0,
	COL_SEL_PREV,
	COL_ACT_CURR,
	COL_ACT_PREV,
	COL_OLD_CURR,
	COL_OLD_PREV,
	COL_COUNT
};

static GdkColor colors[COL_COUNT];

GtkWidget *
mk_sane_label(const char *text, GtkLabel **set, gfloat align) {
	GtkWidget *alignment = gtk_alignment_new(align, 0, 0, 0);
	GtkWidget *label = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(alignment), label);
	gtk_label_set_markup(GTK_LABEL(label), text);
	if (set)
		*set = GTK_LABEL(label);
	return alignment;
}

static void
update_info(const struct ms_aircraft_t *a) {
	char text[256];
	int i = 0;

	{
		if (a)
			snprintf(text, sizeof(text) - 1, "%s:%06X", a->nation->iso3, a->addr);
		else
			snprintf(text, sizeof(text) - 1, "info");
		gtk_frame_set_label(gui.info.frame, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Registration: ");
		if (a) {
			if (a->name[0])
				i += snprintf(text + i, sizeof(text) - 1 - i, "%s / ", a->name);
			snprintf(text + i, sizeof(text) - 1 - i, "%s", a->nation->name);
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.reg, text);
	}{
		if (a && a->velocities.last) {
			int vr = a->velocities.last->vrate;
			int a = vr < 0 ? -vr : vr;
			
			snprintf(text, sizeof(text) - 1,
			         "Rate of %s: %'d ft/min (%'.0f m/s)",
			         vr > 0 ? "climb" : "descent", a, 0.00508 * a);
		} else {
			snprintf(text, sizeof(text) - 1, "Vertical rate: N/A");
		}
		gtk_label_set_text(gui.info.vrate, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Coördinates: ");
		if (a && a->locations.last) {
			double lon = a->locations.last->lon;
			double lat = a->locations.last->lat;
			i += fill_angle_str(text + i, sizeof(text) - 1 - i, lat);
			text[i++] = lat < 0 ? 'S' : 'N';
			text[i++] = ' ';
			i += fill_angle_str(text + i, sizeof(text) - 1 - i, lon);
			text[i++] = lon < 0 ? 'E' : 'W';
			text[i++] = '\0';
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.coord, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Speed: ");
		if (a && a->velocities.last) {
			double vel = a->velocities.last->speed;
			snprintf(text + i, sizeof(text) - 1 - i,
			         "%'.0f kt (%'.0f km/h)",
			         vel, KT2KMPH(vel));
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.vel, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Heading: ");
		if (a && a->velocities.last) {
			double h = a->velocities.last->track;
			i += fill_angle_str(text + i, sizeof(text) - 1 - i, h);
			snprintf(text + i, sizeof(text) - 1 - i,
			         " (%s)", get_comp_point(h));
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.heading, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Squawk: ");
		if (a && a->squawks.last) {
			uint16_t ID = a->squawks.last->ID;
			snprintf(text + i, sizeof(text) - 1 - i,
			         "%04o %s", ID, get_ID_desc(ID));
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.squawk, text);
	}{
		i = snprintf(text, sizeof(text) - 1, "Messages: ");
		if (a) {
			i += snprintf(text + i, sizeof(text) - 1 - i, "%'d", a->n_messages);
		} else {
			snprintf(text + i, sizeof(text) - 1 - i, "N/A");
		}
		gtk_label_set_text(gui.info.msgs, text);
	}{
		if (a) {
			struct tm *tmp = localtime(&a->last_seen);
			strftime(text, sizeof(text) - 1, "%Y-%m-%d %H:%M:%S", tmp);
			gtk_label_set_text(gui.info.seen, text);
		} else {
			gtk_label_set_text(gui.info.seen, NULL);
		}
	}

		


}

#define GPL3URL "https://www.gnu.org/licenses/gpl-3.0.html"
#define GPL2URL "https://www.gnu.org/licenses/gpl-2.0.html"
#define OGMURL "https://nzjrs.github.io/osm-gps-map/"

static const char msdec_line[] = "Mode S decode © Lars Lindqvist 2016 <a href=\"" GPL3URL "\">GPLv3</a>";
static const char osmgm_line[] =  "Map widget based on <a href=\""OGMURL"\">osm-gps-map</a> © John Stowers &amp; al. <a href=\"" GPL2URL "\">GPLv2+</a>";

GtkWidget *
mk_info() {
	GtkWidget *frame     = gtk_frame_new("msdec");
	GtkWidget *container = gtk_table_new(10, 3, FALSE);

	const char *map_att1 = sources[gui.map_source].attrib_full1;
	const char *map_att2 = sources[gui.map_source].attrib_full2;

	GtkWidget *reg_lbl    = mk_sane_label(msdec_line, &gui.info.reg,     0);
	GtkWidget *vrate_lbl  = mk_sane_label("",         &gui.info.vrate,   0);
	GtkWidget *vel_lbl    = mk_sane_label(osmgm_line, &gui.info.vel,     0);
	GtkWidget *track_lbl  = mk_sane_label("",         &gui.info.heading, 0);
	GtkWidget *squawk_lbl = mk_sane_label(map_att1,   &gui.info.squawk,  0);
	GtkWidget *msgs_lbl   = mk_sane_label("",         &gui.info.msgs,    0);
	GtkWidget *coord_lbl  = mk_sane_label(map_att2,   &gui.info.coord,   0);
	GtkWidget *seen_lbl   = mk_sane_label("",         &gui.info.seen,    1);

	int row = 0;

	gtk_table_attach(GTK_TABLE(container), reg_lbl,    0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), vrate_lbl,  0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), vel_lbl,    0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), track_lbl,  0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), squawk_lbl, 0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), coord_lbl,  0, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);++row;
	gtk_table_attach(GTK_TABLE(container), msgs_lbl,   0, 1, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);
	gtk_table_attach(GTK_TABLE(container), seen_lbl,   1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 3, 1);


	gui.info.frame = GTK_FRAME(frame);
	gtk_container_add(GTK_CONTAINER(frame), container);
	return frame;
}


static int
init_colors() {
	if (!gdk_color_parse(default_sel_curr_color, &colors[COL_SEL_CURR])
	 || !gdk_color_parse(default_sel_prev_color, &colors[COL_SEL_PREV])
	 || !gdk_color_parse(default_act_curr_color, &colors[COL_ACT_CURR])
	 || !gdk_color_parse(default_act_prev_color, &colors[COL_ACT_PREV])
	 || !gdk_color_parse(default_old_curr_color, &colors[COL_OLD_CURR])
	 || !gdk_color_parse(default_old_prev_color, &colors[COL_OLD_PREV]))
		return -1;
	return 0;
}

struct ms_ac_ext_t {
	bool listed;
	GtkTreeIter iter;
	struct ms_ac_location_t const *last_loc;
	struct ms_ac_track_t {
		time_t time;
		OsmGpsMapTrack *track;
		struct ms_ac_track_t *next;
	} *head;
};

static int
init_inotify(const char *filename, int *ifd, int *iwfd) {
	if ((*ifd = inotify_init()) < 0) {
		fprintf(stderr, "%s: ERROR: inotify_init: %s\n",
			argv0, strerror(errno));
		return -1;
	}
	if ((*iwfd = inotify_add_watch(*ifd, filename, IN_MODIFY
	                                             | IN_DELETE_SELF
	                                             | IN_MOVE_SELF
	                                             | IN_UNMOUNT)) < 0) {
		fprintf(stderr, "%s: ERROR: inotify_add_watch %s: %s\n",
			argv0, filename, strerror(errno));
		close(*ifd);
	}
	return 0;
}

enum {
	SC_FLAG,
	SC_ADDR,
	SC_NAME,
	SC_SQUAWK,
	SC_ALT,
	SC_SPEED,
	SC_TRACK,
	SC_VRATE,
	SC_MSGS,
	SC_SEEN,
	SC_STR_TRACK,
	SC_STR_SEEN,
	SC_PTR_AC,
	SC_OBJ_FLAG,
	SC_LAST
};

static void
update_store(struct ms_aircraft_t *a) {
	GtkTreeIter  *iter  = &((struct ms_ac_ext_t *)(a->ext))->iter;
	int prev_seen;
	gtk_tree_model_get(GTK_TREE_MODEL(gui.store), iter, SC_SEEN, &prev_seen, -1);
	
	gtk_list_store_set(gui.store, iter, SC_MSGS, a->n_messages, -1);

	if (a->name[0]) {
		gtk_list_store_set(gui.store, iter, SC_NAME, a->name, -1);
	}
	if (a->altitudes.last) {
		double alt = a->altitudes.last->alt;
		gtk_list_store_set(gui.store, iter, SC_ALT, lrint(alt), -1);
	}
	if (a->squawks.last) {
		char s_squawk[5];
		snprintf(s_squawk, 5, "%04o", a->squawks.last->ID);
		s_squawk[4] = 0;
		gtk_list_store_set(gui.store, iter, SC_SQUAWK, s_squawk, -1);
	}
	if (a->velocities.last) {
		const char *track_str = get_comp_point(a->velocities.last->track);
		gtk_list_store_set(gui.store, iter,
			SC_VRATE, lrint(a->velocities.last->vrate),
			SC_SPEED, lrint(a->velocities.last->speed),
			SC_TRACK, lrint(a->velocities.last->track),
			SC_STR_TRACK, track_str,
			-1);
	}
	if (prev_seen != a->last_seen) {
		char seen_str[16];
		struct tm *tmp = localtime(&a->last_seen);
		time_t age = time(NULL) - a->last_seen; 
		if (age < 60 * 60 * 24) {
			strftime(seen_str, 15, "%H:%M:%S", tmp);
		} else {
			strftime(seen_str, 15, "%y-%m-%d", tmp);
		}
		seen_str[15] = 0;
		gtk_list_store_set(gui.store, iter, SC_SEEN, a->last_seen, -1);
		gtk_list_store_set(gui.store, iter, SC_STR_SEEN, seen_str, -1);
	}

}

static void
add_to_store(struct ms_aircraft_t *a) {
	GtkTreeIter *iter = &((struct ms_ac_ext_t *)(a->ext))->iter;
	GdkPixbuf *flag = gdk_pixbuf_new_from_xpm_data((const char**)a->nation->flag);

	char s_addr[7];

	snprintf(s_addr, 7, "%06X", a->addr);
	s_addr[6] = 0;

	gtk_list_store_append(gui.store, iter);
	gtk_list_store_set(gui.store, iter,
		SC_ADDR, s_addr,
		SC_FLAG, a->nation->iso3,
		SC_OBJ_FLAG, flag,
		SC_PTR_AC, a,
		-1);

	g_object_unref(flag);

	update_store(a);
}


static void
set_trail(struct ms_aircraft_t *a, bool selected) {
	time_t seen;
	struct ms_ac_ext_t *ext;
	struct ms_ac_track_t *track;
	int cidx;
	double lw;
	bool listed;

	if (!a || !(ext = a->ext) || !ext->head)
		return;

	listed = ext->listed;
	seen = time(NULL) - a->last_seen;

	if (selected && !listed) {
		selected = false;
		gui.sel = NULL;
		update_info(NULL);
	}
		

	for (track = ext->head; track; track = track->next) {
		bool active = gui.filter.seen_active > seen;
		bool curr = track == ext->head;
		bool visible;
		double lw;
		int c;

		if (selected) {
			lw = 1.0;
			visible = true;
			c = curr ? COL_SEL_CURR : COL_SEL_PREV;
		}
		else if (active) {
			lw = 0.5;
			visible = listed;
			c = curr ? COL_ACT_CURR : COL_ACT_PREV;
		}
		else {
			lw = 0.5;
			visible = listed && gui.filter.plot_inactive;
			c = curr ? COL_OLD_CURR : COL_OLD_PREV;
		}

		if (!curr && !gui.filter.plot_previous)
			visible = false;

		osm_gps_map_track_set_visible(track->track, visible);
		osm_gps_map_track_set_linewidth(track->track, lw);
		osm_gps_map_track_set_color(track->track, &colors[c]);

	}
}


static void
add_loc_to_track(struct ms_aircraft_t *a, const struct ms_ac_location_t *loc) {
	struct ms_ac_ext_t *ext = a->ext;
	struct ms_ac_track_t *track;
	OsmGpsMapPoint p;
	
	if (!ext) {
		return;
	}

	if (ext->head && ext->head->time && loc->time - ext->head->time < 300) {
		track = ext->head;
	} else {
		allocs++;
		track = calloc(1, sizeof(struct ms_ac_track_t));
		track->track = g_object_new(OSM_TYPE_GPS_MAP_TRACK,
			"visible", false,
			"continuous", false,
			"line-width", 0.5,
			NULL);
		osm_gps_map_track_add(gui.map, track->track);
		set_trail(a, false);
		track->next = ext->head;
		ext->head = track;
	}

	p.rlat = deg2rad(loc->lat);
	p.rlon = deg2rad(loc->lon);
	osm_gps_map_track_add_point(track->track, &p);
	track->time = loc->time;
	ext->last_loc = loc;
}


static int
cat(const char *filename) {
	static off_t offset = 0;
	int err = 0;
	struct ms_msg_t *msgs, *msg;
	time_t now = time(NULL);
	int all = 0;
	int fre = 0;

	msgs = parse_file(filename, &offset, &aircrafts);

	if (!msgs && errno) {
		err -= 1;
		fprintf(stderr, "%s: Failed to parse file %s: %s\n",
			argv0, filename ? filename : "stdin", strerror(errno));
	}

	while (msgs) {
		struct ms_msg_t *tmp = msgs->next;
		struct ms_aircraft_t *a = msgs->aircraft;
		if (a) {
			struct ms_ac_ext_t *ext;
			struct ms_ac_track_t *track;
			const struct ms_ac_location_t *l;

			if (a->ext) {
				ext = a->ext;
				update_store(a);
			} else {
				ext = calloc(1, sizeof(struct ms_ac_ext_t));
				a->ext = ext;
				add_to_store(a);
			}

			if (a->locations.head) {
				if (!ext->last_loc)
					ext->last_loc = a->locations.head;

				for (l = ext->last_loc; l; l = l->next) {
					add_loc_to_track(a, l);
				}
				if (ext->head && ext->head->track && a->velocities.last) {
					double heading = a->velocities.last->track;
					osm_gps_map_track_set_heading(ext->head->track, heading);
				}
			}

			if (gui.message_cache && a->n_msg_aux > gui.message_cache && a->messages) {
				struct ms_msg_t *tmp;
				
				tmp = a->messages->ac_next;
				destroy_msg(a->messages);
				a->messages = tmp;
				--a->n_msg_aux;
			}
		}
		msgs = tmp;
	}

	osm_gps_map_map_redraw_idle(gui.map);

	return err ? -1 : 0;
}


gboolean
icb(GIOChannel *giofp, GIOCondition cond, gpointer data) {
	int fd = g_io_channel_unix_get_fd(giofp);
	char ib[4 * sizeof(struct inotify_event)];
	struct inotify_event *ev;
	ssize_t len, i;
	int ret;

	len = read(fd, ib, sizeof(ib));

	if (len < 0) {
		if (errno == EINTR || errno == EAGAIN) {
			return true;
		}
		return false;
	}

	for (i = 0; i < len; i += sizeof(struct inotify_event) + ev->len) {
		ev = (struct inotify_event *)(ib + i);
		if (ev->mask & ~IN_MODIFY) {
			return false;
		}
	}

	if (cat((char*)data) == -1)
		return false;
	else
		return true;
}

void
cleanup() {
	if (gui.store) {
		g_object_unref(gui.store);
	}

	while (aircrafts) {
		struct ms_aircraft_t *tmp;
		
		tmp = aircrafts->next;
		if (aircrafts->ext) {
			struct ms_ac_ext_t *ext = aircrafts->ext;
			while (ext->head) {
				frees++;
				struct ms_ac_track_t *tmp = ext->head->next;
				if (ext->head->track) {
					g_object_unref(ext->head->track);
				}
				ext->head = tmp;
			}
			free(aircrafts->ext);
		}
		destroy_aircraft(aircrafts);
		aircrafts = tmp;
	}
	gtk_main_quit();
}

void
cb_map_changed(void) {
	double lat, lon;
	long int scale;
	char msg[128];
	char *s;
	int zoom;
	int len;

	g_object_get(G_OBJECT(gui.map),
	             "zoom", &zoom,
	             "latitude", &lat,
	             "longitude", &lon,
	             NULL);

	len = 0;
	len += fill_angle_str(msg + len, 30, lat);
	msg[len++] = lat < 0 ? 'S' : 'N';
	msg[len++] = '\t';
	len += fill_angle_str(msg + len, 30, lon);
	msg[len++] = lon < 0 ? 'E' : 'W';
	msg[len++] = '\t';
	scale = lrint(gui.scale * cos(deg2rad(lat)) / pow(2, zoom));
	snprintf(msg + len, 128 - len, "Z=%d\t1:%'ld\t", zoom, scale);

	gtk_statusbar_push(GTK_STATUSBAR(gui.sb), 1, msg);
}

GtkWidget*
mk_ogm_map() {
	OsmGpsMap *map = g_object_new(OSM_TYPE_GPS_MAP,
		"map-source", gui.map_source,
		"home-lat", gui.home_lat,
		"home-lon", gui.home_lon,
		"show-home", gui.show_home,
		"max-zoom", 13,
		"min-zoom", 1,
		"zoom", default_zoom_level,
		NULL);

	osm_gps_map_set_center(map, gui.home_lat, gui.home_lon);
	g_signal_connect(map, "changed", cb_map_changed, NULL);

	gui.map = map;

	return GTK_WIDGET(map);
}

GtkWidget *
mk_map() {
	GtkWidget *container = gtk_vbox_new(FALSE, 0);
	GtkWidget *statusbar = gtk_statusbar_new();
	GtkWidget *ogm_map   = mk_ogm_map();

	gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), FALSE);
	gui.sb = statusbar;

	gtk_box_pack_start(GTK_BOX(container), ogm_map,   TRUE,  TRUE,  0);
	gtk_box_pack_start(GTK_BOX(container), statusbar, FALSE, FALSE, 0);

	return container;
}

static gboolean
visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	struct ms_aircraft_t *a;
	bool visible = true;
	int seen;
	int msgs;

	gtk_tree_model_get(model, iter, SC_SEEN, &seen, -1);
	gtk_tree_model_get(model, iter, SC_MSGS, &msgs, -1);
	gtk_tree_model_get(model, iter, SC_PTR_AC, &a, -1);

	seen = time(NULL) - seen;

	if (gui.filter.seen_list && seen > gui.filter.seen_list)
		visible = false;

	if (gui.filter.msgs_list && msgs < gui.filter.msgs_list)
		visible = false;

	if (a && a->ext) {
		struct ms_ac_ext_t *ext = a->ext;
		ext->listed = visible;
		
		set_trail(a, a == gui.sel);
	}

	return visible;
}

static void
cb_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	GtkTreeIter iter;
	
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		struct ms_aircraft_t *a;
		struct ms_ac_ext_t *ext;

		if (gui.sel) {
			set_trail(gui.sel, false);
		}

		gtk_tree_model_get(model, &iter, SC_PTR_AC, &a, -1);

		if (a) {
			gui.sel = a;
			update_info(a);
			set_trail(a, true);
		}

		osm_gps_map_map_redraw_idle(gui.map);
	}
}

static void
cb_mapsel(GtkWidget *w, gpointer user_data) {
	gui.map_source = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
	gui.new_source = true;
}

GtkWidget *
mk_mapsel() {
	GtkComboBoxText *drop = (GtkComboBoxText*)gtk_combo_box_text_new();
	int i;

	for (i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
		gtk_combo_box_text_append_text(drop, sources[i].name);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(drop), gui.map_source);

	g_signal_connect(drop, "changed", G_CALLBACK(cb_mapsel), NULL);
	return GTK_WIDGET(drop);
}

#if 0
GtkWidget *
mk_head(const char *title) {
	PangoFontDescription *pfd;
	PangoAttrList *attrs;
	PangoAttribute *size;
	PangoAttribute *font;
	GtkWidget *head;

	pfd = pango_font_description_from_string("verdana, 7");
	head = gtk_label_new(title);
	gtk_widget_modify_font(head, pfd);
	gtk_widget_show(head);

	pango_font_description_free(pfd);

	return head;
}
#endif

static GtkTreeViewColumn *
add_text_renderer(GtkWidget *view, const char *title, int col, int sort_col, gfloat align) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *head;

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_padding(renderer, 0, 0);
	gtk_cell_renderer_set_alignment(renderer, align, 0.0f);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", col, NULL);
	gtk_tree_view_column_set_title(column, title);
#if 0
	head = mk_head(title);
	gtk_tree_view_column_set_widget(column, head);
#endif
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	if (sort_col >= 0)
		gtk_tree_view_column_set_sort_column_id(column, sort_col);

	g_object_set(renderer, "font", "fixed", NULL);

	return column;
}

GtkWidget *
mk_list() {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *bin;
	GtkWidget *container;
	GtkWidget *list;
	GtkListStore *store;
	GtkTreeModel *filter;
	GtkTreeModel *sort;
	
	store = gtk_list_store_new(SC_LAST,
		G_TYPE_STRING, /* flag */
		G_TYPE_STRING, /* addr */
		G_TYPE_STRING, /* name */
		G_TYPE_STRING, /* squawk */
		G_TYPE_INT,    /* alt */
		G_TYPE_INT,    /* speed */
		G_TYPE_INT,    /* head */
		G_TYPE_INT,    /* vrate */
		G_TYPE_INT,    /* msgs */
		G_TYPE_INT,    /* seen */
		G_TYPE_STRING, /* track str */
		G_TYPE_STRING, /* seen str */
		G_TYPE_POINTER,
		G_TYPE_OBJECT
	);
	gui.store = store;

	container = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow*)container, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	sort   = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(filter));
	list   = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sort));
	g_object_unref(sort);
	gui.f = GTK_TREE_MODEL_FILTER(filter);

	g_signal_connect(list, "row-activated", G_CALLBACK(cb_row_activated), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter), visible_func, NULL, NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_alignment(renderer, 0.0f, 0.0f);
	column = gtk_tree_view_column_new_with_attributes("", renderer, "pixbuf", SC_OBJ_FLAG, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
	gtk_tree_view_column_set_sort_column_id(column, SC_FLAG);
	gui.column.flag = column;

	gui.column.addr = add_text_renderer(list, "addr", SC_ADDR, SC_ADDR, 0.0f);
	gui.column.name = add_text_renderer(list, "name", SC_NAME, SC_NAME, 0.0f);
	gui.column.id   = add_text_renderer(list, "id", SC_SQUAWK, SC_SQUAWK, 0.0f);
	gui.column.alt  = add_text_renderer(list, "alt", SC_ALT, SC_ALT, 1.0f);
	gui.column.vel  = add_text_renderer(list, "speed", SC_SPEED, SC_SPEED, 1.0f);
	gui.column.head = add_text_renderer(list, "head", SC_STR_TRACK, SC_TRACK, 0.0f);
	gui.column.vert = add_text_renderer(list, "vert", SC_VRATE, SC_VRATE, 1.0f);
	gui.column.msgs = add_text_renderer(list, "msgs", SC_MSGS, SC_MSGS, 1.0f);
	gui.column.seen = add_text_renderer(list, "seen", SC_STR_SEEN, SC_SEEN, 1.0f);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), 0);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(list));

	gtk_container_add(GTK_CONTAINER(container), list);

	gui.treeview = GTK_TREE_VIEW(list);
	return container;
}

static void
cb_filter_seen(GtkSpinButton *spin, gpointer user_data) {
	GtkAdjustment *adj;
	adj = gtk_spin_button_get_adjustment(spin);
	gui.filter.seen_list = (int)gtk_adjustment_get_value(adj);
}

static void
cb_filter_msgs(GtkSpinButton *spin, gpointer user_data) {
	GtkAdjustment *adj;
	adj = gtk_spin_button_get_adjustment(spin);
	gui.filter.msgs_list = (int)gtk_adjustment_get_value(adj);
}

static void
cb_do_filter(GtkWidget *w, gpointer user_data) {
	struct ms_aircraft_t *a;

	gtk_tree_model_filter_refilter(gui.f);

	for (a = aircrafts; a; a = a->next) {
		struct ms_ac_ext_t *ext = aircrafts->ext;

		if (!ext)
			continue;
		set_trail(a, a == gui.sel);
	}

	osm_gps_map_map_redraw_idle(gui.map);
	if (gui.new_source) {
		gui.new_source = false;
		osm_gps_map_set_map_source(gui.map, gui.map_source);
	}
}

static void
cb_plot_old(GtkWidget *w, gpointer user_data) {
	gui.filter.plot_inactive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static void
cb_plot_prev(GtkWidget *w, gpointer user_data) {
	gui.filter.plot_previous = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

GtkWidget *
mk_settings() {
	GtkWidget *container  = gtk_table_new(7, 3, FALSE);
	GtkWidget *f_seen_lbl = gtk_label_new("Max seen:");
	GtkWidget *f_msgs_lbl = gtk_label_new("Min msgs:");
	GtkWidget *mapsrc_lbl = gtk_label_new("Map:");
	GtkWidget *filter_age = gtk_spin_button_new_with_range(0, G_MAXINT, 60);
	GtkWidget *filter_msg = gtk_spin_button_new_with_range(0, G_MAXINT, 1);
	GtkWidget *do_filter  = gtk_button_new_with_label("Apply");
	GtkWidget *plot_old   = gtk_check_button_new_with_label("Plot inactive aircrafts");
	GtkWidget *plot_prev  = gtk_check_button_new_with_label("Plot old trails");
	GtkWidget *map_sel    = mk_mapsel();
	
	gtk_table_set_col_spacings(GTK_TABLE(container), 2);
	gtk_table_set_row_spacings(GTK_TABLE(container), 2);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(filter_age), gui.filter.seen_list);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(filter_msg), gui.filter.msgs_list);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plot_old),  gui.filter.plot_inactive);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plot_prev), gui.filter.plot_previous);

	gtk_table_attach_defaults(GTK_TABLE(container), f_seen_lbl, 0, 1, 1, 2); 
	gtk_table_attach_defaults(GTK_TABLE(container), f_msgs_lbl, 0, 1, 2, 3); 
	gtk_table_attach_defaults(GTK_TABLE(container), filter_age, 1, 2, 1, 2); 
	gtk_table_attach_defaults(GTK_TABLE(container), filter_msg, 1, 2, 2, 3); 
	gtk_table_attach_defaults(GTK_TABLE(container), plot_prev,  2, 3, 1, 2); 
	gtk_table_attach_defaults(GTK_TABLE(container), plot_old,   2, 3, 2, 3); 

	gtk_table_attach_defaults(GTK_TABLE(container), mapsrc_lbl, 0, 1, 3, 4); 
	gtk_table_attach_defaults(GTK_TABLE(container), map_sel,    1, 2, 3, 4); 
	gtk_table_attach_defaults(GTK_TABLE(container), do_filter,  2, 3, 3, 4); 

	g_signal_connect(filter_age, "value-changed", G_CALLBACK(cb_filter_seen), NULL);
	g_signal_connect(filter_msg, "value-changed", G_CALLBACK(cb_filter_msgs), NULL);
	g_signal_connect(do_filter, "button-press-event", G_CALLBACK(cb_do_filter), NULL);
	g_signal_connect(plot_old,  "toggled", G_CALLBACK(cb_plot_old),  NULL);
	g_signal_connect(plot_prev, "toggled", G_CALLBACK(cb_plot_prev), NULL);

	return container;
}

GtkWidget *
mk_panel() {
	GtkWidget *frame     = gtk_frame_new(NULL);
	GtkWidget *container = gtk_vbox_new(FALSE, 0);
	GtkWidget *info      = mk_info();
	GtkWidget *list      = mk_list();
	GtkWidget *settings  = mk_settings();
	GtkTextBuffer *buf   = gtk_text_buffer_new(NULL);
	GtkWidget *text      = gtk_text_view_new_with_buffer(buf);

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);

	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(frame), container);

	gtk_box_pack_start(GTK_BOX(container), info, FALSE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(container), list, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(container), text, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(container), settings, FALSE, TRUE, 0);

	gui.tbuf = buf;
	gui.text = text;
	gui.list = list;

	return frame;
}

#define CTRL_OR_SHIFT (GDK_CONTROL_MASK | GDK_SHIFT_MASK)
static gboolean
cb_keypress(GtkWidget *win, GdkEventKey *event, gpointer data) {
	int dx = 0, dy = 0;
	bool b;
	int sort_col = -1;

	switch (event->keyval) {
	case GDK_plus:
		osm_gps_map_zoom_in(gui.map);
		return true;
	case GDK_minus:
		osm_gps_map_zoom_out(gui.map);
		return true;
	case GDK_Up:
		dy = -16;
		break;
	case GDK_Down:
		dy = +16;
		break;
	case GDK_Left:
		dx = -16;
		break;
	case GDK_Right:
		dx = +16;
		break;
	case GDK_1:
		sort_col = SC_FLAG;
		break;
	case GDK_2:
		sort_col = SC_ADDR;
		break;
	case GDK_3:
		sort_col = SC_NAME;
		break;
	case GDK_4:
		sort_col = SC_SQUAWK;
		break;
	case GDK_5:
		sort_col = SC_ALT;
		break;
	case GDK_6:
		sort_col = SC_SPEED;
		break;
	case GDK_7:
		sort_col = SC_TRACK;
		break;
	case GDK_8:
		sort_col = SC_VRATE;
		break;
	case GDK_9:
		sort_col = SC_MSGS;
		break;
	case GDK_0:
		sort_col = SC_SEEN;
		break;
	case GDK_p:
	case GDK_P:
		b = gtk_tree_view_get_headers_visible(gui.treeview);
		gtk_tree_view_set_headers_visible(gui.treeview, !b);
		return true;
	case GDK_m:
	case GDK_M:
		if (!gui.sel)
			return false;
		if ((gui.show_list = !gui.show_list)) {
			gtk_widget_hide(gui.text);
			gtk_widget_show(gui.list);
		} else {
			struct ms_msg_t *msg;
			gtk_widget_hide(gui.list);
			gtk_widget_show(gui.text);
			char buf[4096];
			FILE *fp = fmemopen(buf, sizeof(buf), "w+");
			GtkTextIter start, end;
			int i;
			gtk_text_buffer_get_start_iter(gui.tbuf, &start);
			gtk_text_buffer_get_end_iter(gui.tbuf, &end);
			gtk_text_buffer_delete(gui.tbuf, &start, &end);

			for (msg = gui.sel->messages, i = 1; msg; ++i, msg = msg->ac_next) {
				memset(buf, 0, sizeof(buf));
				rewind(fp);
				fprintf(fp, "Message: %d (%d)\n",
				        i,
				        gui.sel->n_messages - (gui.message_cache - i));
				pr_msg(fp, msg, 0);
				gtk_text_buffer_get_start_iter(gui.tbuf, &start);
				gtk_text_buffer_insert(gui.tbuf, &start, buf, -1);
			}
			gtk_text_buffer_get_start_iter(gui.tbuf, &start);
			gtk_text_buffer_place_cursor(gui.tbuf, &start);
			fclose(fp);

		}
		return true;
	case GDK_h:
	case GDK_H:
		if (event->state & CTRL_OR_SHIFT)
			osm_gps_map_set_show_home(gui.map, gui.show_home = !gui.show_home);
		else
			osm_gps_map_set_center(gui.map, gui.home_lat, gui.home_lon);
		return true;
	}


	if (event->state & CTRL_OR_SHIFT) {
		if (sort_col != -1) {
			switch (sort_col) {
			case SC_FLAG:
				gtk_tree_view_column_clicked(gui.column.flag);
				return true;
			case SC_ADDR:
				gtk_tree_view_column_clicked(gui.column.addr);
				return true;
			case SC_NAME:
				gtk_tree_view_column_clicked(gui.column.name);
				return true;
			case SC_SQUAWK:
				gtk_tree_view_column_clicked(gui.column.id);
				return true;
			case SC_ALT:
				gtk_tree_view_column_clicked(gui.column.alt);
				return true;
			case SC_SPEED:
				gtk_tree_view_column_clicked(gui.column.vel);
				return true;
			case SC_TRACK:
				gtk_tree_view_column_clicked(gui.column.head);
				return true;
			case SC_VRATE:
				gtk_tree_view_column_clicked(gui.column.vert);
				return true;
			case SC_MSGS:
				gtk_tree_view_column_clicked(gui.column.msgs);
				return true;
			case SC_SEEN:
				gtk_tree_view_column_clicked(gui.column.seen);
				return true;
			}
		}
		else if (dy != 0 || dx != 0) {
			osm_gps_map_move(gui.map, dx, dy);
			return true;
		}
	}


	return false;
}

GtkWidget*
mk_win() {
	GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_hbox_new(FALSE, 1);
	GtkWidget *pan = mk_panel();
	GtkWidget *map = mk_map();

	gtk_box_pack_start(GTK_BOX(box), pan, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), map, TRUE,  TRUE, 0);

	g_signal_connect(win, "delete_event", G_CALLBACK(cleanup), NULL);
	g_signal_connect(win, "key-press-event", G_CALLBACK(cb_keypress), NULL);

	gtk_container_add(GTK_CONTAINER(win), box);

	gtk_widget_show_all(win);

	gtk_widget_hide(gui.text);
	gui.show_list = true;

	gui.win = win;

	return win;
}

static void
usage() {
	printf("usage: %s [options] <file>\n", argv0);
	printf("options:\n"
	       " -m #:\tlisted aircrafts threshold, messages\n"
	       " -s #:\tlisted aircrafts threshold, seconds\n"
	       " -a #:\tactive aircrafts threshold, seconds\n"
	       " -M #:\tmessage cache per aircraft\n"
	       " -h:\tdo show home location\n"
	       " -H:\tdo not show home location\n"
	       " -p:\tdo plot previous trails\n"
	       " -P:\tdo not plot previous trails\n"
	       " -i:\tdo plot inactive aircraft trails\n"
	       " -I:\tdo not plot inactive aircraft trails\n"
	       " -S #:\tuse map source number #\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int i;
	int err = 0;
	int ifd = -1;
	int iwfd = -1;
	char *filename;
	GIOChannel *gioc;
	double dpi;
	GError *gerr;

	gtk_init(&argc, &argv);
	setlocale(LC_ALL, "");

	if (init_colors() < 0) {
		fprintf(stderr, "invalid color(s)\n");
		return 1;
	}

	gui.home_lat = default_lat;
	gui.home_lon = default_lon;
	gui.filter.seen_list = default_seen_list;
	gui.filter.msgs_list = default_msgs_list;
	gui.filter.seen_active = default_seen_active;
	gui.filter.plot_inactive = default_plot_inactive;
	gui.filter.plot_previous = default_plot_previous;
	gui.map_source = 0;
	gui.message_cache = default_message_cache;
	gui.show_home = default_show_home;

	ARGBEGIN {
	case 's':
		gui.filter.seen_list = atoi(EARGF(usage()));
		break;
	case 'a':
		gui.filter.seen_active = atoi(EARGF(usage()));
		break;
	case 'M':
		gui.message_cache = atoi(EARGF(usage()));
		break;
	case 'm':
		gui.filter.msgs_list = atoi(EARGF(usage()));
		break;
	case 'P':
		gui.filter.plot_previous = false;
		break;
	case 'p':
		gui.filter.plot_previous = true;
		break;
	case 'H':
		gui.show_home = false;
		break;
	case 'h':
		gui.show_home = true;
		break;
	case 'I':
		gui.filter.plot_inactive = false;
		break;
	case 'i':
		gui.filter.plot_inactive = true;
		break;
	case 'S':
		gui.map_source = atoi(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND;

	if (!argc)
		usage();

	filename = argv[0];

	if (gui.map_source >= sizeof(sources) / sizeof(sources[0])) {
		fprintf(stderr, "%s: FATAL: Invalid map source: %d\n", argv0, gui.map_source);
		return 1;
	}

	if (init_inotify(filename, &ifd, &iwfd) < 0) {
		return 1;
	}

	mk_win();

	dpi = gdk_screen_get_resolution(gdk_screen_get_default());
	gui.scale = (dpi * 39.37) * (6378136.6 * (2 * M_PI)) / 256.0;

	gioc = g_io_channel_unix_new(ifd);
	g_io_add_watch(gioc, G_IO_IN | G_IO_PRI, &icb, filename);

	if (cat(filename) < 0)
		err -= 1;

	gtk_main();

	if (ifd >= 0)
		close(ifd);
	if (iwfd >= 0)
		close(iwfd);

/*
	for (i = 0; i < COL_COUNT; ++i)
		gdk_color_free(colors + i);
*/
	return err ? 1 : 0;
}
