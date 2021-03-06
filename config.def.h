#ifdef CONF_MSG_TTL
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
#endif


#ifdef CONF_MSDEC
/*
 * Default paths for dumping, if the path
 * contains “XXXXXX” it will be passed
 * to mkstemp or mkdtemp.
 */
static const char default_histogram_filename[]  = "/tmp/msdec-hist-XXXXXX";
static const char default_statistics_filename[] = "/tmp/msdec-stats-XXXXXX";
static const char default_aircraft_directory[]  = "/tmp/msdec-blackbox-XXXXXX";
#endif


#ifdef CONF_MSGUI
#define CONF_MSFILES
static const double   default_lon  = 0.0;
static const double   default_lat  = 0.0;

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
#endif

#ifdef CONF_RTL_MODES
#define CONF_MSFILES
/*
 * User to run daemon as, the log- and outfiles will
 * also be chowned to this uid.
 */
static uid_t default_uid = 215;
/* Group daemon is run as, make sure it has access to
 * the rtl-sdr device
 */
static gid_t default_daemon_gid = 87;
/*
 * Groups which the files are chown to. (Doesn't really
 * matter, since no permissions on the files are changed,
 * so both group and others would have read but no write
 * access. On most systems at least, I suppose…)
 */
static gid_t default_log_gid = 10;
static gid_t default_out_gid = 10;
#endif


#ifdef CONF_MSFILES
static const char default_logfile[] = "/var/log/mode_s.log";
static const char default_outfile[] = "/var/log/mode_s.out";
#endif
