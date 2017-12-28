/*
 * Copyright (c) 1980 Regents of the University of California.
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
 * $Header: /source/Repository/sysline/sysline.h,v 1.1 2000/05/27 04:26:22 acli Rel $
 */

void initterm();
void getwinsize();
int isloggedin();
void prtinfo();
void stringinit();
void stringspace();
void stringcat();
void stringdump();
void stringprt(const char *format, ...);
int readline();
void whocheck();
int mailseen();
void timeprint();
void ttyprint();
void touch();
char *getenv();
char *ttyname();
char *strcpy1();
char *sysrup();
#if 0
char *calloc();
char *malloc();
#endif
int outc();
int erroutc();
int readutmp();
void readproctab( uid_t uid );
void setsignal( int sig, void (*func)(int) );
extern int getloadavg();
int get_attrs_colors();
