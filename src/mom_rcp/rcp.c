/*
 * Copyright (c) 1983, 1990, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (C) 1994-2018 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * For a copy of the commercial license terms and conditions,
 * go to: (http://www.pbspro.com/UserArea/agreement.html)
 * or contact the Altair Legal Department.
 *
 * Altair’s dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of PBS Pro and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair’s trademarks, including but not limited to "PBS™",
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
 * trademark licensing policies.
 *
 */

/*
 * Copyright (c) 1983, 1990, 1992, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * rcp.c 8.2 (Berkeley) 4/2/94";
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "extern.h"
#include "pbs_version.h"
#include "pbs_stat.h"
#include "libutil.h"
#include "pbs_ifl.h"

#ifdef        USELOG
#include <syslog.h>
#include <arpa/inet.h>
#endif        /* USELOG */
/**
 * @file	rcp.c
 */
/*
 ** Anything old enough to not have utimes() needs this.
 */
#ifdef	_SX
#include <utime.h>

/**
 * @brief
 *	changes given file's access and modification time.
 *
 * @param[in] path - file path
 * @param[in] times - pointer to timeval struct which holds access and modification time
 *
 * @return	int
 * @retval	0	success
 * @retval	-1	error
 */
int
utimes(const char *path, const struct timeval *times)
{
	struct utimbuf	utimar, *utp = NULL;

	if (times != NULL) {
		utimar.actime = times[0].tv_sec;
		utimar.modtime = times[1].tv_sec;
		utp = &utimar;
	}

	return (utime(path, utp));
}
#endif

extern int setresuid(uid_t, uid_t, uid_t);

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>


char	dst_realm_buf[REALM_SZ];
char	*dest_realm = NULL;
int	use_kerberos = 1;
CREDENTIALS 	cred;
Key_schedule	schedule;
extern	char	*krb_realmofhost();
#ifdef CRYPT
int	doencrypt = 0;
#define	OPTIONS	"dfKk:prtx-:"
#else
#define	OPTIONS	"dfKk:prt-:"
#endif
#else
#define	OPTIONS "Edfprt-:"
#endif

char    *credb = NULL;
size_t  credl = 0;
struct passwd *pwd;
u_short	port;
uid_t	userid;
int errs, rem;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#ifdef WIN32
struct _utimbuf times;
#endif

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

#ifdef KERBEROS
int	 kerberos(char **, char *, char *, char *);
void	 oldw(const char *, ...);
#endif
int	 response(void);
void	 rsource(char *,  pbs_stat_struct *);
void	 sink(int, char *[]);
void	 source(int, char *[]);
void	 tolocal(int, char *[]);
void	 toremote(char *, int, char *[]);
void	 usage(void);

#ifdef USELOG
#define       exit    use_exit
static void	      use_logusage();
static void          use_prep_timer();
static float         use_get_wtime();        /* wall clock time */
static void          use_tvsub();
static void          use_exit();
struct hostent        *gethostbyname();
struct hostent        *gethostbyaddr();
struct timeval        use_time0;              /* Time at which timing started */
struct hostent        *use_host_rec;
struct sockaddr_in    use_sock_rec;
int           use_namelen;
char          use_message[160];
char          use_host[50];
char          use_user[20];
char          use_direction[2];       /* Put/Send or Get/Receive a file */
float         use_size = 0.0;         /* Size of transfer in MBytes */
float         use_rate = 0.1;         /* MBytes/Second */
int           use_status = 0;
int           use_filec = 0;
float         use_wctime = 0.1;
int           use_neterr = 0;
#endif	/* USELOG */

int
main(int argc, char *argv[])
{
	struct servent *sp;
	int ch, fflag, tflag;
	char *targ, *shell;
	extern int optind;
#ifdef WIN32
	DWORD  cnt;
	DWORD  a_cnt = 0;
#else
	size_t cnt;
	size_t a_cnt = 0;
#endif
	/* pbs_version is used in main only, so no need to declare it as a global variable*/
	char pbs_version[] = PBS_VERSION;
#ifdef USELOG
	use_prep_timer();
	strcpy(use_user, "no_user");
	strcpy(use_host, "no_host");
	strcpy(use_direction, "E");              /* default is Error */
#endif        /* USELOG */

	/*the real deal or output pbs_version and exit?*/

	if (argc == 2 && strcasecmp(argv[1], "--version") == 0) {

		fprintf(stdout, "pbs_version = %s\n", pbs_version);
		exit(0);
	}
	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != EOF)
		switch (ch) {			/* User-visible flags. */
			case 'K':
#ifdef KERBEROS
				use_kerberos = 0;
#endif
				break;
#ifdef	KERBEROS
			case 'k':
				dest_realm = dst_realm_buf;
				(void)strncpy(dst_realm_buf, optarg, REALM_SZ);
				break;
#ifdef CRYPT
			case 'x':
				doencrypt = 1;
				/* des_set_key(cred.session, schedule); */
				break;
#endif
#endif
			case 'p':
				pflag = 1;
				break;
			case 'r':
				iamrecursive = 1;
				break;
				/* Server options. */
			case 'd':
				targetshouldbedirectory = 1;
				break;
			case 'f':			/* "from" */
				iamremote = 1;
				fflag = 1;
				break;
			case 't':			/* "to" */
				iamremote = 1;
				tflag = 1;
				break;
			case 'E':                       /* "encrypted password" */

				cnt = sizeof(size_t);
#ifdef WIN32
				ReadFile(GetStdHandle(STD_INPUT_HANDLE),
					(char *)&credl, cnt, &a_cnt, NULL);
#else
				a_cnt=fread((char *)&credl, sizeof(char), cnt,
					stdin);
#endif
				if (cnt != a_cnt) {
					fprintf(stderr,
						"failed to read in credlen. expect %ld got %ld",
#ifdef NAS /* localmod 005 */
						(long)cnt, (long)a_cnt);
#else
						cnt, a_cnt);
#endif /* localmod 005 */
					exit(1);
				}

				if (credl == 0) {
					fprintf(stderr, "credlen=0\n");
					exit(1);
				}


				credb = (char *)malloc(credl);
				if (credb == NULL) {
					fprintf(stderr, "failed to malloc cred buf...");
					exit(1);
				}

				a_cnt = 0;
#ifdef WIN32
				ReadFile(GetStdHandle(STD_INPUT_HANDLE), (char *)credb,
					credl, &a_cnt, NULL);
#else
				a_cnt=fread((char *)credb, sizeof(char), credl,
					stdin);
#endif

				if (credl != a_cnt) {
					fprintf(stderr,
						"failed to read in cred...expect %ld got %ld\n",
#ifdef NAS /* localmod 005 */
						(long)credl, (long)a_cnt);
#else
						credl, a_cnt);
#endif /* localmod 005 */
					exit(-1);
				}
				break;

			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;

#ifdef WIN32
	winsock_init();
#endif

#ifdef KERBEROS
	if (use_kerberos) {
#ifdef CRYPT
		shell = doencrypt ? "ekshell" : "kshell";
#else
		shell = "kshell";
#endif
		if ((sp = getservbyname(shell, "tcp")) == NULL) {
			use_kerberos = 0;
			oldw("can't get entry for %s/tcp service", shell);
			sp = getservbyname(shell = "shell", "tcp");
		}
	} else
		sp = getservbyname(shell = "shell", "tcp");
#else
	sp = getservbyname(shell = "shell", "tcp");
#endif
	if (sp == NULL)
		errx(1, "%s/tcp: unknown service", shell);
	port = sp->s_port;

	if ((pwd = getpwuid(userid = getuid())) == NULL)
		errx(1, "unknown user %d", (int)userid);

#ifdef        USELOG
	strcpy(use_user, pwd->pw_name);
#endif        /* USELOG */

	rem = STDIN_FILENO;

	if (fflag) {			/* Follow "protocol", send data. */
		(void)response();
#ifndef WIN32
		if (setuid(userid) == -1)
			exit(-11);
#endif
		source(argc, argv);
		exit(errs);
	}

	if (tflag) {			/* Receive data. */
#ifndef WIN32
		if (setuid(userid) == -1)
			exit(-12);
#endif
		sink(argc, argv);
		exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	/* Command to be executed on remote system using "rsh". */
#ifdef	KERBEROS
	(void)sprintf(cmd,
		"rcp%s%s%s%s", iamrecursive ? " -r" : "",
#ifdef CRYPT
		(doencrypt && use_kerberos ? " -x" : ""),
#else
		"",
#endif
		pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");
#else
	(void)sprintf(cmd, "rcp%s%s%s",
		iamrecursive ? " -r" : "", pflag ? " -p" : "",
		targetshouldbedirectory ? " -d" : "");
#endif

#ifndef WIN32
	(void)signal(SIGPIPE, lostconn);
#endif

#ifdef WIN32
	if (!(IS_UNCPATH(argv[argc - 1])) && \
		((targ = colon(argv[argc - 1])) != NULL))
#else
	if ((targ = colon(argv[argc - 1])) != NULL)
#endif
		/* Dest is non-UNC remote path */
		toremote(targ, argc, argv);
	else {
		/* Dest is local host or UNC path */
		tolocal(argc, argv);
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
}

/**
 * @brief
 *	strips the uid port number and other details required for authentication,
 *	using rcmd or kerboros authentication mechanism tries to execute commands on remote system.
 *
 * @param[in] targ - target path
 * @param[in] argc - num of args
 * @param[in] argv - pointer to array of args
 *
 * @return	Void
 *
 */
void
toremote(char *targ, int argc, char *argv[])
{
	int i, len;
	char *bp, *host, *src, *suser, *thost, *tuser;

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if ((thost = strchr(argv[argc - 1], '@')) != NULL) {
		/* user@host */
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser)) {
			if (credb != NULL) {
				(void)free(credb);
				credb = NULL;
			}
			exit(1);
		}
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}

#ifdef USELOG
	use_host_rec = gethostbyname(thost);
	if (use_host_rec)
		strcpy(use_host, use_host_rec->h_name);
	else
		strcpy(use_host, thost);
#endif        /* USELOG */

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = strchr(argv[i], '@');
			len = strlen(_PATH_RSH) + strlen(argv[i]) +
				strlen(src) + (tuser ? strlen(tuser) : 0) +
			strlen(thost) + strlen(targ) + CMDNEEDS + 20;
			if (!(bp = malloc(len)))
				err(1, NULL);
			if (host) {
				*host++ = 0;
				suser = argv[i];
				if (*suser == '\0')
					suser = pwd->pw_name;
				else if (!okname(suser))
					continue;
				(void)sprintf(bp,
					"%s %s -l %s -n %s %s '%s%s%s:%s'",
					_PATH_RSH, host, suser, cmd, src,
					tuser ? tuser : "", tuser ? "@" : "",
					thost, targ);
			} else
				(void)sprintf(bp,
#ifdef WIN32
					"%s %s -n %s %s '%s%s%s:%s'",
#else
					"exec %s %s -n %s %s '%s%s%s:%s'",
#endif
					_PATH_RSH, argv[i], cmd, src,
					tuser ? tuser : "", tuser ? "@" : "",
					thost, targ);
			(void)susystem(bp, userid, pwd->pw_name);
			(void)free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				len = strlen(targ);
				if((targ[len-1] == '/') ||
					(targ[len-1] == '\\'))
					targ[len-1] = '\0';
				len = len + CMDNEEDS + 20;
				if (!(bp = malloc(len)))
					err(1, NULL);
				(void)sprintf(bp, "%s -t %s", cmd, targ);
				host = thost;
#ifdef KERBEROS
				if (use_kerberos)
					rem = kerberos(&host, bp,
						pwd->pw_name,
						tuser ? tuser : pwd->pw_name);
				else
#endif
#ifdef WIN32
					rem = rcmd2(&host, port, pwd->pw_name,
						tuser ? tuser : pwd->pw_name,
						credb, credl, bp, 0);
#else
					rem = rcmd(&host, port, pwd->pw_name,
						tuser ? tuser : pwd->pw_name,
						bp, 0);
#endif

				if (rem < 0) {
					if (credb != NULL) {
						(void)free(credb);
						credb = NULL;
					}
					exit(1);
				}
				if (response() < 0) {
					if (credb != NULL) {
						(void)free(credb);
						credb = NULL;
					}
					exit(1);
				}
				(void)free(bp);
#ifndef WIN32
				if (setuid(userid) == - 1)
					exit(-14);
#endif
			}
			source(1, argv+i);
		}
	}
	if (credb != NULL) {
		(void)free(credb);
		credb = NULL;
	}
}

/**
 * @brief
 *      using rcmd or kerboros authentication mechanism tries to execute
 *	commands on local machine
 *
 * @param[in] argc - num of args
 * @param[in] argv - pointer to array of args
 *
 * @return      Void
 *
 */

void
tolocal(int argc, char *argv[])
{
	int i, len;
	char *bp, *host, *src, *suser;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {		/* Local to local. */
			len = strlen(_PATH_CP) + strlen(argv[i]) +
				strlen(argv[argc - 1]) + 20;
			if (!(bp = malloc(len)))
				err(1, NULL);
#ifdef WIN32
			(void)sprintf(bp, "cmd /c %s%s%s %s %s", _PATH_CP,
#else
			(void)sprintf(bp, "exec %s%s%s %s %s", _PATH_CP,
#endif
				iamrecursive ? " -r" : "", pflag ? " -p" : "",
				argv[i], argv[argc - 1]);
			if (susystem(bp, userid, pwd->pw_name))
				++errs;
			(void)free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strchr(argv[i], '@')) == NULL) {
			host = argv[i];
			suser = pwd->pw_name;
		} else {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwd->pw_name;
			else if (!okname(suser))
				continue;
		}
		len = strlen(src) + CMDNEEDS + 20;
		if ((bp = malloc(len)) == NULL)
			err(1, NULL);

#ifdef  USELOG
		use_host_rec = gethostbyname(host);
		if (use_host_rec)
			strcpy(use_host, use_host_rec->h_name);
		else
			strcpy(use_host, host);
		strcpy(use_user, suser);
#endif  /* USELOG */

		(void)sprintf(bp, "%s -f %s", cmd, src);
		rem =
#ifdef KERBEROS
		use_kerberos ?
			kerberos(&host, bp, pwd->pw_name, suser) :
#endif

#ifdef WIN32
			rcmd2(&host, port, pwd->pw_name, suser, credb, credl,
			bp, 0);
#else
			rcmd(&host, port, pwd->pw_name, suser, bp, 0);
#endif
		(void)free(bp);
		if (rem < 0) {
			++errs;
			continue;
		}
#if defined(HAVE_SETRESUID)
#define seteuid(e) (setresuid(-1, (e), -1))
#endif


#ifdef WIN32
		sink(1, argv + argc - 1);
		(void)closesocket(rem);
#else
		if (seteuid(userid) == -1)
			exit(15);
		sink(1, argv + argc - 1);
		if (seteuid(0) == -1)
			exit(15);
		(void)close(rem);
#endif

		rem = -1;
	}

	if (credb != NULL) {
		(void)free(credb);
		credb = NULL;
	}
}

/**
 * @brief
 *	Send requested file(s) (or directory(s)) information to rshd server
 *
 * @param[in]	argc - no. of arguments
 * @param[in]	argv - arguments which contains requested file informations
 *
 * @return	void
 */
void
source(int argc, char *argv[])
{
	pbs_stat_struct stb = {0};
	static BUF buffer = {0};
	BUF *bp = NULL;
	off_t_pbs i = 0;
	int amt = 0;
	int fd = 0;
	int haderr = 0;
	int indx = 0;
	int result = 0;
	char *last = NULL;
	char *name = NULL;
	char buf[RCP_BUFFER_SIZE] = {'\0'};

	for (indx = 0; indx < argc; ++indx) {
		name = argv[indx];
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;

#ifdef WIN32
		setmode(fd, O_BINARY);
		/* windows will fail to open file if a dir. To capture dir */
		/* info, we also do a pbs_stat() */
		syserr:		if (pbs_stat(name, &stb)) {
			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
#else
		if (fstat(fd, &stb)) {
			syserr:			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
#endif

		switch (stb.st_mode & S_IFMT) {
			case S_IFREG:
				break;
			case S_IFDIR:
				if (iamrecursive) {
					rsource(name, &stb);
					goto next;
				}
				/* FALLTHROUGH */
			default:
				run_err("%s: not a regular file", name);
				goto next;
		}
		if ((last = strrchr(name, '/')) == NULL)
			last = name;
		else
			++last;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void)sprintf(buf, "T%ld 0 %ld 0\n",
				(long)stb.st_mtime, (long)stb.st_atime);

#ifdef WIN32
			(void)send(rem, buf, strlen(buf), 0);
#else
			(void)write(rem, buf, strlen(buf));
#endif
			if (response() < 0)
				goto next;
		}

#ifdef WIN32
#define	MODMASK	(S_IRWXU|S_IRWXG|S_IRWXO)
#else
#define	MODMASK	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#ifdef WIN32
		(void)sprintf(buf, "C%04o %I64d %s\n",
			stb.st_mode & MODMASK, stb.st_size, last);
#else
		(void)sprintf(buf, "C%04o %ld %s\n",
			stb.st_mode & MODMASK, (long)stb.st_size, last);
#endif


#ifdef WIN32
		(void)send(rem, buf, strlen(buf), 0);
#else
		(void)write(rem, buf, strlen(buf));
#endif
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer, fd, RCP_BUFFER_SIZE)) == NULL) {
			next:			if (fd > 0)(void)close(fd);
			continue;
		}

		/* Keep writing after an error so that we stay sync'd up. */
		haderr = 0;
		for (i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = (int)(stb.st_size - i);
			if (!haderr) {
				result = read(fd, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
			if (haderr)

#ifdef WIN32
				(void)send(rem, bp->buf, amt, 0);
#else
				(void)write(rem, bp->buf, amt);
#endif
			else {
#ifdef WIN32
				result = send(rem, bp->buf, amt, 0);
#else
				result = write(rem, bp->buf, amt);
#endif
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
		}
		if (close(fd) && !haderr)
			haderr = errno;

#ifdef USELOG                                   /* MegaBytes */
		use_filec++;
		use_size += stb.st_size/1048576.0;
		strcpy(use_direction, "P");
		use_namelen = sizeof(use_sock_rec);

		if (!strcmp(use_host, "no_host")) {
			if (!getpeername(rem, (struct sockaddr*)&use_sock_rec,
				&use_namelen))
			use_host_rec = \
				   gethostbyaddr((char*)&use_sock_rec.sin_addr,
					sizeof(use_sock_rec.sin_addr),
					AF_INET);
			if (use_host_rec)
				strcpy(use_host, use_host_rec->h_name);
			else
				strcpy(use_host,
					inet_ntoa(use_sock_rec.sin_addr));
		}
#endif        /* USELOG */

		if (!haderr)

#ifdef WIN32
			(void)send(rem, "", 1, 0);
#else
			(void)write(rem, "", 1);
#endif
		else
			run_err("%s: %s", name, strerror(haderr));
		(void)response();
	}
}

/**
 *
 *  @brief Send directory information to remote host.
 *
 *  @param[in]  name - directory name.
 *  @param[in]  statp - pointer to struct stat.
 *
 *  @return void
 */
void
rsource(char *name, pbs_stat_struct *statp)
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[MAXPATHLEN];

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == 0) {
#ifdef WIN32
		/* '/' not found so check for '\\' in windows*/
		last = strrchr(name, '\\');
		if (last == 0)
			last = name;
		else
			last++;
#else
		last = name;
#endif
	} else
		last++;
	if (pflag) {
		(void)sprintf(path, "T%ld 0 %ld 0\n",
			(long)statp->st_mtime, (long)statp->st_atime);
#ifdef WIN32
		(void)send(rem, path, strlen(path), 0);
#else
		(void)write(rem, path, strlen(path));
#endif
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	(void)sprintf(path,
		"D%04o %d %s\n", statp->st_mode & MODMASK, 0, last);

#ifdef WIN32
	(void)send(rem, path, strlen(path), 0);
#else
	(void)write(rem, path, strlen(path));
#endif
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while (errno = 0, (dp = readdir(dirp)) != NULL) {

#ifndef WIN32
		if (dp->d_ino == 0)
			continue;
#endif
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= (size_t)(MAXPATHLEN - 1)) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		(void)sprintf(path, "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	if (errno != 0 && errno != ENOENT) {
		run_err("%s: %s", name, strerror(errno));
		(void)closedir(dirp);
		return;
	}
	(void)closedir(dirp);

#ifdef WIN32
	(void)send(rem, "E\n", 2, 0);
#else
	(void)write(rem, "E\n", 2);
#endif
	(void)response();
}

/**
 * @brief
 *	Receive file(s) (or directory(s)) informations sent by rshd server
 *
 * @param[in]	argc - no. arguments
 * @param[in]	argv - arguments which contains target information
 *
 * @return	void
 */
void
sink(int argc, char *argv[])
{
	static BUF buffer = {0};
	pbs_stat_struct stb = {0};
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	BUF *bp = NULL;
	off_t j = 0;
	int_pbs size = 0;
	int_pbs i = 0;
	int amt = 0;
	int count = 0;
	int exists = 0;
	int first = 0;
	int mask = 0;
	int mode = 0;
	int ofd = 0;
	int omode = 0;
	int setimes = 0;
	int targisdir = 0;
	int wrerrno = 0;
	char ch = '\0';
	char *cp = NULL;
	char *np = NULL;
	char *targ = NULL;
	char *why = NULL;
	char *vect[1] = {0};
	char buf[RCP_BUFFER_SIZE] = {'\0'};

	memset(tv, 0, sizeof(tv));

#ifdef USELOG
	pbs_stat_struct use_stb;
#endif        /* USELOG */

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str) { why = str; goto screwup; }

#ifdef USELOG
	strcpy(use_direction, "G");
#endif        /* USELOG */

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void)umask(mask);
	if (argc != 1) {
		run_err("ambiguous target");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);

#ifdef WIN32
	(void)send(rem, "", 1, 0);
#else
	(void)write(rem, "", 1);
#endif
	if (pbs_stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;

#ifdef WIN32
		if (recv(rem, cp, 1, 0) <= 0)
#else
		if (read(rem, cp, 1) <= 0)
#endif
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
#ifdef WIN32
			if (recv(rem, &ch, sizeof(ch), 0) != sizeof(ch))
#else
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
#endif
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[RCP_BUFFER_SIZE - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void)write(STDERR_FILENO,
					buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
#ifdef WIN32
			(void)send(rem, "", 1, 0);
#else
			(void)write(rem, "", 1);
#endif
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");

#ifdef WIN32
			(void)send(rem, "", 1, 0);
#else
			(void)write(rem, "", 1);
#endif
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				run_err("%s", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");

		for (size = 0; isdigit(*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > (unsigned)cursize) {
				if (!(namebuf = malloc(need)))
					run_err("%s", strerror(errno));
			}
			(void)sprintf(namebuf, "%s%s%s", targ,
				*targ ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		exists = pbs_stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void)chmod(np, mode);
			} else {
				/* Handle copying from a read-only directory */
				mod_flag = 1;

#ifdef WIN32
				/* use _mkdir as it supports UNC path */
				if (_mkdir(np) == -1)
#else
				if (mkdir(np, mode | S_IRWXU) < 0)
#endif
					goto bad;
			}
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
#ifdef WIN32
				times.actime = atime.tv_sec;
				times.modtime = mtime.tv_sec;

				if (_utime(np, &times) < 0)
#else
				if (utimes(np, tv) < 0)
#endif
					run_err("%s: set times: %s",
						np, strerror(errno));
			}
			if (mod_flag)
				(void)chmod(np, mode);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;

#ifdef WIN32
		if ((ofd = open(np, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, mode)) < 0)
#else
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0)
#endif
		{
			bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}


#ifdef WIN32
		(void)send(rem, "", 1, 0);
#else
		(void)write(rem, "", 1);
#endif
		if ((bp = allocbuf(&buffer, ofd, RCP_BUFFER_SIZE)) == NULL) {
			(void)close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;
		count = 0;
		for (i = 0; i < size; i += RCP_BUFFER_SIZE) {
			amt = RCP_BUFFER_SIZE;
			if (i + amt > size)
				amt = (int)(size - i);
			count += amt;
			do {

#ifdef WIN32
				j = recv(rem, cp, amt, 0);
#else
				j = read(rem, cp, amt);
#endif
				if (j <= 0) {
					run_err("%s", j ? strerror(errno) :
						"dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					j = write(ofd, bp->buf, (unsigned int)count);
					if (j != count) {
						wrerr = YES;
						wrerrno = j >= 0 ? EIO : errno;
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == NO &&
			(j = write(ofd, bp->buf, (unsigned int)count)) != count) {
			wrerr = YES;
			wrerrno = j >= 0 ? EIO : errno;
		}


#ifdef WIN32
		if (pflag) {
			if (exists || omode != mode)
				if (_chmod(np, omode))
					run_err("%s: set mode: %s",
						np, strerror(errno));
		} else {
			if (!exists && omode != mode)
				if (_chmod(np, omode & ~mask))
					run_err("%s: set mode: %s",
						np, strerror(errno));
		}
#else
		if (ftruncate(ofd, size)) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
				if (fchmod(ofd, omode))
					run_err("%s: set mode: %s",
						np, strerror(errno));
		} else {
			if (!exists && omode != mode)
				if (fchmod(ofd, omode & ~mask))
					run_err("%s: set mode: %s",
						np, strerror(errno));
		}
#endif
		(void)close(ofd);

#ifdef        USELOG
		use_filec++;
		pbs_stat(np, &use_stb);
		use_size += use_stb.st_size/1048576.0;
		use_namelen = sizeof(use_sock_rec);

		if (!strcmp(use_host, "no_host")) {
			if (!getpeername(rem, (struct sockaddr*)&use_sock_rec,
				&use_namelen))
			use_host_rec = \
				   gethostbyaddr((char*)&use_sock_rec.sin_addr,
					sizeof(use_sock_rec.sin_addr), AF_INET);
			if (use_host_rec)
				strcpy(use_host, use_host_rec->h_name);
			else
				strcpy(use_host, inet_ntoa(use_sock_rec.sin_addr));
		}

#endif        /* USELOG */

		(void)response();
		if (setimes && wrerr == NO) {
			setimes = 0;

#ifdef WIN32
			times.actime = atime.tv_sec;
			times.modtime = mtime.tv_sec;

			if (_utime(np, &times) < 0)
#else
			if (utimes(np, tv) < 0)
#endif
			{
				run_err("%s: set times: %s",
					np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch (wrerr) {
			case YES:
				run_err("%s: %s", np, strerror(wrerrno));
				break;
			case NO:
#ifdef WIN32
				(void)send(rem, "", 1, 0);
#else
				(void)write(rem, "", 1);
#endif
				break;
			case DISPLAYED:
				break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
#ifdef USELOG
	use_neterr++;
#endif        /* USELOG */

	exit(1);
}

#ifdef KERBEROS

/**
 * @brief
 *	provides kerberos authentication data.
 *
 * @param[in] host - host name
 * @param[in] bp -
 * @param[in] localuser - present working user
 * @param[in] user - username
 *
 * @return	int
 * @retval	communication handle	success
 * @retval	-1			error
 *
 */
int
kerberos(char **host, char *bp, char *locuser, char *user)
{
	struct servent *sp;

again:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(*host);
		rem =
#ifdef CRYPT
		doencrypt ?
			krcmd_mutual(host, port, user, bp, 0, dest_realm, &cred, schedule) :
#endif
			krcmd(host, port, user, bp, 0, dest_realm);

		if (rem < 0) {
			use_kerberos = 0;
			if ((sp = getservbyname("shell", "tcp")) == NULL)
				errx(1, "unknown service shell/tcp");
			if (errno == ECONNREFUSED)
				oldw("remote host doesn't support Kerberos");
			else if (errno == ENOENT)
				oldw("can't provide Kerberos authentication data");
			port = sp->s_port;
			goto again;
		}
	} else {
#ifdef CRYPT
		if (doencrypt)
			errx(1,
				"the -x option requires Kerberos authentication");
#endif
		rem = rcmd(host, port, locuser, user, bp, 0);
	}
	return (rem);
}
#endif /* KERBEROS */

/**
 * @brief
 *	Receive response of last operation from rshd server
 *		response will be as follow:
 *		0 - OK
 *		1 - error, followed by error message
 *		2 - fatal error followed by ""
 *
 * @return	int
 * @retval	0	OK
 * @retval	-1	error
 *
 * @par	Note:
 *	on receiving fatal error response will do exit with 1.
 */
int
response()
{
	char ch = '\0';
	char *cp = NULL;
	char resp = '\0';
	char rbuf[RCP_BUFFER_SIZE] = {'\0'};

#ifdef WIN32
	if (recv(rem, &resp, sizeof(resp), 0) != sizeof(resp))
#else
	if (read(rem, &resp, sizeof(resp)) != sizeof(resp))
#endif
		lostconn(0);

	cp = rbuf;
	switch (resp) {
		case 0:				/* ok */
			return (0);
		default:
			*cp++ = resp;
			/* FALLTHROUGH */
		case 1:				/* error, followed by error msg */
		case 2:				/* fatal error, "" */
			do {

#ifdef WIN32
				if (recv(rem, &ch, sizeof(ch), 0) != sizeof(ch))
#else
				if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
#endif
					lostconn(0);
				*cp++ = ch;
			} while (cp < &rbuf[RCP_BUFFER_SIZE] && ch != '\n');

			if (!iamremote)
				(void)write(STDERR_FILENO, rbuf, cp - rbuf);
			++errs;
			if (resp == 1)
				return (-1);
			exit(1);
	}
	/* NOTREACHED */
}

/**
 * @brief
 *	prints the usage guidelines for using pbs_rcp.
 *
 */
void
usage()
{
#ifdef KERBEROS
#ifdef CRYPT
	(void)fprintf(stderr, "%s\n\t%s\n",
		"usage: pbs_rcp [-Kpx] [-k realm] f1 f2",
		"or: pbs_rcp [-Kprx] [-k realm] f1 ... fn directory");
#else
	(void)fprintf(stderr, "%s\n\t%s\n",
		"usage: pbs_rcp [-Kp] [-k realm] f1 f2",
		"or: pbs_rcp [-Kpr] [-k realm] f1 ... fn directory");
#endif
#else
	(void)fprintf(stderr,
		"usage: pbs_rcp [-E] [-p] f1 f2; or: pbs_rcp [-pr] f1 ... fn directory\n");
#endif
	(void)fprintf(stderr,
		"       pbs_rcp --version\n");
	exit(1);
}

#include <stdarg.h>

#ifdef KERBEROS
/**
 * @brief
 *	prints error message to stderr in provided format.
 *
 * @param[in] fmt - error message to be logged
 *
 */
void
oldw(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	(void)fprintf(stderr, "rcp: ");
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, ", using standard rcp\n");
	va_end(ap);
}
#endif

/**
 * @brief
 *      prints error message to specified file in provided format.
 *
 * @param[in] fmt - error message to be logged
 *
 */
void
run_err(const char *fmt, ...)
{
	static FILE *fp;
	va_list ap;
	va_start(ap, fmt);

	++errs;

	if (!iamremote) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, "\n");
	}

	if (fp == NULL && !(fp = fdopen(rem, "w"))) {
		va_end(ap);
		return;
	}
	(void)fprintf(fp, "%c", 0x01);
	(void)fprintf(fp, "rcp: ");
	(void)vfprintf(fp, fmt, ap);
	(void)fprintf(fp, "\n");
	(void)fflush(fp);

	va_end(ap);
}

#ifdef USELOG
/**
 * @brief
 *	generates a distributed log message.
 */
static void
use_logusage(char *mes, char *app)
{
	closelog();
	openlog(app, LOG_INFO|LOG_NDELAY, LOG_LOCAL4);
	syslog(LOG_INFO, "%s", mes);
	closelog();
}

/**
 * @brief
 *	returns the time and timezone info.
 */
static void
use_prep_timer()
{
	gettimeofday(&use_time0, NULL);
}

/**
 * @brief
 *	returns the time in seconds and microseconds format.
 *
 */
static float
use_get_wtime()
{
	struct timeval timedol;
	struct timeval td;
	float realt;

	gettimeofday(&timedol, NULL);

	/* Get real time */
	use_tvsub(&td, &timedol, &use_time0);
	realt = td.tv_sec + ((double)td.tv_usec) / 1000000;
	if (realt < 0.00001) realt = 0.00001;

	return (realt);
}

/**
 * @brief
 *	calculate the time difference of t1 and t0.
 *
 * @param[in] t1 - time value1
 * @param[in] t1 - time value2
 *
 * @return	Void
 */
static void
use_tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

/**
 * @brief
 *	end up a communication by logging appropriate msg.
 *
 * @param[in] status - status code
 *
 */
void
use_exit(int status)
{
	/*   UNICOS tcp/ip socket error codes */
	if (status && (127 < errno && errno < 156)) {
		strcpy(use_direction, "E");
		use_status    = errno;
		use_neterr++;
	}
	else
		use_status = status;

	/* only report files bigger than 0.001MB or network problems */
	if (use_size > 0.001 || use_neterr) {
		use_wctime    = use_get_wtime();
		use_rate      = use_size / use_wctime;

		sprintf(use_message,
			"%-25s %-9s %-2s %5.3f %5.3f %2d %2d\n",
			use_host, use_user, use_direction, use_size,
			use_rate, use_status, use_filec);

		use_logusage(use_message, "rcp");
	}

#undef exit
	exit(status);
}
#endif	/* USELOG */
