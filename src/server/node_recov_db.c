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
 * @file    node_recov_db.c
 *
 * @brief
 *		node_recov_db.c - This file contains the functions to record a node
 *		data structure to database and to recover it from database.
 *
 * Included functions are:
 *	node_recov_db()
 *	node_save_db()
 *	db_to_svr_node()
 *	svr_to_db_node()
 *	node_recov_db_raw()
 *	node_delete_db()
 */


#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>

#ifndef WIN32
#include <sys/param.h>
#endif

#include "pbs_ifl.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"

#ifdef WIN32
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include "win.h"
#endif

#include "log.h"
#include "attribute.h"
#include "list_link.h"
#include "server_limits.h"
#include "credential.h"
#include "libpbs.h"
#include "batch_request.h"
#include "pbs_nodes.h"
#include "job.h"
#include "resource.h"
#include "reservation.h"
#include "queue.h"
#include "svrfunc.h"
#include <memory.h>
#include "libutil.h"
#include "pbs_db.h"

#ifdef NAS /* localmod 005 */
/* External Functions Called */
extern int recov_attr_db(pbs_db_conn_t *conn,
	void *parent,
	pbs_db_attr_info_t *p_attr_info,
	struct attribute_def *padef,
	struct attribute *pattr,
	int limit,
	int unknown);
extern int recov_attr_db_raw(pbs_db_conn_t *conn,
	pbs_db_attr_info_t *p_attr_info,
	pbs_list_head *phead);
#endif /* localmod 005 */


extern int make_pbs_list_attr_db(void *parent, pbs_db_attr_list_t *attr_list, struct attribute_def *padef, pbs_list_head *phead, int limit, int unknown);
/**
 * @brief
 *		Load a server node object from a database node object
 *
 * @param[out]	pnode - Address of the node in the server
 * @param[in]	pdbnd - Address of the database node object
 *
 * @return	Error code
 * @retval   0 - Success
 * @retval  -1 - Failure
 *
 */
static int
db_to_svr_node(struct pbsnode *pnode, pbs_db_node_info_t *pdbnd)
{
	if (pdbnd->nd_name && pdbnd->nd_name[0]!=0) {
		pnode->nd_name = strdup(pdbnd->nd_name);
		if (pnode->nd_name == NULL)
			return -1;
	}
	else
		pnode->nd_name = NULL;

	if (pdbnd->nd_hostname && pdbnd->nd_hostname[0]!=0) {
		pnode->nd_hostname = strdup(pdbnd->nd_hostname);
		if (pnode->nd_hostname == NULL)
			return -1;
	}
	else
		pnode->nd_hostname = NULL;

	pnode->nd_ntype = pdbnd->nd_ntype;
	pnode->nd_state = pdbnd->nd_state;
	if (pnode->nd_pque)
		strcpy(pnode->nd_pque->qu_qs.qu_name, pdbnd->nd_pque);


	if ((decode_attr_db(pnode, &pdbnd->attr_list, node_attr_def,
		pnode->nd_attr, (int) ND_ATR_LAST, 0)) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *		Load a database node object from a server node object
 *
 * @param[in]	pnode - Address of the node in the server
 * @param[out]	pdbnd - Address of the database node object
 *
 * @return 0    Success
 * @return !=0  Failure
 */
static int
svr_to_db_node(struct pbsnode *pnode, pbs_db_node_info_t *pdbnd)
{
	int j, wrote_np = 0;
	svrattrl *psvrl;
	pbs_list_head wrtattr;
	pbs_db_attr_info_t *attrs = NULL;
	int numattr = 0;
	int count = 0;
	int vnode_sharing = 0;

	if (pnode->nd_name)
		strcpy(pdbnd->nd_name, pnode->nd_name);
	else
		pdbnd->nd_name[0]=0;

	/* node_index is used to sort vnodes upon recovery.
	 * For Cray multi-MoM'd vnodes, we ensure that natural vnodes come
	 * before the vnodes that it manages by introducing offsetting all
	 * non-natural vnodes indices to come after natural vnodes.
	 */
	pdbnd->nd_index = (pnode->nd_nummoms * svr_totnodes) + pnode->nd_index;

	if (pnode->nd_hostname)
		strcpy(pdbnd->nd_hostname, pnode->nd_hostname);
	else
		pdbnd->nd_hostname[0]=0;

	if (pnode->nd_moms && pnode->nd_moms[0])
		pdbnd->mom_modtime = pnode->nd_moms[0]->mi_modtime;
	else
		pdbnd->mom_modtime = 0;

	pdbnd->nd_ntype = pnode->nd_ntype;
	pdbnd->nd_state = pnode->nd_state;
	if (pnode->nd_pque)
		strcpy(pdbnd->nd_pque, pnode->nd_pque->qu_qs.qu_name);
	else
		pdbnd->nd_pque[0]=0;

	/*
	 * node attributes are saved in a different way than attributes of other objects
	 * for other objects we directly call save_attr_db, but for node attributes
	 * we massage some of the attributes. The special ones are pcpus
	 * and resv_enable and sharing.
	 */
	CLEAR_HEAD(wrtattr);

	for (j = 0; j < ND_ATR_LAST; ++j) {
		/* skip certain ones: no-save and default values */
		if ((node_attr_def[j].at_flags & ATR_DFLAG_NOSAVM) ||
				(pnode->nd_attr[j].at_flags & ATR_VFLAG_DEFLT))
			continue;

		(void) node_attr_def[j].at_encode(&pnode->nd_attr[j],
		                                  &wrtattr,
		                                  node_attr_def[j].at_name,
		                                  NULL,
		                                  ATR_ENCODE_SVR,
		                                  NULL);

		node_attr_def[j].at_flags &= ~ATR_VFLAG_MODIFY;
	}

	vnode_sharing = (((pnode->nd_attr[ND_ATR_Sharing].at_flags & (ATR_VFLAG_SET | ATR_VFLAG_DEFLT))
				== (ATR_VFLAG_SET | ATR_VFLAG_DEFLT))
				&& ((pnode->nd_attr[ND_ATR_Sharing].at_val.at_long != VNS_UNSET)
						&& (pnode->nd_attr[ND_ATR_Sharing].at_val.at_long != VNS_DFLT_SHARED)));
	numattr = vnode_sharing;
	psvrl = (svrattrl *)GET_NEXT(wrtattr);
	while (psvrl) {
		if ((strcmp(psvrl->al_name, ATTR_rescavail) == 0) &&
				(strcmp(psvrl->al_resc, "ncpus") == 0)) {
			wrote_np = 1;
		}
		psvrl = (svrattrl *)GET_NEXT(psvrl->al_link);
		numattr++;
	}
	if (wrote_np == 0) {
		numattr++;
	}
	pdbnd->attr_list.attributes = calloc(sizeof(pbs_db_attr_info_t), numattr);
	if (!pdbnd->attr_list.attributes)
			return -1;
	pdbnd->attr_list.attr_count = 0;
	attrs = pdbnd->attr_list.attributes;

	while ((psvrl = (svrattrl *) GET_NEXT(wrtattr)) != NULL) {

		if (strcmp(psvrl->al_name, ATTR_NODE_pcpus) == 0) {
			/* don't write out pcpus at this point, see */
			/* check for pcpus if needed after loop end */
			delete_link(&psvrl->al_link);
			(void) free(psvrl);

			continue;
		} else if (strcmp(psvrl->al_name, ATTR_NODE_resv_enable) == 0) {
			/*  write resv_enable only if not default value */
			if ((psvrl->al_flags & ATR_VFLAG_DEFLT) != 0) {
				delete_link(&psvrl->al_link);
				(void) free(psvrl);

				continue;
			}
		}

		/* every attribute to this point we write to database */
		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, psvrl->al_name, sizeof(attrs[count].attr_name));
		if (psvrl->al_resc) {
			attrs[count].attr_resc[sizeof(attrs[count].attr_resc) - 1] = '\0';
			strncpy(attrs[count].attr_resc, psvrl->al_resc, sizeof(attrs[count].attr_resc));
		}
		else
			strcpy(attrs[count].attr_resc, "");
		attrs[count].attr_value = strdup(psvrl->al_value);
		attrs[count].attr_flags = psvrl->al_flags;
		count++;

        delete_link(&psvrl->al_link);
        (void)free(psvrl);
	}

	/*
	 * Attributes with default values are not general saved to disk.
	 * However to deal with some special cases, things needed for
	 * attaching jobs to the vnodes on recover that we don't have
	 * except after we hear from Mom, i.e. we :
	 * 1. Need number of cpus, if it isn't writen as a non-default, as
	 *    "np", then write "pcpus" which will be treated as a default
	 * 2. Need the "sharing" attribute written even if default
	 *    and not the default value (i.e. it came from Mom).
	 *    so save it as the "special" [sharing] when it is a default
	 */
	if (wrote_np == 0) {
		char pcpu_str[10];
		/* write the default value for the num of cpus */
		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, ATTR_NODE_pcpus, sizeof(attrs[count].attr_name));
		strcpy(attrs[count].attr_resc, "");
		sprintf(pcpu_str, "%ld", pnode->nd_nsn);
		attrs[count].attr_value = strdup(pcpu_str);
		count++;
	}

	if (vnode_sharing) {
		char *vn_str;
		vn_str = vnode_sharing_to_str((enum vnode_sharing) pnode->nd_attr[ND_ATR_Sharing].at_val.at_long);

		attrs[count].attr_name[sizeof(attrs[count].attr_name) - 1] = '\0';
		strncpy(attrs[count].attr_name, ATTR_NODE_Sharing, sizeof(attrs[count].attr_name));
		strcpy(attrs[count].attr_resc, "");
		attrs[count].attr_value = strdup(vn_str);
		count++;
	}
	pdbnd->attr_list.attr_count = count;

	pnode->nd_modified &= ~NODE_UPDATE_OTHERS;

	return 0;
}

/**
 * @brief
 *		Recover a node from the database
 *
 * @param[in]	nd	- Information about the node to recover
 *
 * @return	The recovered node structure
 * @retval	NULL - Failure
 * @retval	!NULL - Success - address of recovered node returned
 */
struct pbsnode *
node_recov_db(void *nd)
{
	pbs_db_obj_info_t obj;
	struct pbsnode *np;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;
	pbs_db_node_info_t *dbnode =(pbs_db_node_info_t *) nd;

	np = malloc(sizeof(struct pbsnode));
	if (np == NULL) {
		log_err(errno, "node_recov", "error on recovering node attr");
		return NULL;
	}
	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = dbnode;

	if (pbs_db_begin_trx(conn, 0, 0) !=0)
		goto db_err;

	if (pbs_db_load_obj(conn, &obj) != 0)
		goto db_err;

	initialize_pbsnode(np, NULL, NTYPE_PBS);
	if (db_to_svr_node(np, dbnode) != 0)
		goto db_err;

	if (pbs_db_end_trx(conn, PBS_DB_COMMIT) != 0)
		goto db_err;

	pbs_db_reset_obj(&obj);

	return np;

db_err:
	free(np);
	log_err(-1, "node_recov", "error on recovering node attr");
	(void) pbs_db_end_trx(conn, PBS_DB_ROLLBACK);
	return NULL;
}


/**
 * @brief
 *	Recover a node from the database without calling the action routines
 *	for the node attributes. This is because, the node attribute action
 *	routines access other resources in the node attributes which may
 *	not have been loaded yet. create_pbs_node (the top level caller)
 *	eventually calls mgr_set_attr to atomically set all the attributes
 *	and in that process triggers all the action routines.
 *
 * @param[in]	nd - Information about the node to recover
 * @param[in]	phead - list head to which to append loaded node attributes
 *
 * @return      Error code
 * @retval	-1 - Failure
 * @retval	 0 - Success
 *
 */
int
node_recov_db_raw(void *nd, pbs_list_head *phead)
{
	pbs_db_node_info_t *dbnode =(pbs_db_node_info_t *) nd;

	/* now convert attributes array to pbs list structure */
	if ((make_pbs_list_attr_db(nd, &dbnode->attr_list, node_attr_def,
		phead, (int) ND_ATR_LAST, 0)) != 0)
		return -1;

	return 0;
}

/**
 * @brief
 *	Save a node to the database. When we save a node to the database, delete
 *	the old node information and write the node afresh. This ensures that
 *	any deleted attributes of the node are removed, and only the new ones are
 *	updated to the database.
 *
 * @param[in]	pnode - Pointer to the node to save
 * @param[in]	mode:
 *		NODE_SAVE_FULL - Full update along with attributes
 *		NODE_SAVE_QUICK - Quick update without attributes
 *		NODE_SAVE_NEW	- New node insert into database
 *		NODE_SAVE_QUICK_STATE - Quick update, along with state attrib
 *
 * @return      Error code
 * @retval	0 - Success
 * @retval	-1 - Failure
 *
 */
int
node_save_db(struct pbsnode *pnode)
{
	pbs_db_node_info_t dbnode;
	pbs_db_obj_info_t obj;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;

	svr_to_db_node(pnode, &dbnode);
	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = &dbnode;

	if (pbs_db_save_obj(conn, &obj, PBS_UPDATE_DB_FULL) != 0) {
		if (pbs_db_save_obj(conn, &obj, PBS_INSERT_DB) != 0) {
			goto db_err;
		}
	}

	pbs_db_reset_obj(&obj);

	return (0);
db_err:
	strcpy(log_buffer, "node_save failed ");
	if (conn->conn_db_err != NULL)
		strncat(log_buffer, conn->conn_db_err, LOG_BUF_SIZE - strlen(log_buffer) - 1);
	log_err(-1, "node_save_db", log_buffer);
	panic_stop_db(log_buffer);
	return (-1);
}



/**
 * @brief
 *	Delete a node from the database
 *
 * @param[in]	pnode - Pointer to the node to delete
 *
 * @return      Error code
 * @retval	0 - Success
 * @retval	-1 - Failure
 *
 */
int
node_delete_db(struct pbsnode *pnode)
{
	pbs_db_node_info_t dbnode;
	pbs_db_obj_info_t obj;
	pbs_db_conn_t *conn = (pbs_db_conn_t *) svr_db_conn;

	dbnode.nd_name[sizeof(dbnode.nd_name) - 1] = '\0';
	strncpy(dbnode.nd_name, pnode->nd_name, sizeof(dbnode.nd_name));
	obj.pbs_db_obj_type = PBS_DB_NODE;
	obj.pbs_db_un.pbs_db_node = &dbnode;

	if (pbs_db_delete_obj(conn, &obj) == -1)
		return (-1);
	else
		return (0);	/* "success" or "success but rows deleted" */
}
