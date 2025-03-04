/*	$OpenBSD: dc.c,v 1.10 2007/07/29 17:12:18 sobrado Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: dc.c,v 1.10 2007/07/29 17:12:18 sobrado Exp $";
#endif /* not lint */

#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static __dead void	usage(void);

extern char		*__progname;

static __dead void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-x] [-e expression] [file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		ch;
	bool		extended_regs = false;
	FILE		*file;
	struct source	src;
	char		*buf, *p;
	struct stat	st;


	if ((buf = strdup("")) == NULL)
		err(1, NULL);
	/* accept and ignore a single dash to be 4.4BSD dc(1) compatible */
	while ((ch = getopt(argc, argv, "e:x-")) != -1) {
		switch (ch) {
		case 'e':
			p = buf;
			if (asprintf(&buf, "%s %s", buf, optarg) == -1)
				err(1, NULL);
			free(p);
			break;
		case 'x':
			extended_regs = true;
			break;
		case '-':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	init_bmachine(extended_regs);
	(void)setlinebuf(stdout);
	(void)setlinebuf(stderr);

	if (argc > 1)
		usage();
	if (buf[0] != '\0') {
		src_setstring(&src, buf);
		reset_bmachine(&src);
		eval();
		free(buf);
		if (argc == 0)
			return (0);
	}
	if (argc == 1) {
		file = fopen(argv[0], "r");
		if (file == NULL)
			err(1, "cannot open file %s", argv[0]);
		if (fstat(fileno(file), &st) == -1)
			err(1, "%s", argv[0]);
		if (S_ISDIR(st.st_mode)) {
			errno = EISDIR;
			err(1, "%s", argv[0]);
		}
		src_setstream(&src, file);
		reset_bmachine(&src);
		eval();
		(void)fclose(file);
		/*
		 * BSD and Solaris dc(1) continue with stdin after processing
		 * the file given as the argument. We follow GNU dc(1).
		 */
		 return (0);
	}
	src_setstream(&src, stdin);
	reset_bmachine(&src);
	eval();

	return (0);
}
