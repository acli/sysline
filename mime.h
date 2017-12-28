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
 *
 * $Header: /source/Repository/sysline/mime.h,v 1.4 2000/05/27 21:01:17 acli Exp $
 */

#ifndef _MIME_H_
#define _MIME_H_

#define CTE_NONE	0	/* no or unknown transfer encoding */
#define CTE_QP		1	/* quoted-printable (RFC1341 section 5.1) */
#define CTE_BASE64	2	/* base64 (RFC1341 section 5.2) */
#define CTE_Q		3	/* Q encoding in RFC1342 (_ means 0x20) */

int	skip_whitespace( char *lbuf, int i );
int	find_boundary( int n, char *lbuf, unsigned sizeof_lbuf, char *bbuf, unsigned sizeof_bbuf, FILE *mfd );
int	find_content_transfer_encoding( int n, char *lbuf, unsigned sizeof_lbuf );
char *	mime_convert( char *lbuf, int cte );
char *	rfc1342_convert( char *lbuf );

#endif
