/*
 * MIME-related helper functions.
 * RFC-1341
 *
 * Copyright (c) 1980 Regents of the University of California.
 * Copyright (c) 2000 Ambrose Li.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
/*static char sccsid[] = "@(#)sysline.c	5.16 (Berkeley) 6/24/90";*/
static char rcsid[] = "$Header: /source/Repository/sysline/mime.c,v 1.5 2000/05/31 16:04:31 acli Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ctype.h>
#include "mime.h"
#include "sysline.h"

/*
 * header helpers for mailseen() in sysline.c
 */

/*
 * Skip to the first non-whitespace character at or after position i in lbuf
 * Returns the position of such a non-whitespace character
 */
int skip_whitespace( lbuf, i )
	char *lbuf;
	int i;
{
	for (; lbuf[i] && isspace(lbuf[i]); i += 1)
		;
	return i;
}

/*
 * Find the "boundary=" option in the given header lbuf, containing the
 * "Content-Type" header of length n and with an allocated size of sizeof_lbuf;
 * the result is returned in a size-sizeof_bbuf buffer bbuf, and the result of
 * the last readline() call is returned as the return value.
 *
 * The bbuf buffer would not need to be larger than lbuf, since the result
 * stored in it is a substring of the value in lbuf.
 */
int find_boundary( n, lbuf, sizeof_lbuf, bbuf, sizeof_bbuf, mfd )
	int n;
	char *lbuf;
	unsigned sizeof_lbuf;
	char *bbuf;
	unsigned sizeof_bbuf;
	FILE *mfd;
{
	int quote, done, e, i, j, c;
	/* There is no such thing as strcasestr */
	for (quote = 0, done = 0, e = 24, bbuf[0] = '\0'; !done;) {
		for (i = e; lbuf[i]; i += 1) {
			/* Some MTA's put spaces around the word `boundary'... sigh... */
			if (strncasecmp(lbuf + i, "boundary", 8) == 0 && strncmp(lbuf + skip_whitespace(lbuf, i + 8), "=", 1) == 0) {
				i = skip_whitespace(lbuf, skip_whitespace(lbuf, i + 8) + 1);
				if (lbuf[i] == '"') {
					quote = lbuf[i];
					i += 1;
				}
				for (j = 0; lbuf[i] && !isspace(lbuf[i]) && lbuf[i] != quote; i++, j++) {
					bbuf[j] = lbuf[i];
				}
				bbuf[j] = '\0';
				done = 1;
			}
		if (done) break;
		}
	if (done) break;
		/* See if the next line is a continuation line */
		c = fgetc(mfd);
		ungetc(c, mfd);
	if (!isspace(c)) break;
		n = readline(mfd, lbuf, sizeof_lbuf);
		e = 1;
	}
	return n;
}

/*
 * Find the content transfer encoding, given a length-n, size sizeof_lbuf
 * buffer containing the "Content-Transfer-Encoding" header
 */
int find_content_transfer_encoding( n, lbuf, sizeof_lbuf )
	int n;
	char *lbuf;
	unsigned sizeof_lbuf;
{
	int i = skip_whitespace(lbuf, 27);
	int cte = CTE_NONE;
	if (strncasecmp(lbuf + i, "quoted-printable", 16) == 0)
		cte = CTE_QP;
	else if (strncasecmp(lbuf + i, "base64", 6) == 0)
		cte = CTE_BASE64;
	return cte;
}

/*
 * Do quoted-printable conversion on lbuf if cte is CTE_QP or CTE_Q,
 * or base64 conversion on lbuf if cte is CTE_BASE64. Note that CTE_Q
 * (Q encoding defined in RFC 1342) is slightly different from the
 * normal quoted-printable encoding (defined in RFC 1341), that the _
 * character is also defined (as a shorthand for =20).
 */
const char base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *mime_convert( lbuf, cte )
	char *lbuf;
	int cte;
{
	int n;				/* for case CTE_BASE64 */
	int i, j;
	switch(cte) {
	case CTE_QP: case CTE_Q:
		/* RFC 1341, section 5.1 */
		for (i = 0, j = 0; lbuf[i]; i += 1) {
			if (lbuf[i] == '=') {
				if (isxdigit(lbuf[i + 1]) && isxdigit(lbuf[i + 2])) {
					char buf[3];
					buf[2] = '\0';
					strncpy(buf, lbuf + i + 1, 2);
					lbuf[j] = (char)strtol(buf, NULL, 16);
					i += 2;
					j += 1;
				}
			} else if (lbuf[i] == '_' && cte == CTE_Q) {
				lbuf[j] = 0x20;
				j += 1;
			} else {
				lbuf[j] = lbuf[i];
				j += 1;
			}
		}
		lbuf[j] = '\0';
		break;
	case CTE_BASE64:
		/* RFC 1341, section 5.2 */
		/* Passes the "test suite" in RFC 1342 */
		for (n = strlen(lbuf), i = 0, j = 0; i + 3 < n;) {
			char *px = strchr(base64_alphabet, lbuf[i++]);
			char *py = strchr(base64_alphabet, lbuf[i++]);
			char *pz = strchr(base64_alphabet, lbuf[i++]);
			char *pw = strchr(base64_alphabet, lbuf[i++]);
			int x = px? px - base64_alphabet: -1;
			int y = py? py - base64_alphabet: -1;
			int z = pz? pz - base64_alphabet: -1;
			int w = pw? pw - base64_alphabet: -1;
			if (x < 0 || y < 0) {
				/* Impossible (illegal quantum) */
				;
			} else if (z < 0 && w < 0) {
				/* Case 3 in the RFC (two = characters in final 8-bit quantum) */
				unsigned long n = (x << 18) + (y << 12);
				lbuf[j++] = (char)(n >> 16);
			} else if (w < 0) {
				/* Case 2 in the RFC (one = character in final 16-bit quantum) */
				unsigned long n = (x << 18) + (y << 12) + (z << 6);
				lbuf[j++] = (char)(n >> 16);
				lbuf[j++] = (char)(n >> 8);
			} else {
				/* Case 1 in the RFC (normal 24-bit quantum) */
				unsigned long n = (x << 18) + (y << 12) + (z << 6) + w;
				lbuf[j++] = (char)(n >> 16);
				lbuf[j++] = (char)(n >> 8);
				lbuf[j++] = (char)(n);
			}
		}
		lbuf[j] = '\0';
		break;
	}
	return lbuf;
}

/*
 * Do header conversion on lbuf according to RFC 1342, but discarding
 * any charset information (and hope that we won't ruin the display)
 *
 * Note that we don't need to pass the size of the buffer because the
 * result of the conversion is guaranteed (by the RFC) to be at most
 * as long as the input.
 */
char *rfc1342_convert( lbuf )
	char *lbuf;
{
	int i, j;
	for (i = 0, j = 0; lbuf[i];) {
		int converted = 0;
		if (lbuf[i] == '=' && lbuf[i + 1] == '?') {
			char *p = lbuf + i;
			char *q = strstr(p, "?=");
			char *r, *s;
			char  encoding;
			/* Skip over the charset declaration */
			p = strchr(p + 2, '?');
			if (p && p[1] && p[2] == '?' && p[3] && q) {
				encoding = toupper(p[1]);
				if ((encoding == 'B' || encoding == 'Q') && q) {
					/* Invariant: string to convert at p + 3 */
					/* Move the string for mime_convert */
					for (r = p + 3, s = lbuf + i; r < q; r++, s++) {
						*s = *r;
					}
					*s = '\0';
					/* Do the conversion */
					if (encoding == 'B') {
						mime_convert(lbuf + i, CTE_BASE64);
					} else if (encoding == 'Q') {
						mime_convert(lbuf + i, CTE_Q);
					}
					/* Update state variables */
					i = (q + 2) - lbuf;
					j = strlen(lbuf);
					converted = 1;
				}
			}
		}
		if (!converted) {
			lbuf[j++] = lbuf[i++];
		}
	}
	lbuf[j] = '\0';
	return lbuf;
}
