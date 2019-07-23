/*
 * Copyright (C) 1994-2019 Altair Engineering, Inc.
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
/**
 * @file	pbs_password.c
 * @brief
 *	This program exists to give a way to update
 *	a user's password to the server.
 *
 * @par	usage:	pbs_passwd [-s server] [-r] [-d] [user]
 */

#include	<pbs_config.h>   /* the master config generated by configure */
#include	<pbs_version.h>
#include	<assert.h>
#include	<pwd.h>
#include	<unistd.h>
#include	<sys/types.h>

#include	"cmds.h"
#include	"portability.h"
#include	"pbs_ifl.h"
#include	"libpbs.h"

#include "credential.h"
#include "ticket.h"

int quiet = 0;
int	cred_type;
size_t	cred_len;
char	*cred_buf = NULL;


/**
 * @brief
 *	sets the attribute details
 *
 * @param[out] list - attribute list
 * @param[in] a_name - attribute name
 * @param[in] r_name - resource name
 * @param[in] v_name - value for the attribute
 *
 * @return Void
 *
 */
static void
set_attrop(struct attropl **list, char *a_name, char *r_name, char *v_name, enum batch_op op)
{
	struct attropl *attr;

	attr = (struct attropl *) malloc(sizeof(struct attropl));
	if (attr == NULL) {
		fprintf(stderr, "qselect: out of memory\n");
		CS_close_app();
		exit(2);
	}
	if (a_name == NULL)
		attr->name = NULL;
	else {
		attr->name = (char *) malloc(strlen(a_name)+1);
		if (attr->name == NULL) {
			fprintf(stderr, "qselect: out of memory\n");
			CS_close_app();
			exit(2);
		}
		strcpy(attr->name, a_name);
	}
	if (r_name == NULL)
		attr->resource = NULL;
	else {
		attr->resource = (char *) malloc(strlen(r_name)+1);
		if (attr->resource == NULL) {
			fprintf(stderr, "qselect: out of memory\n");
			CS_close_app();
			exit(2);
		}
		strcpy(attr->resource, r_name);
	}
	if (v_name == NULL)
		attr->value = NULL;
	else {
		attr->value = (char *) malloc(strlen(v_name)+1);
		if (attr->value == NULL) {
			fprintf(stderr, "qselect: out of memory\n");
			CS_close_app();
			exit(2);
		}
		strcpy(attr->value, v_name);
	}
	attr->op = op;
	attr->next = *list;
	*list = attr;
	return;
}
/**
 * @brief
 *	The main function in C - entry point
 *
 * @param[in]  argc - argument count
 * @param[in]  argv - pointer to argument array
 *
 * @return  int
 * @retval  0 - success
 * @retval  !0 - error
 */
int
main(int argc, char *argv[])
{
	char	*the_server;
#ifdef WIN32
	char	the_user[UNLEN+1];
	char	cur_user[UNLEN+1];
#else
	/* To store the uid of the current process */
	struct	passwd 	*pwd = NULL;
	char	the_user[PBS_MAXUSER+1];
	char	cur_user[PBS_MAXUSER+1];
#endif
	int	con;
	int	 errflg = 0;
	int	i;

	extern	char	*optarg;
	extern	int	 optind;

	int     ret, rc = 0;
	int	release_jobs = 0;
	int	delete_user = 0;
	char    *errmsg;

#if	defined(PBS_PASS_CREDENTIALS)
	int	err, j;
	char	passwd_buf[PBS_MAXPWLEN] = {'\0'};
#endif

	/*test for real deal or just version and exit*/

	execution_mode(argc, argv);

	/* do CS library initialization if that's appropriate */

	if (CS_client_init() != CS_SUCCESS) {
		fprintf(stderr,
			"pbs_password: unable to initialize security library.\n");
		exit(1);
	}

#ifdef WIN32
	winsock_init();
#endif

	/* get default server, may be changed by -s option */

	the_server = pbs_default();

#ifdef WIN32
	strcpy(the_user, getlogin());
#else
	/*
	 * On standards-compliant systems, getlogin() returns the name
	 * of the user who logged in on a terminal, not the name of
	 * the user whose UID executed the command.
	 */
	pwd = getpwuid(getuid());
	if (pwd == NULL) {
		fprintf(stderr,
			"pbs_password: unable to determine user name\n");
		exit(1);
	}
	strcpy(the_user, pwd->pw_name);
#endif	/* WIN32 */
	strcpy(cur_user, the_user);

	while ((i = getopt(argc, argv, "s:rd")) != EOF) {
		switch (i) {

			case 's':
				the_server = optarg;
				break;
			case 'r':
				release_jobs = 1;
				break;
			case 'd':
				delete_user = 1;
				break;

			case '?':
			default:
				errflg++;
		}
	}

	if (errflg) {
		fprintf(stderr,
			"\nusage:\t%s [-s server] [-r] [-d] [user]...\n",
			argv[0]);
		fprintf(stderr, "      \t%s --version\n", argv[0]);
		CS_close_app();
		exit(-7);
	}


	if (argv[optind] != NULL) {
		strcpy(the_user, argv[optind]);
#if	defined(PBS_PASS_CREDENTIALS)
		optind++;
		if (argv[optind] != NULL) {
			strncpy(passwd_buf, argv[optind], sizeof(passwd_buf));
			passwd_buf[sizeof(passwd_buf) - 1] = '\0';
		}
#endif
	}

	if (delete_user) {
		int go = 1;

		printf("You want to delete user %s's info at %s? y(es) or n(o): ",
			the_user, the_server);

		while (go) {
			int answ, c;

			answ = getchar();
			c = answ;
			while ((c != '\n') && (c != EOF))
				c = getchar();

			switch (answ) {
				case  EOF:
				case '\n':
				case 'n':
				case 'N':
					fprintf(stderr, "delete of user cancelled!\n");
					CS_close_app();
					exit(-7);
				case 'y':
				case 'Y':
					go = 0;
					break;
				default:
					printf("y(es) or n(o) please:");
			}
		}

		cred_type = PBS_CREDTYPE_AES;
		cred_buf =  strdup("");
		assert(cred_buf != NULL);
		cred_len = 0;

	} else {


		/* let's send a null, "User Credential" request to the */
		/* server, which causes some pre-checks to ocurr like */
		/* telling us if we're authorize to change password of */
		/* a particular user */

		con= cnt2server(the_server);

		if (con <= 0) {
			if (!quiet)
				fprintf(stderr,
					"%s: cannot connect to server %s, error=%d\n",
					argv[0], the_server, pbs_errno);
			CS_close_app();
			exit(-6);
		}


		ret = PBSD_ucred(con, the_user, PBS_CREDTYPE_AES, NULL, 0);

		if (ret != 0) {

			switch (ret) {
				case PBSE_SSIGNON_UNSET_REJECT:
					rc = -1;
					errmsg = pbs_geterrmsg(con);
					if (errmsg != NULL)
						fprintf(stderr, "pbs_password: %s\n",
							errmsg);
					break;
				case PBSE_PERM:
					fprintf(stderr,
						"%s not authorized to change password of %s\n",
						cur_user, the_user);
					rc = -5;
					break;
				default:
					rc = 0;
			}
		}
		pbs_disconnect(con);

		if (rc != 0) { /* pre-check unearthed an error */
			CS_close_app();
			exit(rc);
		}

#if	defined(PBS_PASS_CREDENTIALS)
		if (passwd_buf[0] == '\0') {
			for (j=0; j<3; j++) {
				err = EVP_read_pw_string(passwd_buf,
					sizeof(passwd_buf),
					"Enter user's password: ", 1);
				if (err == 0)
					break;
			}
			if (j == 3) {
				fprintf(stderr,
					"%s failed: max retries reached!\n", argv[0]);
				CS_close_app();
				exit(-11);
			}
		}

		pbs_encrypt_pwd(passwd_buf, &cred_type, &cred_buf, &cred_len);

		if ((cred_buf != NULL) && (strcmp(cred_buf, "") == 0) &&
			(cred_len == 0)) {
			fprintf(stderr, "passworded encrypted to empty!");
			CS_close_app();
			exit(-8);
		}

#else
		fprintf(stderr, "%s failed: not compiled with PBS_PASS_CREDENTIALS!\n", argv[0]);
		CS_close_app();
		exit(-10);
#endif

	}

	con= cnt2server(the_server);
	if (con <= 0) {
		if (!quiet)
			fprintf(stderr, "%s: cannot connect to server %s, error=%d\n",
				argv[0], the_server, pbs_errno);
		CS_close_app();
		exit(-6);
	}


	ret = PBSD_ucred(con, the_user, cred_type, cred_buf, cred_len);

	if (ret == 0) {

		if (delete_user)
			printf("user %s's info deleted on server %s\n",
				the_user, the_server);
		else
			printf("Changed user %s's PBS password on server %s\n",
				the_user, the_server);
	} else {
		switch (ret) {
			case PBSE_SSIGNON_UNSET_REJECT:
				rc = -1;
				errmsg = pbs_geterrmsg(con);
				if (errmsg != NULL)
					fprintf(stderr, "pbs_password: %s\n",
						errmsg);
				break;
			case PBSE_PERM:
				fprintf(stderr,
					"%s not authorized to change password of %s\n",
					cur_user, the_user);
				rc = -5;
				break;
			case PBSE_SYSTEM:
			case PBSE_BADUSER:
			default:
				if (delete_user) {

					if (ret == PBSE_BADUSER) {
						fprintf(stderr,
							"No password exists for user %s on %s\n",
							the_user, the_server);
					} else {
						fprintf(stderr,
							"failed to delete password of user %s on %s\n",
							the_user, the_server);
					}

					rc = -4;
				} else {
					fprintf(stderr,
						"password of user %s on %s failed to be "
						"created/updated\n", the_user, the_server);
					rc = -2;
				}
		}
	}

	if (cred_buf) {
		(void)free(cred_buf);
	}
	cred_buf = NULL;

	/* release_jobs will not mimic qrls -h p `qselect -h p` since */
	/* the qselect will only match the p hold type exactly. It won't */
	/* match jobs that have more than one hold type. So the solution */
	/* here is to qselect -s H (held state), and apply a qrls -h p */
	/* on each job listed. */
	if (release_jobs) {
		struct attropl *select_list = 0;
		char **selectjob_list;

		set_attrop(&select_list, ATTR_u, NULL, the_user, EQ);
		set_attrop(&select_list, ATTR_state, NULL, "H", EQ);

		selectjob_list = pbs_selectjob(con, select_list, NULL);
		if (selectjob_list == NULL) {

			if (pbs_errno != PBSE_NONE) {
				errmsg = pbs_geterrmsg(con);
				if (errmsg != NULL) {
					fprintf(stderr,
						"selecting jobs to release: %s\n",
						errmsg);
				} else {
					fprintf(stderr,
						"Error (%d) selecting jobs to release\n",
						pbs_errno);
				}
				fprintf(stderr,
					"failed to release bad password held jobs by user %s on %s\n", the_user, the_server);
				rc = -3;
			}
		} else {   /* got some jobs ids */
			int i = 0;
			while (selectjob_list[i] != NULL) {
				int stat;

				stat = pbs_rlsjob(con, selectjob_list[i],
					"p", NULL);

				if (stat && (pbs_errno != PBSE_UNKJOBID)) {
					fprintf(stderr, "failed to release job %s\n",
						selectjob_list[i]);
					rc = -3;
				} else if ( stat && \
					(pbs_errno == PBSE_UNKJOBID) ) {
					fprintf(stderr, "could not find job %s\n",
						selectjob_list[i]);
					rc = -3;
				} else if (stat == 0) {
					printf("released job %s\n",
						selectjob_list[i]);
				}
				i++;
			}
			free(selectjob_list);
			PBS_free_aopl(select_list);
		}
	}

	(void)pbs_disconnect(con);

	if (rc == -3)
		printf("failed to release \"bad password\" held jobs by %s on %s though password has been changed.\n", the_user, the_server);

	CS_close_app();
	return (rc);
}
