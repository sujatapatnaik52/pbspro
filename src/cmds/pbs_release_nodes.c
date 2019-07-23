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
 * @file    pbs_release_nodes.c
 *
 * @brief
 *
 * 	Send release nodes request to batch job.
 *
 */

#include <errno.h>
#include "cmds.h"
#include "pbs_ifl.h"
#include <pbs_config.h>   /* the master config generated by configure */
#include <pbs_version.h>

#define USAGE	"usage: pbs_release_nodes [-j job_identifier] host_or_vnode1 host_or_vnode2 ...\n"
#define USAGE2	"usage: pbs_release_nodes [-j job_identifier] -a\n"
#define USAGE3	"       pbs_release_nodes --version\n"

int
main(int argc, char **argv, char **envp) /* pbs_release_nodes */
{
	int c;
	int errflg=0;
	int any_failed=0;

	char job_id[PBS_MAXCLTJOBID];       /* from the command line */
	char job_id_out[PBS_MAXCLTJOBID];
	char server_out[MAXSERVERNAME];
	char rmt_server[MAXSERVERNAME];
	int  len;
	char *node_list = NULL;
	int connect;
	int stat=0;
	int k;
	int all_opt = 0;

#define GETOPT_ARGS "j:a"

	/*test for real deal or just version and exit*/
	execution_mode(argc, argv);

#ifdef WIN32
	winsock_init();
#endif

	job_id[0] = '\0';
	while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF) {
		switch (c) {
			case 'j':
				strncpy(job_id, optarg, PBS_MAXCLTJOBID);
				job_id[PBS_MAXCLTJOBID-1] = '\0';
				break;
			case 'a':
				all_opt = 1;
				break;
			default :
				errflg++;
		}
	}
	if (job_id[0] == '\0') {
		char *jid;
		jid = getenv("PBS_JOBID");
		strncpy(job_id, jid?jid:"", PBS_MAXCLTJOBID);
		job_id[PBS_MAXCLTJOBID-1] = '\0';
	}

	if (errflg ||
		((optind == argc) && !all_opt) ||
		((optind != argc) && all_opt) ) {
		fprintf(stderr, USAGE);
		fprintf(stderr, USAGE2);
		fprintf(stderr, USAGE3);
		exit(2);
	}

	if (job_id[0] == '\0') {
		fprintf(stderr, "pbs_release_nodes: No jobid given\n");
		exit(2);
	}

	/*perform needed security library initializations (including none)*/

	if (CS_client_init() != CS_SUCCESS) {
		fprintf(stderr, "pbs_release_nodes: unable to initialize security library.\n");
		exit(2);
	}

	len = 0;
	for (k = optind; k < argc; k++) {
		len += (strlen(argv[k]) + 1);	/* +1 for space */
	}

	node_list = (char *)malloc(len + 1);
	if (node_list == NULL) {
		fprintf(stderr, "failed to malloc to store data (error %d)", errno);
		exit(2);
	}
	node_list[0] = '\0';

	for (k = optind; k < argc; k++) {
		if (k != optind)
			strcat(node_list, "+");
		strcat(node_list, argv[k]);
	}
	if (get_server(job_id, job_id_out, server_out)) {
		fprintf(stderr, "pbs_release_nodes: illegally formed job identifier: %s\n", job_id);
		free(node_list);
		exit(2);
	}

	pbs_errno = 0;
	stat = 0;
	while(1) {
		connect = cnt2server(server_out);
		if (connect <= 0) {
			fprintf(stderr,
				"pbs_release_nodes: cannot connect to server %s (errno=%d)\n",
							pbs_server, pbs_errno);
			break;
		}

		stat = pbs_relnodesjob(connect, job_id_out, node_list, NULL);
		if (stat && (pbs_errno == PBSE_UNKJOBID)) {
			if (locate_job(job_id_out, server_out, rmt_server)) {
				/*
				 * job located at a different server
				 * retry connect on the new server
				 */
				pbs_disconnect(connect);
				strcpy(server_out, rmt_server);
			} else {
				prt_job_err("pbs_release_nodes", connect, job_id_out);
				break;
			}
		} else {
			char *info_msg;

			if (stat && (pbs_errno != PBSE_UNKJOBID)) {
				prt_job_err("pbs_release_nodes", connect, "");
			} else if ((info_msg = pbs_geterrmsg(connect)) != NULL) {
				/* print potential warning message */
				printf("pbs_release_nodes: %s\n", info_msg);
			}
			break;
		}
	}
	any_failed = pbs_errno;

	pbs_disconnect(connect);

	/*cleanup security library initializations before exiting*/
	CS_close_app();

	exit(any_failed);
}
