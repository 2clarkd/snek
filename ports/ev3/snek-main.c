/*
 * Copyright © 2018 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "snek.h"
#include "snek-io.h"
#include <getopt.h>
#include <signal.h>
#include "sensors.h"
#include "motors.h"

static FILE *snek_posix_input;

static const struct option options[] = {
	{.name = "version", .has_arg = 0, .val = 'v'},
	{.name = "interactive", .has_arg = 0, .val = 'i'},
	{.name = "help", .has_arg = 0, .val = 'h'},
	{.name = NULL, .has_arg = 0, .val = 0},
};

static void
usage(char *program, int val)
{
	fprintf(stderr, "usage: %s [--version] [--help] [--interactive] <program.py>\n", program);
	exit(val);
}


static int
snek_getc_interactive(void)
{
	static char  line_base[4096];
	static char *line;
	int	     c;

	if (!line) {
		char *prompt = "> ";
		if (snek_parse_middle)
			prompt = "+ ";
		fputs(prompt, stdout);
		fflush(stdout);
		line = fgets(line_base, 4096, stdin);
		if (!line)
			return EOF;
	}
	c = (*line++) & 0xff;
	if (!c || c == '\n') {
		c = '\n';
		line = 0;
	}
	return c;
}

int
snek_getc()
{
	if (snek_interactive)
		return snek_getc_interactive();
	return getc(snek_posix_input);
}

static void
on_sigint(int signal)
{
	(void) signal;
	snek_abort = true;
}

int
main(int argc, char **argv)
{
	int  c;
	bool interactive_flag = false;

	while ((c = getopt_long(argc, argv, "vi", options, NULL)) != -1) {
		switch (c) {
		case 'v':
			printf("%s version %s\n", argv[0], SNEK_VERSION);
			exit(0);
			break;
		case 'i':
			interactive_flag = true;
			break;
		case 'h':
			usage(argv[0], 0);
			break;
		default:
			usage(argv[0], 1);
			break;
		}
	}

	struct sigaction act = {
		.sa_handler = on_sigint,
	};
	sigaction(SIGINT, &act, NULL);

	snek_ev3_init_colors();
	snek_init();

	bool ret = true;

	if (argv[optind]) { // At least one file is supplied on the command line
		for (; argv[optind]; optind++) {
			snek_file = argv[optind];
			snek_posix_input = fopen(snek_file, "r");
			if (!snek_posix_input) {
				perror(snek_file);
				exit(1);
			}
			if (snek_parse() != snek_parse_success) {
				fclose(snek_posix_input);
				ret = false;
				break;
			}
			fclose(snek_posix_input);
		}
		snek_interactive = interactive_flag;
	} else {
		snek_interactive = true;
		printf("Welcome to Snek version %s\n", SNEK_VERSION);
	}

	if (snek_interactive) { // -i or at no files are supplied on the command line
		snek_file = "<stdin>";
		snek_posix_input = stdin;
		if (snek_parse() != snek_parse_success)
			ret = false;
	}

	return ret ? 0 : 1;
}

snek_poly_t
snek_builtin_read(snek_poly_t port)
{
	switch (snek_poly_type(port)) {
	case snek_float:
		return snek_ev3_sensors_read(port);
	case snek_string:
		return snek_ev3_motors_read(port);
	default:
		snek_error_type_1(port);
		return SNEK_NULL;
	}
}
