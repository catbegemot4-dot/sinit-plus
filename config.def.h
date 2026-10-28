/* See LICENSE file for copyright and license details. */

/* ---- commands (same names as in the original sinit) ---- */
static char *const rcinitcmd[]     = { "/bin/rc.init", NULL };
static char *const rcrebootcmd[]   = { "/bin/rc.shutdown", "reboot", NULL };
static char *const rcpoweroffcmd[] = { "/bin/rc.shutdown", "poweroff", NULL };

/* ---- modules: 1 = on, 0 = off (an old config.h = everything off) ---- */

/* After rc.shutdown ends (or hangs): SIGTERM -> SIGKILL all, sync(),
 * then reboot(2). rc.shutdown no longer has to power the machine off. */
#define MOD_NATIVE_SHUTDOWN 1

/* Linux: Ctrl-Alt-Del is delivered as SIGINT (graceful reboot)
 * instead of an instant hard reset. */
#define MOD_CAD 1

/* If rc.init fails or cannot be executed, start a rescue shell
 * instead of leaving a dead system. */
#define MOD_RESCUE 1
#if MOD_RESCUE
static char *const rcrescuecmd[]   = { "/bin/sh", NULL };
#endif

/* Keep services alive (getty etc.). They start after rc.init exits and
 * are restarted at most once per RESPAWN_DELAY seconds. */
#define MOD_RESPAWN 0
#if MOD_RESPAWN
static char *const svc_tty1[] = { "/sbin/agetty", "tty1", "linux", NULL };
static char *const svc_tty2[] = { "/sbin/agetty", "tty2", "linux", NULL };
static struct service services[] = {
	{ .argv = svc_tty1 },
	{ .argv = svc_tty2 },
};
#endif

/* ---- optional tuning ---- */
/* #define TIMEO          30 */
/* #define SHUTDOWN_TIMEO 60 */
/* #define KILL_TIMEO      5 */
/* #define RESPAWN_DELAY   2 */
