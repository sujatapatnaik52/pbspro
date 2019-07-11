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
 * @file    get_4byte.c
 *
 * @brief
 * 		get_4byte.c - contains functions related to get_sched_cmd sent by the Server.
 *
 * Functions included are:
 *	get_sched_cmd()
 *	get_sched_cmd_noblk()
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include <stdlib.h>
#include "dis.h"
#include "sched_cmds.h"

#ifndef WIN64
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(FD_SET_IN_SYS_SELECT_H)
#include <sys/select.h>
#endif

/**
 *
 * @brief
 * 		Gets the Scheduler Command sent by the Server
 *
 * @param[in]	sock	-	socket endpoint to the server
 * @param[in]	val	-	pointer to the value of the scheduler command sent.
 * @param[in]	jid	-	if 'val' obtained is SCH_SCHEDULE_AJOB, then '*jid'
 *						holds the jobid of the job to be scheduled.
 * @return	int
 * @retval	0	: for EOF,
 * @retval	+1	: for success
 * @retval	-1	: for error.
 *
 * @note
 *		Callers need to free the value pointed to by 'jid' after returning
 *		from this call.
 */

int
get_sched_cmd(int sock, int *val, char **jid)
{
	int	i;
	int     rc = 0;
	char	*jobid = NULL;

	DIS_tcp_setup(sock);

	i = disrsi(sock, &rc);
	if (rc != 0)
		goto err;
	if (i == SCH_SCHEDULE_AJOB) {
		jobid = disrst(sock, &rc);
		if (rc != 0)
			goto err;
		*jid = jobid;
	} else {
		*jid = NULL;
	}
	*val = i;
	return 1;

err:
	if (rc == DIS_EOF)
		return 0;
	else
		return -1;
}

/**
 *
 * @brief
 * 		This is non-blocking version of get_sched_cmd().
 *
 * @param[in]	sock	-	communication endpoint to the server`
 * @param[in]	val	-	points to the value of the command sent by server.
 * @param[in]   jid	-	if *val is SCH_SCHEDULE_AJOB, then return jobid in
 *						*jid.
 * @return	int
 * @retval	0	for EOF,
 * @retval	+1	for success
 * @retval	-1	for error.
 *
 * @note
 *		Callers need to free the value pointed to by 'jid' after returning
 *		from this call.
 */

int
get_sched_cmd_noblk(int sock, int *val, char **jid)
{
	struct timeval timeout;
	fd_set		fdset;
	timeout.tv_usec = 0;
	timeout.tv_sec  = 0;

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	if ((select(FD_SETSIZE, &fdset, NULL, NULL,
		&timeout) != -1)  && (FD_ISSET(sock, &fdset))) {
		return (get_sched_cmd(sock, val, jid));
	}
	return (0);
}
