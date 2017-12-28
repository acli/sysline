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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)sysline.c	5.16 (Berkeley) 6/24/90";*/
static char rcsid[] = "$Header: /source/Repository/sysline/sysline.c,v 1.9 2000/06/08 16:53:13 acli Exp $";
#endif /* not lint */

/*
 *
 * Linux port:
 * v 1.0	May 11th 1995	bjdouma@xs4all.nl (Bauke Jan Douma)
 * v 1.1	Mar 16th 1996	bjdouma@xs4all.nl (Bauke Jan Douma)
 *
 * Miscellaneous hacks by Ambrose Li
 *
 */

/*
 * sysline - system status display on 25th line of terminal
 * j.k.foderaro
 *
 * Prints a variety of information on the special status line of terminals
 * that have a status display capability.  Cursor motions, status commands,
 * etc. are gleaned from /etc/termcap, or an appropriate terminfo file.
 * By default, ALL information is printed, and flags are given on the command
 * line to disable the printing of information.  The information and
 * disabling flags are:
 *
 *  flag	what
 *  -----	----
 *		time of day
 *		load average and change in load average in the last 5 mins
 *		number of user logged on
 *   -p		# of processes the users owns which are runnable and the
 *		  number which are suspended.  Processes whose parent is 1
 *		  are not counted.
 *   -l		users who've logged on and off.
 *   -m		summarize new mail which has arrived
 *
 *  <other flags>
 *   -r		use non reverse video
 *   -c		turn off 25th line for 5 seconds before redisplaying.
 *   -b		beep once one the half hour, twice on the hour
 *   +N		refresh display every N seconds.
 *   -i		print pid first thing
 *   -e		do simple print designed for an emacs buffer line
 *   -w		do the right things for a window
 *   -h		print hostname between time and load average
 *   -D		print day/date before time of day
 *   -d		debug mode - print status line data in human readable format
 *   -q		quiet mode - don't output diagnostic messages
 *   -s		print Short (left-justified) line if escapes not allowed
 *   -j		Print left Justified line regardless
 *   -t         use 24-hour format for time of day
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <utmp.h>
#include <ctype.h>
#include <termio.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__	/* Red Hat 7, general for all glibc-based systems? */
#include <time.h>
#endif
#include <sys/time.h>
#include <termcap.h>
#include "config.h"
#include "mime.h"

#ifdef RWHO
	#include "rwhod.h"
	#define	DOWN_THRESHOLD	(11 * 60)
	struct remotehost 
	{
		char *rh_host;
		int rh_file;
	} 
	 remotehost[10];
	int nremotes = 0;
#endif

#include "pathnames.h"
double _avenrun[3];		/* used for storing load averages */

/*
 * In order to determine how many people are logged on and who has
 * logged in or out, we read in the /etc/utmp file. We also keep track of
 * the previous utmp file.
 */
int ut = -1;			/* the file descriptor */
struct utmp *new, *old;	
char *ttystatus;		/* per tty status bits, see below */
int nentries;			/* number of utmp entries */

	/* string lengths for printing */
#define LINE_SIZE	UT_LINESIZE
#define NAME_SIZE	UT_NAMESIZE

/*
 * Status codes to say what has happened to a particular entry in utmp.
 * NOCH means no change, ON means new person logged on,
 * OFF means person logged off.
 */
#define NOCH	0
#define ON	0x1
#define OFF	0x2

#ifdef WHO
	char whofilename[100];
	char whofilename2[100];
#endif

#ifdef HOSTNAME
	char hostname[MAXHOSTNAMELEN+1];	/* one more for null termination */
#endif

char lockfilename[100];		/* if exists, will prevent us from running */

	/* flags which determine which info is printed */
int mailcheck = 1;	/* m - do biff like checking of mail */
int proccheck = 1;	/* p - give information on processes */
int logcheck = 1; 	/* l - tell who logs in and out */
int hostprint = 0;	/* h - print out hostname */
int dateprint = 0;	/* h - print out day/date */
int quiet = 0;		/* q - hush diagnostic messages */

	/* flags which determine how things are printed */
int clr_bet_ref = 0;	/* c - clear line between refeshes */
int reverse = 1;	/* r - use reverse video */
int shortline = 0;	/* s - short (left-justified) if escapes not allowed */
int leftline = 0;	/* j - left-justified even if escapes allowed */
int clock24 = 0;	/* t - 24-hour clock */

	/* flags which have terminal do random things	*/
int do_beep = 0;	/* b - beep every half hour and twice every hour */
int printid = 0;	/* i - print pid of this process at startup */
int synch = 1;		/* synchronize with clock */

	/* select output device (status display or straight output) */
int emacs = 0;		/* e - assume status display */
int window = 0;		/* w - window mode */
int dbug = 0;		/* d - debug */

int revtime = 1;

	/* used by mail checker */
off_t mailsize = 0;
off_t linebeg = 0;		/* place where we last left off reading */

	/* things used by the string routines */
int chars;			/* number of printable characters */
char *sp;
char strarr[512];		/* big enough now? */
	/* flags to stringdump() */
char sawmail;			/* remember mail was seen to print bells */
char mustclear;			/* status line messed up */

	/* strings which control status line display */
#ifdef TERMINFO
	char	*bel;
	char	*rev_out;
	char	*rev_end;
#if 0
	char	*tparm();
#endif
#else
	/* as long as Linux has gnu or gnu-based termcap, let it allocate storage */
	#define _HAVE_GNU_TERMCAP_
	#if defined( _HAVE_GNU_TERMCAP_ )
		char	*bel;
		char	*rev_out;
		char	*rev_end;
		char	*clr_eol;
	#else
		char	bel[32];
		char	rev_out[32];
		char	rev_end[32];
		char	clr_eol[64];
#endif
	int	eslok;		/* escapes on status line okay (reverse, cursor addressing) */
	#define tparm(cap,parm)	tgoto((cap),0,(parm))
	char	*tgoto();
#endif

char	*arrows;
char	tsl[64];
char	fsl[64];
char	dsl[64];
int	cols;
int	hasws = 0;		/* is "ws" explicitly defined? */
char	nm_colors[32]="";
char	rv_colors[32]="";

	/* to deal with window size changes */
#ifdef SIGWINCH
	void sigwinch();
	char winchanged;	/* window size has changed since last update */
#endif

	/* random globals */
char *username;
char *ourtty;			/* keep track of what tty we're on */
char *progname;
struct stat stbuf, mstbuf;	/* mstbuf for mail check only */
unsigned delay = DEFDELAY;
uid_t uid;
uid_t fd2_uid;
double cur_loadavg = 0.0;	/* current load average */
int users = 0;
int procrun, procstop;

#include "sysline.h"

int main(int argc,register char **argv)
{
	void clearbotl();
	register char *cp;
	char *home;
	extern char *index();
	char *tty;

	progname = argv[0];

#ifdef HOSTNAME
	gethostname(hostname, sizeof hostname - 1);
	if ((cp = index(hostname, '.')) != NULL)
		*cp = '\0';
#endif

	for (argv++; *argv != 0; argv++)
		switch (**argv) {
		case '-':
			for (cp = *argv + 1; *cp; cp++) {
				switch(*cp) {
				case 'r' :	/* turn off reverse video */
					reverse = 0;
					break;
				case 'c':
					clr_bet_ref = 1;
					break;
				case 'h':
					hostprint = 1;
					break;
				case 'D':
					dateprint = 1;
					break;
#ifdef RWHO
				case 'H':
					if (argv[1] == 0 || *argv[1]=='-')
					{
						fprintf(stderr,	"%s: -H flag requires argument\n", progname);
						exit( 1 );
					}
					argv++;
					/* we exclude ourselves */
					if (strcmp(hostname, *argv) &&
					    strcmp(&hostname[sizeof NETPREFIX - 1], *argv))
						remotehost[nremotes++].rh_host = *argv;
					break;
#endif RWHO
				case 'm':
					mailcheck = 0;
					break;
				case 'p':
					proccheck = 0;
					break;
				case 'l':
					logcheck = 0;
					break;
				case 'b':
					do_beep = 1;
					break;
				case 'i':
					printid = 1;
					break;
				case 't':		/* MFCF */
					clock24 = 1;
					break;
				case 'w':
					window = 1;
					break;
				case 'e':
					emacs = 1;
					break;
				case 'd':
					dbug = 1;
					break;
				case 'q':
					quiet = 1;
					break;
				case 's':
					shortline = 1;
					break;
				case 'j':
					leftline = 1;
					break;
				default:
					fprintf(stderr, "%s: bad flag: %c\n", progname, *cp);
					exit( 1 );
				}
			}
			break;
		case '+':
			delay = atoi(*argv + 1);
			if (delay < 10)
				delay = 10;
			else if (delay > 500)
				delay = 500;
			synch = 0;	/* no more sync */
			break;
		default:
			fprintf(stderr, "%s: illegal argument `%s'\n", progname, argv[0]);
			exit( 1 );
		}

	if ((cp=getenv( "SYSLINE_COLORS" )) || (cp=getenv( "SYSLINE_COLOURS" )))
	{
		get_attrs_colors( cp, "nm=", nm_colors );	/* normal  */
		get_attrs_colors( cp, "rv=", rv_colors );	/* reverse */
	}

	if (emacs) {
		reverse = 0;
		cols = 79;
	} else		/* if not to emacs window, initialize terminal dependent info */
		initterm();

#ifdef SIGWINCH
	/*
	 * When the window size changes and we are the foreground
	 * process (true if -w), we get this signal.
	 */
	setsignal(SIGWINCH, sigwinch);
#endif
	getwinsize();		/* get window size from ioctl */

	/* immediately fork and let the parent die if not emacs mode */
	if (!emacs && !window && !dbug) {
		if (fork())
			exit(0);
		/* pgrp should take care of things, but ignore them anyway */
		setsignal(SIGINT, SIG_IGN);
		setsignal(SIGQUIT, SIG_IGN);
		setsignal(SIGTTOU, SIG_IGN);
	}
	/*
	 * When we logoff, init will do a "vhangup()" on this
	 * tty which turns off I/O access and sends a SIGHUP
	 * signal.  We catch this and thereby clear the status
	 * display.  Note that a bug in 4.1bsd caused the SIGHUP
	 * signal to be sent to the wrong process, so you had to
	 * `kill -HUP' yourself in your .logout file.
	 * Do the same thing for SIGTERM, which is the default kill
	 * signal.
	 */
	setsignal(SIGHUP,  clearbotl);
	setsignal(SIGTERM, clearbotl);
	/*
	 * This is so kill -ALRM to force update won't screw us up..
	 */
	setsignal(SIGALRM, SIG_IGN);

	uid = getuid();
	ourtty = ttyname(2);	/* remember what tty we are on */
	{
		struct stat statbuf;
		if (fstat(2, &statbuf)) {
			perror("sysline");
			exit(1);
		}
		fd2_uid = statbuf.st_uid;
		if (fd2_uid != uid) {
			fprintf(stderr, "Warning: %s is not owned by uid %d\n",
				ourtty, uid);
		}
	}
	if (printid) {
		printf("%d\n", getpid());
		fflush(stdout);
	}
	dup2(2, 1);

	if ((home = getenv("HOME")) == 0)
		home = "";
#ifdef TTY_SUFFIX
	#define DOT "."
	tty = strrchr( ourtty, '/' );
	if (tty)
		tty++;
#else
	#define DOT ""
	tty = "";
#endif
	strcpy1(strcpy1(strcpy1(whofilename,  home), "/.who"DOT), tty );
	strcpy1(strcpy1(strcpy1(whofilename2, home), "/.sysline"DOT), tty );
	strcpy1(strcpy1(strcpy1(lockfilename, home), "/.syslinelock"DOT ), tty );

	if (mailcheck) {
		/* On a semi-SysV system like GNU/Linux, we can't rely on $USER
		 * being set; $LOGNAME might be set instead. And in any case
		 * $MAIL should override all of these
		 */
		char *dir = _PATH_MAILDIR;
		if ((username = getenv("MAIL"))) {
			if ((dir = malloc(strlen(username)))) {
				char *p = strrchr(username, '/');
				if (p) {
					int L = p - username;
					strncpy(dir, username, L);
					dir[L] = '\0';
					username += L + 1;
				} else {
					free(dir);
					dir = NULL;
				}
			}
			if (dir == NULL)
				mailcheck = 0;
		} else {
			if ((username = getenv("USER")) == 0)
				username = getenv("LOGNAME");
			if (username == 0)
				mailcheck = 0;
		}
		if (mailcheck) {
			if (chdir(dir) == -1) {
				perror(dir);
			} else {
				if (stat(username, &mstbuf) >= 0)
					mailsize = mstbuf.st_size;
				else
					mailsize = 0;
			}
		}
	}

	while (emacs || window || isloggedin()) {
		if (access(lockfilename, 0) >= 0) {
			sleep(60);
		} else {
			prtinfo();
			sleep(delay);
			if (clr_bet_ref) {
				tputs(dsl, 1, outc);
				fflush(stdout);
				sleep(5);
			}
			revtime = (1 + revtime) % REVOFF;
		}
	}
	clearbotl();
	/*NOTREACHED*/
	return 0;
}

int isloggedin()
{
	/*
	 * you can tell if a person has logged out if the owner of
	 * the tty has changed
	 */
	struct stat statbuf;

	return fstat(2, &statbuf) == 0 && statbuf.st_uid == fd2_uid;
}

int readutmp(nflag)
	char nflag;
{
	static time_t lastmod;		/* initially zero */
	static off_t utmpsize;		/* ditto */
	struct stat st;

	if (ut < 0 && (ut = open(_PATH_UTMP, 0)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, _PATH_UTMP);
		exit(1);
	}
	if (fstat(ut, &st) < 0 || st.st_mtime == lastmod)
		return 0;
	lastmod = st.st_mtime;
	if (utmpsize != st.st_size)
	{
		off_t old_utmpsize;
		
		old_utmpsize = utmpsize;
		utmpsize = st.st_size;
		nentries = utmpsize / sizeof (struct utmp);
		if (old == 0) {
			old = (struct utmp *)calloc(utmpsize, 1);
			new = (struct utmp *)calloc(utmpsize, 1);
		} else {
			old = (struct utmp *)realloc((char *)old, utmpsize);
			if (utmpsize>old_utmpsize)
				/* clear the extension */
				memset( ((char *)old)+old_utmpsize, 0, (size_t)(utmpsize-old_utmpsize) );
			new = (struct utmp *)realloc((char *)new, utmpsize);
			free(ttystatus);
		}
		ttystatus = malloc(nentries * sizeof *ttystatus);
		if (old == 0 || new == 0 || ttystatus == 0) {
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
	}
	lseek(ut, 0L, 0);
	(void) read(ut, (char *) (nflag ? new : old), utmpsize);
	return 1;
}

void prtinfo()
{
	int on, off;
	register int i;
	char fullprocess;

	stringinit();
#ifdef SIGWINCH
	if (winchanged) {
		winchanged = 0;
		getwinsize();
		mustclear = 1;
	}
#endif
	/*
	 * try to make sure the colour is correct, e.g., a kterm with a
	 * non-white background colour that is not settable by an escape
	 * sequence. this will make sysline on magic cookie terminals
	 * look really ugly, but we assume that such terminals don't have
	 * this colour problem
	 */
#ifdef TERMINFO /* FIXME 20030602 */
	if (!magic_cookie_glitch)
		stringcat(rev_end,
			magic_cookie_glitch <= 0 ? 0 : magic_cookie_glitch);
#endif
#ifdef WHO
	/* 
	 * check for file named .who in the home directory, or, 
	 * if it does not exist, .sysline 
	 */
	whocheck();
#endif
	timeprint();
	/*
	 * if mail is seen, don't print rest of info, just the mail
	 * reverse new and old so that next time we run, we won't lose log
	 * in and out information
	 */
	if (mailcheck && (sawmail = mailseen()))
		goto bottom;
#ifdef HOSTNAME
#ifdef RWHO
	for (i = 0; i < nremotes; i++) {
		char *tmp;

		stringspace();
		tmp = sysrup(remotehost + i);
		stringcat(tmp, strlen(tmp));
	}
#endif
	/*
	 * print hostname info if requested
	 */
	if (hostprint) {
		stringspace();
		stringcat(hostname, -1);
	}
#endif
	/*
	 * print load average and difference between current load average
	 * and the load average 5 minutes ago
	 */
	 
	if (getloadavg(_avenrun, 3) > 0) {
		double diff;

		stringspace();

		if ((diff = _avenrun[0] - _avenrun[1]) < 0.0)
			stringprt("%.1f %.1f", _avenrun[0],  diff);
		else
			stringprt("%.1f +%.1f", _avenrun[0], diff);
		cur_loadavg = _avenrun[0];
	}
	
	/*
	 * print log on and off information
	 */
	stringspace();
	fullprocess = 1;
#ifdef MAXLOAD
	if (cur_loadavg > MAXLOAD)
		fullprocess = 0;	/* too loaded to run */
#endif
	/*
	 * Read utmp file (logged in data) only if we are doing a full
	 * process, or if this is the first time and we are calculating
	 * the number of users.
	 */
	on = off = 0;
	if (users == 0) {		/* first time */
		if (readutmp(0))
			for (i = 0; i < nentries; i++)
				if (old[i].ut_name[0])
					if (old[i].ut_type==USER_PROCESS)
						users++;
	} else if (fullprocess && readutmp(1)) {
		struct utmp *tmp;

		users = 0;

		for (i = 0; i < nentries; i++) 
		{
			if (old[i].ut_name[0] == '\0')
			{
				if (new[i].ut_type==USER_PROCESS)
				{
					ttystatus[i] = ON;
					on++;
				}
				else
					ttystatus[i] = NOCH;
			} 
			else
			if (new[i].ut_name[0] == '\0')
			{
				if (old[i].ut_type==USER_PROCESS)
				{
					ttystatus[i] = OFF;
					off++;
				}
				else
					ttystatus[i] = NOCH;
			}
			else
			if (strncmp(old[i].ut_name, new[i].ut_name, NAME_SIZE) == 0)
			{
				if (old[i].ut_type==new[i].ut_type)
					ttystatus[i] = NOCH;
				else
				{
					if (old[i].ut_type==USER_PROCESS)
					{
						ttystatus[i] = OFF;
						off++;
					}
					else
					if (new[i].ut_type==USER_PROCESS)
					{
						ttystatus[i] = ON;
						on++;
					}
					else
						ttystatus[i] = NOCH;
				}
			}
			else
			{
				ttystatus[i] = ON | OFF;
				on++;
				off++;
			}
			if (new[i].ut_name[0])
				if (new[i].ut_type==USER_PROCESS)
					users++;
		}
		tmp = new;
		new = old;
		old = tmp;
	}
	/*
	 * Print:
	 * 	1.  number of users
	 *	2.  a * for unread mail
	 *	3.  a - if load is too high
	 *	4.  number of processes running and stopped
	 */
	stringprt("%du", users);
	if (mailsize > 0 && mstbuf.st_mtime >= mstbuf.st_atime)
		stringcat("*", -1);
	if (!fullprocess && (proccheck || logcheck))
		stringcat("-", -1);

	if (fullprocess && proccheck) {
		readproctab( uid );
		if (procrun > 0 || procstop > 0) {
			stringspace();
			if (procrun > 0 && procstop > 0)
				stringprt("%dr %ds", procrun, procstop);
			else if (procrun > 0)
				stringprt("%dr", procrun);
			else
				stringprt("%ds", procstop);
		}
	}
	/*
	 * If anyone has logged on or off, and we are interested in it,
	 * print it out.
	 */
	if (logcheck) {
		/* old and new have already been swapped */
		if (on) {
			stringspace();
			stringcat("on:", -1);
			for (i = 0; i < nentries; i++)
				if (ttystatus[i] & ON) {
					stringprt(" %.8s", old[i].ut_name);
					ttyprint(old[i].ut_line);
				}
		}
		if (off) {
			stringspace();
			stringcat("off:", -1);
			for (i = 0; i < nentries; i++)
				if (ttystatus[i] & OFF) {
					stringprt(" %.8s", new[i].ut_name);
					ttyprint(new[i].ut_line);
				}
		}
	}
bottom:
		/* dump out what we know */
	stringdump();
}

void timeprint()
{
	long curtime;
	struct tm *tp, *localtime();
	static int beepable = 1;

		/* always print time */
	time(&curtime);
	tp = localtime(&curtime);
	if (dateprint)
		stringprt("%.11s", ctime(&curtime));
#ifndef CLOCK24
	if (clock24)
#endif
		stringprt("%02d:%02d", tp->tm_hour, tp->tm_min);
#ifndef CLOCK24
	else
		stringprt("%02d:%02d", tp->tm_hour > 12 ? tp->tm_hour - 12 :
			(tp->tm_hour == 0 ? 12 : tp->tm_hour), tp->tm_min);
#endif
	if (synch)			/* sync with clock */
		delay = 60 - tp->tm_sec;
	/*
	 * Beepable is used to insure that we get at most one set of beeps
	 * every half hour.
	 */
	if (do_beep) {
		if (beepable) {
			if (tp->tm_min == 30) {
				tputs(bel, 1, outc);
				fflush(stdout);
				beepable = 0;
			} else if (tp->tm_min == 0) {
				tputs(bel, 1, outc);
				fflush(stdout);
				sleep(2);
				tputs(bel, 1, outc);
				fflush(stdout);
				beepable = 0;
			}
		} else {
			if (tp->tm_min != 0 && tp->tm_min != 30) {
				beepable = 1;
			}
		}
	}
}

/*
 * whocheck -- check for file named .who or .sysline and print it on the who line first
 */
void whocheck()
{
	int chss;
	register char *p;
	char buff[81];
	int whofile;

	if ((whofile = open(whofilename, 0)) < 0 &&
	    (whofile = open(whofilename2, 0)) < 0)
		return;
	chss = read(whofile, buff, sizeof buff - 1);
	close(whofile);
	if (chss <= 0)
		return;
	buff[chss] = '\0';
	/*
	 * Remove all line feeds, and replace by spaces if they are within
	 * the message, else replace them by nulls.
	 */
	for (p = buff; *p;)
		if (*p == '\n')
			if (p[1])
				*p++ = ' ';
			else
				*p = '\0';
		else
			p++;
	stringcat(buff, p - buff);
	stringspace();
}

/*
 * ttyprint -- given the name of a tty, print in the string buffer its
 * short name surrounded by parenthesis.
 * ttyxx is printed as (xx)
 * console is printed as (cty)
 */
void ttyprint(name)
	char *name;
{
	if (strncmp(name, "tty", 3) == 0)
		stringprt("(%.*s)", LINE_SIZE - 3, name + 3);
	else if (strcmp(name, "console") == 0)
		stringcat("(cty)", -1);
	else
		stringprt("(%.*s)", LINE_SIZE, name);
}

/*
 * mail checking function
 * returns 0 if no mail seen
 */
int mailseen()
{
	FILE *mfd;
	register int n;
	register char *cp;
	char lbuf[100], sendbuf[100], *bufend;
	char bbuf[100];
	char seenspace;
	int retval = 0;
	int multipart = 0;
	int mime = 0;
	int blen = 0;
	int cte = CTE_NONE;

	if (stat(username, &mstbuf) < 0) {
		mailsize = 0;
		return 0;
	}
	if (mstbuf.st_size <= mailsize || (mfd = fopen(username,"r")) == NULL) {
		mailsize = mstbuf.st_size;
		return 0;
	}
	fseek(mfd, mailsize, 0);
	while ((n = readline(mfd, lbuf, sizeof lbuf)) >= 0 &&
	       strncmp(lbuf, "From ", 5) != 0)
		;
	if (n < 0) {
		stringspace();
		stringcat("Mail has just arrived", -1);
		goto out;
	}
	retval = 1;
	/*
	 * Found a From line, get second word, which is the sender,
	 * and print it.
	 */
	for (cp = lbuf + 5; *cp && *cp != ' '; cp++)	/* skip to blank */
		;
	*cp = '\0';					/* terminate name */
	stringspace();
	stringprt("Mail from %s ", lbuf + 5);
	/*
	 * Print subject, and skip over header.
	 * This is now a mess because we need to handle MIME mail.
	 */
	while ((n = readline(mfd, lbuf, sizeof lbuf)) > 0) {
		if (strncasecmp(lbuf, "Subject:", 8) == 0) {
			rfc1342_convert(lbuf + 9);
			stringprt("on %s ", lbuf + 9);
		} else if (strncasecmp(lbuf, "Content-Type:", 13) == 0) {
			char *det = lbuf + skip_whitespace(lbuf, 14);
			if (strncasecmp(det, "multipart/", 10) == 0) {
				if (!mime)
					mime = 1;
				multipart = 1;
				n = find_boundary( n, lbuf, sizeof lbuf, bbuf, sizeof bbuf, mfd );
				blen = strlen(bbuf);
			} else if (strncasecmp(det, "text/", 5) != 0) {
				/* Oh, it's not text?! */
				stringprt("(%s) ", det);
				goto out;
			}
		} else if (strncasecmp(lbuf, "Content-Transfer-Encoding:", 26) == 0) {
			cte = find_content_transfer_encoding( n, lbuf, sizeof lbuf);
		} else if (strncasecmp(lbuf, "MIME-Version:", 13) == 0) {
			mime = atol(lbuf + skip_whitespace(lbuf, 14));
		}
	if (n <= 0) break; /* stupid, but needed for the boundary search */
	}
	/*
	 * If we get multipart mail we will need to skip the preamble
	 * and skip over the correct body part's header section---we
	 * will assume that the correct body part is the first one
	 * with a type of "text" (any subtype).
	 */
	if (multipart) {
		int istext;
		for (n = 0, istext = 0; n > -1 && !istext;) {
			while ((n = readline(mfd, lbuf, sizeof lbuf)) > -1) {
			if (strncmp(lbuf, "--", 2) == 0 && strcmp(lbuf + 2, bbuf) == 0) break;
			}
			istext = 1; /* default type is text/plain (RFC 1341 section 4) */
			while ((n = readline(mfd, lbuf, sizeof lbuf)) > 0) {
				if (strncasecmp(lbuf, "Content-Type:", 13) == 0) {
					if (strncasecmp(lbuf + skip_whitespace(lbuf, 14), "text/", 5)) {
						istext = 0;
					}
				} else if (strncasecmp(lbuf, "Content-Transfer-Encoding:", 26) == 0) {
					cte = find_content_transfer_encoding( n, lbuf, sizeof lbuf);
				}
			}
		}
	}
	if (!emacs)
		stringcat(arrows, 2);
	else
		stringcat(": ", 2);
	if (n < 0)					/* already at eof */
		goto out;
	/*
	 * Print as much of the letter as we can.
	 */
	cp = sendbuf;
	if ((n = cols - chars) > sizeof sendbuf - 1)
		n = sizeof sendbuf - 1;
	bufend = cp + n;
	seenspace = 0;
	while ((n = readline(mfd, lbuf, sizeof lbuf)) >= 0) {
		register char *rp;

		if (multipart && strncmp(lbuf, "--", 2) == 0 && strncmp(lbuf + 2, bbuf, blen) == 0
				&& (lbuf[2 + blen] == 0 || strcmp(lbuf + 2 + blen, "--") == 0))
			break;
		if (strncmp(lbuf, "From ", 5) == 0)
			break;
		if (cp >= bufend)
			continue;
		if (!seenspace) {
			*cp++ = ' ';		/* space before lines */
			seenspace = 1;
		}
		mime_convert(lbuf, cte);
		rp = lbuf;
		while (*rp && cp < bufend)
			if (isspace(*rp)) {
				if (!seenspace) {
					*cp++ = ' ';
					seenspace = 1;
				}
				rp++;
			} else {
				*cp++ = *rp++;
				seenspace = 0;
			}
	}
	*cp = 0;
	stringcat(sendbuf, -1);
	/*
	 * Want to update write time so a star will
	 * appear after the number of users until the
	 * user reads his mail.
	 */
out:
	mailsize = linebeg;
	fclose(mfd);
	touch(username);
	return retval;
}

/*
 * readline -- read a line from fp and store it in buf.
 * return the number of characters read.
 */
int readline(fp, buf, n)
	register FILE *fp;
	char *buf;
	register int n;
{
	register int c=0;
	register char *cp = buf;

	linebeg = ftell(fp);		/* remember loc where line begins */
	cp = buf;
#if 0
	while (--n > 0 && (c = getc(fp)) != EOF && c != '\n')
		*cp++ = c;
#else
	/*
	 * If a line is too long, skip over to end of line anyway
	 * Otherwise we get weirdness from long Received headers (say)
	 */
	while ((c = getc(fp)) != EOF && c != '\n')
		if (n > 1) {
			--n;
			*cp++ = c;
		}
#endif
	*cp = 0;
	if (c == EOF && cp - buf == 0)
		return -1;
	return cp - buf;
}


/*
 * string hacking functions
 */

void stringinit()
{
	sp = strarr;
	chars = 0;
}

void stringprt(const char *format, ...)
{
	char tempbuf[150];
	va_list ap;

	va_start( ap, format );
	vsprintf(tempbuf, format, ap);
	va_end( ap );
	stringcat( tempbuf, -1 );
}

void stringdump()
{
	char bigbuf[sizeof strarr + 200];
	register char *bp = bigbuf;
	register int i;

	if (!emacs) {
		if (sawmail)
			bp = strcpy1(bp, bel);
#ifdef TERMINFO
		if (!window && status_line_esc_ok)
#else
		if (!window && eslok)
#endif
			bp = strcpy1(bp, tparm(tsl,
				leftline ? 0 : cols - chars));
		else {
			bp = strcpy1(bp, tsl);
			if (!shortline && !leftline)
				for (i = cols - chars; --i >= 0;)
					*bp++ = ' ';
		}

		if (reverse && revtime != 0)
		{
			if (*rv_colors)
				bp = strcpy1( bp, rv_colors );
			else
				bp = strcpy1( bp, rev_out);
		}
		else
			bp = strcpy1( bp, nm_colors );
	}
	*sp = 0;
	bp = strcpy1(bp, strarr);
	if (!emacs) {
		if (reverse)
			bp = strcpy1(bp, rev_end);
		bp = strcpy1(bp, fsl);
		if (sawmail)
			bp = strcpy1(strcpy1(bp, bel), bel);
		*bp = 0;
		tputs(bigbuf, 1, outc);
		if (mustclear) {
			mustclear = 0;
			tputs(clr_eol, 1, outc);
		}
		if (dbug)
			putchar('\n');
		fflush(stdout);
	} else
		write(2, bigbuf, bp - bigbuf);
}

void stringspace()
{
	if (reverse && revtime != 0) {
#ifdef TERMINFO
		stringcat(rev_end,
			magic_cookie_glitch <= 0 ? 0 : magic_cookie_glitch);
		stringcat(" ", 1);
		if (*rv_colors)
			stringcat( rv_colors, 0 );
		else
			stringcat(rev_out,
			magic_cookie_glitch <= 0 ? 0 : magic_cookie_glitch);
#else
		stringcat(rev_end, 0);
		stringcat(" ", 1);
		if (*rv_colors)
			stringcat( rv_colors, 0 );
		else
			stringcat(rev_out, 0);
#endif TERMINFO
	} else
		stringcat(" ", 1);
}

/*
 * stringcat :: concatenate the characters in string str to the list we are
 * 	        building to send out.
 * str - the string to print. may contain funny (terminal control) chars.
 * n  - the number of printable characters in the string
 *	or if -1 then str is all printable so we can truncate it,
 *	otherwise don't print only half a string.
 */
void stringcat(str, n)
	register char *str;
	register int n;
{
	register char *p = sp;

	if (n < 0) {				/* truncate */
		n = cols - chars;
		while ((*p++ = *str++) && --n >= 0)
			;
		p--;
		chars += p - sp;
		sp = p;
	} else if (chars + n <= cols) {	/* don't truncate */
		while ((*p++ = *str++))
			;
		chars += n;
		sp = p - 1;
	}
}

/*
 * touch :: update the modify time of a file.
 */
void touch(name)
	char *name;		/* name of file */
{
	register int fd;
	char buf;

	if ((fd = open(name, 2)) >= 0) {
		read(fd, &buf, 1);		/* get first byte */
		lseek(fd, 0L, 0);		/* go to beginning */
		write(fd, &buf, 1);		/* and rewrite first byte */
		close(fd);
	}
}


/*
 * clearbotl :: clear bottom line.
 * called when process quits or is killed.
 * it clears the bottom line of the terminal.
 */
void clearbotl()
{
	register int fd;

	setsignal(SIGALRM, exit);
	alarm(30);	/* if can't open in 30 secs, just die */
	if (!emacs && (fd = open(ourtty, 1)) >= 0) {
		write(fd, dsl, strlen(dsl));
		close(fd);
	}
#ifdef PROF
	if (chdir(_PATH_SYSLINE) < 0)
		(void) chdir(_PATH_TMP);
#endif
	exit(0);
}

#ifdef TERMINFO
void initterm()
{
	char *term;
	static char standbuf[40];

	term = getenv("TERM");
	setupterm(0, 1, 0);
	if (!window && !has_status_line) {
		/* not an appropriate terminal */
		if (!quiet)
		   fprintf(stderr, "%s: no status-line capability for terminal type `%s'\n", progname, term);
		exit(1);
	}

	if (!window && init_2string != NULL) 
	{
		tputs(init_2string, 1, erroutc);
		fflush(stdout);
	}

	if (window || status_line_esc_ok) {
		if (set_attributes) {
			/* reverse video mode */
			strcpy(standbuf,
				tparm(set_attributes,0,0,1,0,0,0,0,0,0));
			rev_out = standbuf;
			rev_end = exit_attribute_mode;
		} else if (enter_standout_mode && exit_standout_mode) {
			rev_out = enter_standout_mode;
			rev_end = exit_standout_mode;
		} else
			rev_out = rev_end = "";
	} else
		rev_out = rev_end = "";
	cols = width_status_line;
	hasws = cols>=0;
	if (!hasws)
		cols = columns;		
	cols--;		/* avoid cursor wraparound */
	if (!strncmp(term, "h19", 3))
		arrows = "\033Fhh\033G";	/* "two tiny graphic arrows" */
	else
		arrows = ARROWS;
	if (bell!=NULL)
		bel = bell;
	else
		bel = "\007";
	if (window) 
	{
		strcpy( tsl,   "\r" );
		strcpy( dsl,   "\r" );
		strcpy( dsl+1, clr_eol );
		if (leftline)
			strcpy( fsl, clr_eol );
		else
			strcpy( fsl, "" );
	}
	else
	{
		strcpy( tsl, to_status_line   );
		strcpy( fsl, from_status_line );
		strcpy( dsl, dis_status_line  );
	}
}

#else	/* TERMCAP */
void initterm()
{
#if defined( _HAVE_GNU_TERMCAP_ )
	int res;
#else
	static char tbuf[2048];
#endif
	char *term, *cp;
	char is2[40];
	extern char *UP;

	if ((term = getenv("TERM")) == NULL) {
		if (!quiet)
			fprintf(stderr, "%s: no TERM variable in environment\n", progname);
		exit(1);
	}
	
#if defined( _HAVE_GNU_TERMCAP_ )
	if ((res=tgetent(NULL, term))<=0)
	{
		if (!quiet)
		{
			if (res==-1)
				fprintf(stderr,	"%s: could not access termcap database\n", progname);
			else
				fprintf(stderr,	"%s: unknown terminal `%s'\n", progname, term);
		}
		exit(1);
	}
#else
	if (tgetent(tbuf, term) <= 0) {
		if (!quiet)
			fprintf(stderr, "%s: unknown terminal type `%s'\n", progname, term);
		exit(1);
	}
#endif	
	if (!window && tgetflag("hs") <= 0) {
		if (!strncmp(term, "h19", 3)) {
			/* for upward compatability with h19sys */
			strcpy(tsl,
				"\033j\033x5\033x1\033Y8%+ \033o");
			strcpy(fsl, "\033k\033y5");
			strcpy(dsl, "\033y1");
			strcpy(rev_out, "\033p");
			strcpy(rev_end, "\033q");
			arrows = "\033Fhh\033G";
			cols = 80;
			UP = "\b";
			return;
		}
		if (!quiet)
			fprintf(stderr,	"%s: no status-line capability for terminal `%s'\n", progname, term);
		exit(1);
	}
	cp = is2;
	if (!window && tgetstr("i2", &cp)) {
		/* someday tset will do this */
		tputs(is2, 1, erroutc);
		fflush(stdout);
	}

	/* the "-1" below is to avoid cursor wraparound problems */
	cols = tgetnum("ws");
	hasws = cols >= 0;
	if (!hasws)
		cols = tgetnum("co");
	cols -= 1;
#if defined( _HAVE_GNU_TERMCAP_ )
	if (!(bel=tgetstr("bl", NULL)))
		bel = "\007";
#else
	cp = bel;
	if (!tgetstr("bl", &cp))
		strcpy( bel, "\007" );
#endif

	if (window) {
		strcpy(tsl, "\r");
		cp = dsl;		/* use the clear line sequence */
		*cp++ = '\r';
		tgetstr("ce", &cp);
		if (leftline)
			strcpy(fsl, dsl + 1);
		else
			strcpy(fsl, "");
	} else {
		cp = tsl;
		tgetstr("ts", &cp);
		cp = fsl;
		tgetstr("fs", &cp);
		cp = dsl;
		tgetstr("ds", &cp);
		eslok = tgetflag("es");
	}
	if (eslok || window) {
#if defined( _HAVE_GNU_TERMCAP_ )
		rev_out = tgetstr("so", NULL);
		rev_end = tgetstr("se", NULL);
		clr_eol = tgetstr("ce", NULL);
#else
		cp = rev_out;
		tgetstr("so", &cp);
		cp = rev_end;
		tgetstr("se", &cp);
		cp = clr_eol;
		tgetstr("ce", &cp);
#endif
	} else
		reverse = 0;			/* turn off reverse video */
	UP = "\b";
	if (!strncmp(term, "h19", 3))
		arrows = "\033Fhh\033G";	/* "two tiny graphic arrows" */
	else
		arrows = ARROWS;
}
#endif TERMINFO

#ifdef RWHO
char *
sysrup(hp)
	register struct remotehost *hp;
{
	char filename[100];
	struct whod wd;
#define WHOD_HDR_SIZE (sizeof (wd) - sizeof (wd.wd_we))
	static char buffer[50];
	time_t now;

	/*
	 * rh_file is initially 0.
	 * This is ok since standard input is assumed to exist.
	 */
	if (hp->rh_file == 0) {
		/*
		 * Try rwho hostname file, and if that fails try ucbhostname.
		 */
		(void) strcpy1(strcpy1(filename, _PATH_RWHO), hp->rh_host);
		if ((hp->rh_file = open(filename, 0)) < 0) {
			(void) strcpy1(strcpy1(strcpy1(filename, _PATH_RWHO),
				NETPREFIX), hp->rh_host);
			hp->rh_file = open(filename, 0);
		}
	}
	if (hp->rh_file < 0) {
		(void) sprintf(buffer, "%s?", hp->rh_host);
		return(buffer);
	}
	(void) lseek(hp->rh_file, (off_t)0, 0);
	if (read(hp->rh_file, (char *)&wd, WHOD_HDR_SIZE) != WHOD_HDR_SIZE) {
		(void) sprintf(buffer, "%s ?", hp->rh_host);
		return(buffer);
	}
	(void) time(&now);
	if (now - wd.wd_recvtime > DOWN_THRESHOLD) {
		long interval;
		long days, hours, minutes;

		interval = now - wd.wd_recvtime;
		minutes = (interval + 59) / 60;	/* round to minutes */
		hours = minutes / 60;		/* extract hours from minutes */
		minutes %= 60;			/* remove hours from minutes */
		days = hours / 24;		/* extract days from hours */
		hours %= 24;			/* remove days from hours */
		if (days > 7 || days < 0)
			(void) sprintf(buffer, "%s down", hp->rh_host);
		else if (days > 0)
			(void) sprintf(buffer, "%s %d+%d:%02d",
				hp->rh_host, (int)days, (int)hours, (int)minutes);
		else
			(void) sprintf(buffer, "%s %d:%02d",
				hp->rh_host, (int)hours, (int)minutes);
	} else
		(void) sprintf(buffer, "%s %.1f",
			hp->rh_host, wd.wd_loadav[0]/100.0);
	return buffer;
}
#endif RWHO

void getwinsize()
{
#ifdef TIOCGWINSZ
	struct winsize winsize;

	/* the "-1" below is to avoid cursor wraparound problems */
	if (!hasws
	 && ioctl(2, TIOCGWINSZ, (char *)&winsize) >= 0 && winsize.ws_col != 0)
		cols = winsize.ws_col - 1;
#endif
}

#ifdef SIGWINCH
void sigwinch()
{
	winchanged++;
}
#endif

char *
strcpy1(p, q)
	register char *p, *q;
{
	while ((*p++ = *q++))
		;
	return p - 1;
}

/* 
 * inspired upon ncurses's unctrl() -- bjd
 */
#undef  unctrl
#define unctrl(x)	((x&0x60)!=0&&x!=0x7F)?(x):(x==0x7F?'?':(x|0x40))

int outc(c)
	char c;
{
	if (dbug)
		printf("%c", unctrl(c));
	else
		putchar(c);
	return( 1 );
}

int erroutc(c)
	char c;
{
	if (dbug)
		fprintf(stderr, "%c", unctrl(c));
	else
		putc(c, stderr);
	return( 1 );
}


 /* 
  * following readproctab() is derived from Michael K. Johnson's function
  * take_snapshot in his procps package, stripped of redundancy and
  * customized to fit our purposes - bjd 
  */

 #define PZERO	15

 void readproctab( uid_t uid )
 {
	DIR *proc;
	static struct direct *ent;
	static char filename[80];
	static char stat_str[4096];
	char process_state;
	int  process_ppid, process_pgrp, process_priority;
	struct stat sb;
	int fd, nr_read;

	procrun = procstop = 0;
	proc = opendir("/proc");

	while ((ent = readdir(proc)))
	{
		 /*
		  * I'm assuming anything that starts with a digit is a
		  * valid process directory.  Since the kernel has absolute
		  * control over the /proc directory, this is a valid 
		  * assumption, I think.			- bjd
		  */
		if (!isdigit(*ent->d_name))
			continue;
			
		sprintf(filename, "/proc/%s", ent->d_name);
		if ((stat(filename, &sb))==-1)
			continue;
		if(sb.st_uid != uid) 
    			continue;
		strcat(filename, "/stat");
		if ((fd=open(filename, O_RDONLY, 0))==-1)
			continue;
	   	nr_read = read(fd, stat_str, sizeof(stat_str));
		stat_str[nr_read]='\0';
		close(fd);
    
		sscanf(stat_str, "%*d %*s %c %d %d %*d %*d %*d %*u %*u \
			     %*u %*u %*u %*d %*d %*d %*d %*d %d",
			    &process_state, &process_ppid, 
			    &process_pgrp,  &process_priority );
		if (process_pgrp==0 || process_ppid==1)
    			continue;
		switch (process_state) 
		{
			case 'T':procstop++;
				 break;
			case 'S':
				 /*
				  * Sleep can mean waiting for a signal or just
				  * in a disk or page wait queue ready to run.
				  * We can tell if it is the latter by the pri
				  * being negative.
				  */
				 if (process_priority > 0/*PZERO*/)
					procrun++;
				 break;
			case 'D':
			case 'R':procrun++;
		}
	} /* end of the while loop */
	closedir(proc);
 }

 void setsignal( int sig, void (*func)(int) )
 {
	struct sigaction sa;

	sa.sa_handler = func;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = 0;
	if (sig!=SIGALRM) 
	{
#ifdef	SA_RESTART
		sa.sa_flags |= SA_RESTART;
#endif
	}
	sigaction( sig, &sa, NULL );
 }

 int get_attrs_colors( char *str, char *token, char *store )
 {
	char *s=str;
	
	if ((s=strstr( s, token )))
	{
		int n, val[8];	/* max. 8 attributes/colors */ 

		s += 3;	/* strlen of "nm=" and "rv=" */
		if ((n=sscanf( s, "%d;%d;%d;%d;%d;%d;%d;%d", &val[0], &val[1], &val[2], &val[3], &val[4], &val[5], &val[6], &val[7] ))>=1)
		{
			register int i;
			register char *t = store;
			
			t += sprintf( t, "\033[" );
			for ( i=0; i<n; i++ )
				t += sprintf( t, "%d;", val[i] );
			*(t-1) = 'm';
		
			return( 1 );
		}
	}
	return( 0 );
 }
