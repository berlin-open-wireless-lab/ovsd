/*
 * ovsd - Open vSwitch integration into LEDE's netifd
 * Copyright (C) 2016 Arne Kappen <akappen@inet.tu-berlin.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <getopt.h>
#include <syslog.h>
#include <signal.h>

#include "ovsd.h"
#include "ubus.h"

#define DEFAULT_LOG_LVL LOG_NOTICE

static bool use_syslog = true;

static int log_level = DEFAULT_LOG_LVL;
static const int log_class[] = {
	[L_CRIT] = LOG_CRIT,
	[L_WARNING] = LOG_WARNING,
	[L_NOTICE] = LOG_NOTICE,
	[L_INFO] = LOG_INFO,
	[L_DEBUG] = LOG_DEBUG
};

void
ovsd_log_msg(int log_lvl, const char *format, ...)
{
	va_list vl;

	if (log_lvl > log_level)
		return;

	va_start(vl, format);
	if (use_syslog)
		vsyslog(log_class[log_lvl], format, vl);
	else
		vfprintf(stderr, format, vl);
	va_end(vl);
}

static int
usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options]\n"
		"Options:\n"
		" -s <path>:		Path to the ubus socket\n"
		" -l <level>:		Log output level (default: %d)\n"
		" -S:			Use stderr instead of syslog for log messages\n"
		"\n", progname, DEFAULT_LOG_LVL);

	return 1;
}

static void
ovsd_handle_signal(int signo)
{
	ovsd_log_msg(L_NOTICE, "signal %d caught, shutting down...\n", signo);
	uloop_end();
}

/* exit on signals SIGINT, SIGTERM, SIGUSR1, SIGUSR2 */
static void
ovsd_setup_signals(void)
{
	struct sigaction s;

	memset(&s, 0, sizeof(s));
	s.sa_handler = ovsd_handle_signal;
	s.sa_flags = 0;
	sigaction(SIGINT, &s, NULL);
	sigaction(SIGTERM, &s, NULL);
	sigaction(SIGUSR1, &s, NULL);
	sigaction(SIGUSR2, &s, NULL);

	s.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &s, NULL);
}

int main(int argc, char **argv)
{
	const char *socket = NULL;
	int ch;

	//global_argv = argv;

	while ((ch = getopt(argc, argv, "d:s:p:c:h:r:l:S")) != -1) {
		switch(ch) {
		case 's':
			socket = optarg;
			break;
		case 'l':
			log_level = atoi(optarg);
			if (log_level >= ARRAY_SIZE(log_class))
				log_level = ARRAY_SIZE(log_class) -1;
			break;
		case 'S':
			use_syslog = false;
			break;
		default:
			return usage(argv[0]);
		}
	}

	if (use_syslog)
		openlog("ovsd", 0, LOG_DAEMON);

	ovsd_setup_signals();

	if (ovsd_ubus_init(socket) < 0) {
		fprintf(stderr, "Failed to connect to ubus\n");
		return 1;
	}

	uloop_run();

	if (use_syslog)
		closelog();

	return 0;
}
