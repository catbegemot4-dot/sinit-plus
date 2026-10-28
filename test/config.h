/* Test config, used only by test/run.sh. TESTDIR is passed by the script
 * (sinit does chdir("/"), so all paths must be absolute). */
#ifndef TESTDIR
#error "TESTDIR is not defined, build via test/run.sh"
#endif

static char *const rcinitcmd[]     = { TESTDIR "/rc.init", NULL };
static char *const rcrebootcmd[]   = { TESTDIR "/rc.shutdown", "reboot", NULL };
static char *const rcpoweroffcmd[] = { TESTDIR "/rc.shutdown", "poweroff", NULL };

#define MOD_NATIVE_SHUTDOWN 1
#define MOD_CAD 1
#define MOD_RESCUE 1
static char *const rcrescuecmd[]   = { "/bin/sh", "-c", "echo '[rescue] shell started'", NULL };

#define MOD_RESPAWN 1
static char *const svc_demo[] = { "/bin/sh", "-c",
	"echo '[service] started'; sleep 3; echo '[service] exiting'", NULL };
static struct service services[] = {
	{ .argv = svc_demo },
};

/* short timeouts so the test is quick */
#define RESPAWN_DELAY  1
#define SHUTDOWN_TIMEO 10
#define KILL_TIMEO     2
