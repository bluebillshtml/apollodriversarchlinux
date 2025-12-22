// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin Control Daemon
 *
 * User-space daemon for controlling Apollo Twin device parameters
 * that require higher-level coordination or complex protocols.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "apollo_control.h"

#define DAEMON_NAME "apollod"
#define PID_FILE "/var/run/apollod.pid"

static volatile int running = 1;
static struct apollo_control *control;

/* Signal handler */
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		syslog(LOG_INFO, "Received signal %d, shutting down", sig);
		running = 0;
		break;
	case SIGHUP:
		syslog(LOG_INFO, "Received SIGHUP, reloading configuration");
		/* Reload config if needed */
		break;
	}
}

/* Daemonize process */
static void daemonize(void)
{
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Change file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Change working directory */
	if (chdir("/") < 0) {
		syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Close standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

/* Write PID file */
static void write_pid_file(void)
{
	FILE *fp = fopen(PID_FILE, "w");
	if (fp) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

/* Remove PID file */
static void remove_pid_file(void)
{
	unlink(PID_FILE);
}

/* Initialize ALSA mixer controls */
static int init_alsa_mixer(void)
{
	snd_mixer_t *mixer;
	snd_mixer_elem_t *elem;
	int err;

	err = snd_mixer_open(&mixer, 0);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to open mixer: %s", snd_strerror(err));
		return err;
	}

	err = snd_mixer_attach(mixer, "hw:Apollo");
	if (err < 0) {
		syslog(LOG_ERR, "Failed to attach mixer: %s", snd_strerror(err));
		snd_mixer_close(mixer);
		return err;
	}

	err = snd_mixer_selem_register(mixer, NULL, NULL);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to register mixer: %s", snd_strerror(err));
		snd_mixer_close(mixer);
		return err;
	}

	err = snd_mixer_load(mixer);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to load mixer: %s", snd_strerror(err));
		snd_mixer_close(mixer);
		return err;
	}

	/* Enumerate and log available controls */
	for (elem = snd_mixer_first_elem(mixer); elem; elem = snd_mixer_elem_next(elem)) {
		syslog(LOG_INFO, "Mixer element: %s", snd_mixer_selem_get_name(elem));
	}

	snd_mixer_close(mixer);
	return 0;
}

/* Main daemon loop */
static void daemon_loop(void)
{
	struct apollo_config config;

	syslog(LOG_INFO, "Apollo daemon starting");

	/* Initialize control interface */
	control = apollo_control_init();
	if (!control) {
		syslog(LOG_ERR, "Failed to initialize control interface");
		return;
	}

	/* Load configuration */
	if (apollo_control_load_config(control, &config) < 0) {
		syslog(LOG_WARNING, "Failed to load configuration, using defaults");
		apollo_control_default_config(&config);
	}

	/* Initialize ALSA mixer */
	if (init_alsa_mixer() < 0) {
		syslog(LOG_WARNING, "Failed to initialize ALSA mixer");
	}

	syslog(LOG_INFO, "Apollo daemon running");

	while (running) {
		/* Monitor device status and handle control requests */
		apollo_control_process_events(control);

		/* Sleep for 100ms */
		usleep(100000);
	}

	apollo_control_cleanup(control);
	syslog(LOG_INFO, "Apollo daemon stopped");
}

int main(int argc, char *argv[])
{
	int daemon_mode = 1;

	/* Parse command line arguments */
	if (argc > 1 && strcmp(argv[1], "-f") == 0) {
		daemon_mode = 0;
	}

	/* Initialize syslog */
	openlog(DAEMON_NAME, LOG_PID | LOG_CONS, LOG_DAEMON);

	/* Set up signal handlers */
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);

	if (daemon_mode) {
		daemonize();
		write_pid_file();
	}

	daemon_loop();

	if (daemon_mode) {
		remove_pid_file();
	}

	closelog();
	return EXIT_SUCCESS;
}

