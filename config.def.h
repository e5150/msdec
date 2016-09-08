static const double   default_lon  = 0.0;
static const double   default_lat  = 0.0;

/*
 * Print recorded locations and such
 * along with new messages (regardless
 * of message type) received within ttl
 * seconds of the property being decoded.
 */
static const uint32_t location_ttl = 2;
static const uint32_t velocity_ttl = 2;
static const uint32_t altitude_ttl = 5;
static const uint32_t squawk_ttl = 600;

/*
 * Default paths for dumping, if the path
 * contains “XXXXXX” it will be passed
 * to mkstemp or mkdtemp.
 */
static const char default_histogram_filename[]  = "/tmp/msdec-hist-XXXXXX";
static const char default_statistics_filename[] = "/tmp/msdec-stats-XXXXXX";
static const char default_aircraft_directory[]  = "/tmp/msdec-blackbox-XXXXXX";
static const char default_json_filename[]       = "/tmp/msdec-aircrafts.json";

/*
 * Colours for aircraft trails plotted
 */
static const char default_sel_curr_color[] = "#ff3020";
static const char default_sel_prev_color[] = "#e09020";
static const char default_act_curr_color[] = "#209020";
static const char default_act_prev_color[] = "#207060";
static const char default_old_curr_color[] = "#332211";
static const char default_old_prev_color[] = "#000000";

static bool default_show_home = true;
/*
 * Zoom level, valid values [7, 15]
 *
 */
static int default_zoom_level = 10;
/*
 * Least number of messages for an aircraft to be listed
 * 0 = no limit
 */
static const int default_msgs_list = 10;
/*
 * Threshold for an aircraft to be listed
 * seconds, 0 = no limit
 */
static const int default_seen_list = 60 * 60;
/*
 * Threshold for an aircraft to be regarded as active.
 * seconds, 0 = less than one second ago
 */
static const int default_seen_active = 60 * 2;
/*
 * Plot listed aircrafts, even if they are not active
 */
static bool default_plot_inactive = true;
/*
 * Plot old trails of aircrafts
 */
static bool default_plot_previous = true;
/*
 * Number of messages to cache, per aircraft
 * 0 = no limit
 */
static int default_message_cache = 100;


#ifdef CONF_RTL_MODES
static const char default_logfile[] = "/var/log/mode_s.log";
static const char default_outfile[] = "/var/log/mode_s.out";
static uid_t default_uid = 215;
static gid_t default_daemon_gid = 87;
static gid_t default_log_gid = 10;
static gid_t default_out_gid = 10;
#endif
