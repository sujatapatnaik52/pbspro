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
/**
 * @file	w32_start_provision.c
 * @brief
 * The entry point function for pbs_daemon.
 *
 * @par	Included public functions are:
 *
 *	main	initialization and main loop of pbs_daemon
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/stat.h>

#include "pbs_ifl.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <io.h>
#include <windows.h>
#include "win.h"

#include "list_link.h"
#include "work_task.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "job.h"
#include "reservation.h"
#include "credential.h"
#include "ticket.h"
#include "queue.h"
#include "server.h"
#include "libpbs.h"
#include "net_connect.h"
#include "batch_request.h"
#include "pbs_nodes.h"
#include "svrfunc.h"
#include "tracking.h"
#include "acct.h"
#include "sched_cmds.h"
#include "rpp.h"
#include "dis.h"
#include "dis_init.h"
#include "pbs_ifl.h"
#include "pbs_license.h"
#include "resource.h"
#include "pbs_version.h"
#include "hook.h"
#include "pbs_python.h"
#include "provision.h"
#include "pbs_db.h"



/* Global Data Items */
char	       *acct_file = NULL;
char	       *acctlog_spacechar = NULL;
int		license_cpus;
char	       *log_file  = NULL;
char	       *path_acct;
char	        path_log[MAXPATHLEN+1];
char	       *path_priv = NULL;
char	       *path_jobs = NULL;
char	       *path_rescdef = NULL;
char	       *path_users = NULL;
char	       *path_resvs = NULL;
char	       *path_queues = NULL;
char	       *path_spool = NULL;
char	       *path_svrdb = NULL;
char	       *path_svrdb_new = NULL;
char	       *path_scheddb = NULL;
char	       *path_scheddb_new = NULL;
char 	       *path_track = NULL;
char	       *path_nodes = NULL;
char	       *path_nodes_new = NULL;
char	       *path_nodestate = NULL;
char	       *path_hooks;
char	       *path_hooks_rescdef;
char	       *path_hooks_tracking;
char	       *path_hooks_workdir;
char           *path_secondaryact;
char	       *pbs_o_host = "PBS_O_HOST";
pbs_net_t	pbs_mom_addr = 0;
unsigned int	pbs_mom_port = 0;
unsigned int	pbs_rm_port = 0;
pbs_net_t	pbs_scheduler_addr = 0;
unsigned int	pbs_scheduler_port = 0;
pbs_net_t	pbs_server_addr = 0;
unsigned int	pbs_server_port_dis = 0;
struct server	server;		/* the server structure */
struct pbs_sched	*dflt_scheduler;	/* the sched structure */
char	        server_host[PBS_MAXHOSTNAME+1];	/* host_name  */
char	        primary_host[PBS_MAXHOSTNAME+1]; /* host_name of primary */
int		shutdown_who;		/* see req_shutdown() */
char	       *mom_host = server_host;
long		new_log_event_mask = 0;
int	 	server_init_type = RECOV_WARM;
char	        server_name[PBS_MAXSERVERNAME+1]; /* host_name[:service|port] */
int		svr_delay_entry = 0;
int		svr_total_cpus = 0;		/* total number of cpus on nodes   */
int		have_blue_gene_nodes = 0;
int		svr_ping_rate = SVR_DEFAULT_PING_RATE;	/* time between sets of node pings */
int 		ping_nodes_rate = SVR_DEFAULT_PING_RATE; /* time between ping nodes as determined from server_init_type */
int		svr_unsent_qrun_req = 0;
pbs_db_conn_t	*svr_db_conn;
char		*path_svrlive;
char		*pbs_server_name;

struct license_block licenses;
struct license_used  usedlicenses;
struct resc_sum *svr_resc_sum;
attribute      *pbs_float_lic;
pbs_list_head	svr_queues;            /* list of queues                   */
pbs_list_head	svr_allscheds;         /* list of schedulers               */
pbs_list_head	svr_alljobs;           /* list of all jobs in server       */
pbs_list_head	svr_newjobs;           /* list of incomming new jobs       */
pbs_list_head	svr_allresvs;          /* all reservations in server */
pbs_list_head	svr_newresvs;          /* temporary list for new resv jobs */
pbs_list_head	svr_allhooks;	       /* list of all hooks in server       */
pbs_list_head	svr_queuejob_hooks;
pbs_list_head	svr_modifyjob_hooks;
pbs_list_head	svr_resvsub_hooks;
pbs_list_head	svr_movejob_hooks;
pbs_list_head	svr_runjob_hooks;
pbs_list_head	svr_periodic_hooks;
pbs_list_head	svr_provision_hooks;
pbs_list_head	svr_resv_end_hooks;
pbs_list_head	svr_execjob_begin_hooks;
pbs_list_head	svr_execjob_prologue_hooks;
pbs_list_head	svr_execjob_launch_hooks;
pbs_list_head	svr_execjob_epilogue_hooks;
pbs_list_head	svr_execjob_end_hooks;
pbs_list_head	svr_execjob_preterm_hooks;
pbs_list_head	svr_exechost_periodic_hooks;
pbs_list_head	svr_exechost_startup_hooks;
pbs_list_head	svr_execjob_attach_hooks;
pbs_list_head	svr_execjob_resize_hooks;
pbs_list_head	task_list_immed;
pbs_list_head	task_list_timed;
pbs_list_head	task_list_event;
pbs_list_head   	svr_deferred_req;
pbs_list_head   	svr_unlicensedjobs;	/* list of jobs to license */
time_t		time_now;
time_t          secondary_delay = 30;
int		do_sync_mom_hookfiles;
struct batch_request    *saved_takeover_req;
struct python_interpreter_data  svr_interp_data;
AVL_IX_DESC	*AVL_jctx = NULL;
/**
 * @file
 * 	Used only by the TPP layer, to ping nodes only if the connection to the
 * 	local router to the server is up.
 * 	Initially set the connection to up, so that first time ping happens
 * 	by default.
 */
int tpp_network_up = 0;

/**
 * @brief
 * 	just a dummy entry for pbs_close_stdfiles since needed by failover.obj
 */
void
pbs_close_stdfiles(void)
{
	return;
}
/**
 * @brief
 * 	needed by failover.obj
 */
void
make_server_auto_restart(confirm)
int	confirm;
{
	return;
}

/**
 * @brief
 * needed by svr_chk_owner.obj and user_func.obj
 */
int
decrypt_pwd(char *crypted, size_t len, char **passwd)
{
	return (0);
}

/**
 * @brief
 * 	needed by *_recov_db.obj
 *
 */
void
panic_stop_db(char *txt)
{
	return;
}

/**
 * @brief
 * 	main - the initialization and main loop of pbs_daemon
 */
int
main(int argc, char *argv[])
{
	int	i;
	int	rc;
	hook	*phook;
	struct prov_vnode_info prov_info;
	char	output_path[MAXPATHLEN + 1];

	/* python externs */
	extern void pbs_python_svr_initialize_interpreter_data(
		struct python_interpreter_data *interp_data);
	extern void pbs_python_svr_destroy_interpreter_data(
		struct python_interpreter_data *interp_data);

	extern int execute_python_prov_script(hook  *phook,
		struct prov_vnode_info * prov_vnode_info);

	if(set_msgdaemonname("PBS_start_provision")) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}

	/* set python interp data */
	svr_interp_data.data_initialized = 0;
	svr_interp_data.init_interpreter_data =
		pbs_python_svr_initialize_interpreter_data;
	svr_interp_data.destroy_interpreter_data =
		pbs_python_svr_destroy_interpreter_data;

	if (argc != 7) {
		log_err(PBSE_INTERNAL, __func__, "start_provision"
			" <vnode-name> <aoe-requested>");
		exit(2);
	}

	path_priv = strdup(argv[5]);
	if (path_priv == NULL) {
		log_err(errno, argv[5], "strdup failed!");
		exit(2);
	}

	svr_interp_data.daemon_name = strdup(argv[3]);
	if (svr_interp_data.daemon_name == NULL) { /* should not happen */
		log_err(errno, argv[3], "strdup failed!");
		exit(2);
	}

	pbs_loadconf(0);
	/* initialize the pointers in the resource_def array */

	for (i = 0; i < (svr_resc_size - 1); ++i)
		svr_resc_def[i].rs_next = &svr_resc_def[i+1];

	pbs_python_ext_start_interpreter(&svr_interp_data);

	/* Find the provision hook info */
	phook = (hook *)malloc(sizeof(hook));
	if (phook == NULL) {
		log_err(errno, __func__, "no memory");
		exit(2);
	}
	(void)memset((char *)phook, (int)0, (size_t)sizeof(hook));

	phook->hook_name = strdup(argv[4]);
	if (phook->hook_name == NULL) {
		log_err(errno, argv[4], "strdup failed!");
		exit(2);
	}
	phook->type = HOOK_TYPE_DEFAULT;
	phook->user = HOOK_PBSADMIN;
	phook->event = HOOK_EVENT_PROVISION;
	phook->script = NULL;
	/* get script */
	snprintf(output_path, MAXPATHLEN,
		"%s/server_priv/hooks/%s%s", argv[6], phook->hook_name, ".PY");

	if (pbs_python_ext_alloc_python_script(output_path,
		(struct python_script **)&phook->script) == -1) {
		log_err(errno, argv[3], "provision hook script allocation failed!");
		exit(2);
	}


	if ((rc = pbs_python_check_and_compile_script(&svr_interp_data,
		phook->script)) != 0) {
		DBPRT(("%s: Recompilation failed\n", __func__))
		log_event(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER, LOG_INFO,
			argv[3], "Provisioning script recompilation failed");
		exit(2);
	}

	(void)memset(&prov_info, 0, sizeof(prov_info));
	prov_info.pvnfo_vnode = malloc(strlen(argv[1]+1));
	if (prov_info.pvnfo_vnode == NULL) {
		log_err(ENOMEM, __func__, "out of memory");
		exit(2);
	}
	prov_info.pvnfo_aoe_req = malloc(strlen(argv[2]+1));
	if (prov_info.pvnfo_aoe_req == NULL) {
		log_err(ENOMEM, __func__, "out of memory");
		free(prov_info.pvnfo_vnode);
		exit(2);
	}
	strcpy(prov_info.pvnfo_vnode, argv[1]);
	strcpy(prov_info.pvnfo_aoe_req, argv[2]);

	rc = execute_python_prov_script(phook, &prov_info);
	pbs_python_ext_shutdown_interpreter(&svr_interp_data);

	free(prov_info.pvnfo_vnode);
	free(prov_info.pvnfo_aoe_req);
	exit(rc);
}
