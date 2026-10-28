/* sinit+ -- tiny init, an improved sinit (suckless.org)
 * See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LEN(x)        (sizeof (x) / sizeof *(x))

/* a service init keeps alive (used by MOD_RESPAWN, see config.h) */
struct service {
	char *const *argv;
	pid_t pid;      /* 0 = not running */
	time_t started; /* monotonic seconds, 0 = never started */
};

#include "config.h"

/* Defaults: an old sinit config.h still works, all modules are then off. */
#ifndef MOD_NATIVE_SHUTDOWN
#define MOD_NATIVE_SHUTDOWN 0
#endif
#ifndef MOD_CAD
#define MOD_CAD 0
#endif
#ifndef MOD_RESCUE
#define MOD_RESCUE 0
#endif
#ifndef MOD_RESPAWN
#define MOD_RESPAWN 0
#endif
#ifndef TIMEO
#define TIMEO 30            /* safety-net reap interval, s */
#endif
#ifndef SHUTDOWN_TIMEO
#define SHUTDOWN_TIMEO 60   /* max time for rc.shutdown, s */
#endif
#ifndef KILL_TIMEO
#define KILL_TIMEO 5        /* SIGTERM -> SIGKILL grace period, s */
#endif
#ifndef RESPAWN_DELAY
#define RESPAWN_DELAY 2     /* min seconds between service restarts */
#endif
#ifndef DEFPATH
#define DEFPATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#endif

#if MOD_NATIVE_SHUTDOWN || MOD_CAD
#include <sys/reboot.h>
#endif
#if MOD_NATIVE_SHUTDOWN
# if defined(RB_POWER_OFF)
#  define RB_OFF RB_POWER_OFF      /* Linux */
# elif defined(RB_POWERDOWN)
#  define RB_OFF RB_POWERDOWN      /* BSD */
# else
#  error "MOD_NATIVE_SHUTDOWN is not supported here, set it to 0"
# endif
#endif

enum { ACT_NONE, ACT_POWEROFF, ACT_REBOOT };

static void sigpoweroff(void);
static void sigreap(void);
static void sigreboot(void);
static pid_t spawn(char *const []);
static void finish(void);
static unsigned tick(void);

/* Signals: USR1/USR2/PWR = poweroff, TERM/INT = reboot (sinit + busybox). */
static struct {
	int sig;
	void (*handler)(void);
} sigmap[] = {
	{ SIGUSR1, sigpoweroff },
	{ SIGUSR2, sigpoweroff },
	{ SIGTERM, sigreboot   },
	{ SIGINT,  sigreboot   },
	{ SIGCHLD, sigreap     },
#ifdef SIGPWR
	{ SIGPWR,  sigpoweroff },
#endif
};

static sigset_t set;
static pid_t initpid = -1;   /* rc.init */
static pid_t shutpid = -1;   /* rc.shutdown */
static int action = ACT_NONE;
static time_t deadline;

static time_t
now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + 1; /* never 0, so 0 can mean "never" */
}

/* make sure fds 0,1,2 exist so children and perror() never hit a random fd */
static void
fixfds(void)
{
	int fd;

	for (fd = 0; fd <= 2; fd++) {
		if (fcntl(fd, F_GETFD) != -1)
			continue;
		if (open("/dev/console", O_RDWR | O_NOCTTY) < 0)
			open("/dev/null", O_RDWR);
	}
}

#if MOD_RESCUE
static void
rescue(void)
{
	fputs("sinit: rc.init failed, starting rescue shell\n", stderr);
	spawn(rcrescuecmd);
}
#endif

int
main(void)
{
	struct timespec ts;
	size_t i;
	int sig;

	if (getpid() != 1) {
		fputs("sinit: must be run as PID 1\n", stderr);
		return 1;
	}
	if (chdir("/") < 0)
		perror("chdir");
	fixfds();
	setenv("PATH", DEFPATH, 0);
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);
#if MOD_CAD && defined(RB_DISABLE_CAD)
	reboot(RB_DISABLE_CAD); /* Ctrl-Alt-Del -> SIGINT instead of hard reset */
#endif
	initpid = spawn(rcinitcmd);
	if (initpid < 0) {
		initpid = -1;
#if MOD_RESCUE
		rescue();
#endif
	}
	while (1) {
		ts.tv_sec = tick();
		ts.tv_nsec = 0;
		sig = sigtimedwait(&set, NULL, &ts);
		if (sig < 0) { /* timeout or EINTR: reap anyway */
			sigreap();
			continue;
		}
		for (i = 0; i < LEN(sigmap); i++) {
			if (sigmap[i].sig == sig) {
				sigmap[i].handler();
				break;
			}
		}
	}
	/* not reachable */
	return 0;
}

/* Periodic work; returns how many seconds main may sleep. */
static unsigned
tick(void)
{
	unsigned wait = TIMEO;
#if MOD_RESPAWN
	struct service *s;
	time_t t, left;
	size_t i;
#endif

	if (action != ACT_NONE) {
		time_t rest = deadline - now();

		if (rest > 0)
			return (rest < TIMEO) ? (unsigned)rest : TIMEO;
		shutpid = -1; /* rc.shutdown hung, force the finish */
		finish();
	}
#if MOD_RESPAWN
	if (initpid > 0) /* services start only after rc.init is done */
		return wait;
	t = now();
	for (i = 0; i < LEN(services); i++) {
		s = &services[i];
		if (s->pid > 0)
			continue;
		left = s->started ? s->started + RESPAWN_DELAY - t : 0;
		if (left > 0) {
			if ((time_t)wait > left)
				wait = (unsigned)left;
			continue;
		}
		s->started = t;
		s->pid = spawn(s->argv);
		if (s->pid < 0)
			s->pid = 0;
	}
#endif
	return wait;
}

static void
reaped(pid_t pid, int status)
{
#if MOD_RESPAWN
	size_t i;
#endif
	(void)status;

	if (pid == shutpid) {
		shutpid = -1;
		finish();
		return;
	}
	if (pid == initpid) {
		initpid = -1;
#if MOD_RESCUE
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			rescue();
#endif
		return;
	}
#if MOD_RESPAWN
	for (i = 0; i < LEN(services); i++) {
		if (services[i].pid == pid) {
			services[i].pid = 0;
			break;
		}
	}
#endif
}

static void
begin_shutdown(int act)
{
	if (action != ACT_NONE)
		return;
	action = act;
	deadline = now() + SHUTDOWN_TIMEO;
	shutpid = spawn(act == ACT_REBOOT ? rcrebootcmd : rcpoweroffcmd);
	if (shutpid < 0) { /* fork failed: do the minimum ourselves */
		shutpid = -1;
		finish();
	}
}

#if MOD_NATIVE_SHUTDOWN
/* returns 1 while there are still children left */
static int
reapall(void)
{
	pid_t pid;

	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
		;
	return pid == 0;
}

static void
napwait(int tenths)
{
	struct timespec nap = { 0, 100000000L };
	int i;

	for (i = 0; i < tenths && reapall(); i++)
		nanosleep(&nap, NULL);
}
#endif

/* Called when rc.shutdown has exited (or failed / timed out). */
static void
finish(void)
{
#if MOD_NATIVE_SHUTDOWN
	kill(-1, SIGTERM);
	kill(-1, SIGCONT);
	napwait(KILL_TIMEO * 10);
	kill(-1, SIGKILL);
	napwait(10);
	sync();
	reboot(action == ACT_REBOOT ? RB_AUTOBOOT : RB_OFF);
	perror("reboot"); /* still here: allow another attempt */
#endif
	action = ACT_NONE;
}

static void
sigpoweroff(void)
{
	begin_shutdown(ACT_POWEROFF);
}

static void
sigreap(void)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		reaped(pid, status);
}

static void
sigreboot(void)
{
	begin_shutdown(ACT_REBOOT);
}

static pid_t
spawn(char *const argv[])
{
	pid_t pid = fork();

	switch (pid) {
	case 0:
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		setsid();
		execvp(argv[0], argv);
		fprintf(stderr, "sinit: exec %s: %s\n", argv[0], strerror(errno));
		_exit(127);
	case -1:
		perror("fork");
	}
	return pid;
}
