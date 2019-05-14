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
 * dis_read.c - contains function to read and decode the DIS
 *	encoded requests and replies.
 *
 *	Included public functions are:
 *
 *	decode_DIS_CopyFiles
 *	decode_DIS_CopyFiles_Cred
 *	decode_DIS_MomRestart
 *	decode_DIS_replySvr_inner
 *	decode_DIS_replySvr
 *	decode_DIS_replySvrRPP
 *	dis_request_read
 *	DIS_reply_read
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "dis.h"
#include "libpbs.h"
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "log.h"
#include "pbs_error.h"
#include "credential.h"
#include "batch_request.h"
#include "net_connect.h"


/* External Global Data */

extern char	*msg_nosupport;


/*	Data items are:	string		job id			(may be null)
 *			string		job owner		(may be null)
 *			string		execution user name
 *			string		execution group name	(may be null)
 *			unsigned int	direction & sandbox flag
 *			unsigned int	count of file pairs in set
 *			set of		file pairs:
 *				unsigned int	flag
 *				string		local path name
 *				string		remote path name (may be null)
 */
/**
 * @brief
 * 		decode_DIS_CopyFiles() - decode a Copy Files Dependency Batch Request
 *
 *		This request is used by the server ONLY.
 *		The batch request structure pointed to by preq must already exist.
 *
 * @see
 * dis_request_read
 *
 * @param[in] sock - socket of connection from Mom
 * @param[in] preq - pointer to the batch request structure to be filled in
 *
 * @return int
 * @retval 0 - success
 * @retval non-zero - decode failure error from a DIS routine
 */
int
decode_DIS_CopyFiles(int sock, struct batch_request *preq)
{
	int   pair_ct;
	struct rq_cpyfile *pcf;
	struct rqfpair    *ppair;
	int   rc;

	pcf = &preq->rq_ind.rq_cpyfile;
	CLEAR_HEAD(pcf->rq_pair);
	if ((rc = disrfst(sock, PBS_MAXSVRJOBID, pcf->rq_jobid)) != 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXUSER, pcf->rq_owner)) != 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXUSER, pcf->rq_user))  != 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXGRPN, pcf->rq_group)) != 0)
		return rc;
	pcf->rq_dir = disrui(sock, &rc);
	if (rc) return rc;

	pair_ct = disrui(sock, &rc);
	if (rc) return rc;

	while (pair_ct--) {

		ppair = (struct rqfpair *)malloc(sizeof(struct rqfpair));
		if (ppair == NULL)
			return DIS_NOMALLOC;
		CLEAR_LINK(ppair->fp_link);
		ppair->fp_local = 0;
		ppair->fp_rmt   = 0;

		ppair->fp_flag = disrui(sock, &rc);
		if (rc) {
			(void)free(ppair);
			return rc;
		}
		ppair->fp_local = disrst(sock, &rc);
		if (rc) {
			(void)free(ppair);
			return rc;
		}
		ppair->fp_rmt = disrst(sock, &rc);
		if (rc) {
			(void)free(ppair->fp_local);
			(void)free(ppair);
			return rc;
		}
		append_link(&pcf->rq_pair, &ppair->fp_link, ppair);
	}
	return 0;
}
 /*	Data items are:	string		job id			(may be null)
 *			string		job owner		(may be null)
 *			string		execution user name
 *			string		execution group name	(may be null)
 *			unsigned int	direction & sandbox flag
 *			unsigned int	count of file pairs in set
 *			set of		file pairs:
 *				unsigned int	flag
 *				string		local path name
 *				string		remote path name (may be null)
 *			unsigned int	credential length (bytes)
 *			byte string	credential
 */
/**
 * @brief
 * 		decode_DIS_CopyFiles_Cred() - decode a Copy Files with Credential
 *				 Dependency Batch Request
 *
 *		This request is used by the server ONLY.
 *		The batch request structure pointed to by preq must already exist.
 *
 * @see
 * 		dis_request_read
 *
 * @param[in] sock - socket of connection from Mom
 * @param[in] preq - pointer to the batch request structure to be filled in
 *
 * @return int
 * @retval 0 - success
 * @retval non-zero - decode failure error from a DIS routine
 */
int
decode_DIS_CopyFiles_Cred(int sock, struct batch_request *preq)
{
	int			pair_ct;
	struct rq_cpyfile_cred	*pcfc;
	struct rqfpair		*ppair;
	int			rc;

	pcfc = &preq->rq_ind.rq_cpyfile_cred;
	CLEAR_HEAD(pcfc->rq_copyfile.rq_pair);
	if ((rc = disrfst(sock, PBS_MAXSVRJOBID, pcfc->rq_copyfile.rq_jobid))
		!= 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXUSER, pcfc->rq_copyfile.rq_owner)) != 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXUSER, pcfc->rq_copyfile.rq_user))  != 0)
		return rc;
	if ((rc = disrfst(sock, PBS_MAXGRPN, pcfc->rq_copyfile.rq_group)) != 0)
		return rc;
	pcfc->rq_copyfile.rq_dir = disrui(sock, &rc);
	if (rc) return rc;

	pair_ct = disrui(sock, &rc);
	if (rc) return rc;

	while (pair_ct--) {

		ppair = (struct rqfpair *)malloc(sizeof(struct rqfpair));
		if (ppair == NULL)
			return DIS_NOMALLOC;
		CLEAR_LINK(ppair->fp_link);
		ppair->fp_local = 0;
		ppair->fp_rmt   = 0;

		ppair->fp_flag = disrui(sock, &rc);
		if (rc) {
			(void)free(ppair);
			return rc;
		}
		ppair->fp_local = disrst(sock, &rc);
		if (rc) {
			(void)free(ppair);
			return rc;
		}
		ppair->fp_rmt = disrst(sock, &rc);
		if (rc) {
			(void)free(ppair->fp_local);
			(void)free(ppair);
			return rc;
		}
		append_link(&pcfc->rq_copyfile.rq_pair,
			&ppair->fp_link, ppair);
	}

	pcfc->rq_credtype = disrui(sock, &rc);
	if (rc) return rc;
	pcfc->rq_pcred = disrcs(sock, &pcfc->rq_credlen, &rc);
	if (rc) return rc;

	return 0;
}

/**
 * @brief
 * 		Decode a Mom Restart request from a Mom.
 *		This request is sent from a Mom when she first starts up.
 *		Mom opens a connection to the Server, sends this request and waits for
 *		a reply.
 *		The expected data to be read and decoded into the structure are:
 *		the host name of the Mom and the port on which she listens for TCP.
 *
 * @see
 * 		dis_request_read
 *
 * @param[in] sock - socket of connection from Mom
 * @param[in] preq - pointer to the batch request structure to be filled in
 *
 * @return int
 * @retval 0 - success
 * @retval non-zero - decode failure error from a DIS routine
 */
int
decode_DIS_MomRestart(int sock, struct batch_request *preq)
{
	int rc;
	struct rq_momrestart *pmr;

	pmr = &preq->rq_ind.rq_momrestart;
	if ((rc = disrfst(sock, PBS_MAXHOSTNAME, pmr->rq_momhost)) != 0)
		return rc;
	pmr->rq_port = disrui(sock, &rc);
	if (rc)
		return rc;
	return 0;
}


/**
 * @brief
 * 		decode_DIS_replySvr_inner() - decode a Batch Protocol Reply Structure for Server
 *
 *		This routine decodes a batch reply into the form used by server.
 *		The only difference between this and the command version is on status
 *		replies.  For the server,  the attributes are decoded into a list of
 *		server svrattrl structures rather than a commands's attrl.
 *
 * @see
 * 		decode_DIS_replySvrRPP
 *
 * @param[in] sock - socket connection from which to read reply
 * @param[in,out] reply - batch_reply structure defined in libpbs.h, it must be allocated
 *					  by the caller.
 *
 * @return int
 * @retval 0 - success
 * @retval -1 - if brp_choice is wrong
 * @retval non-zero - decode failure error from a DIS routine
 */

static int
decode_DIS_replySvr_inner(int sock, struct batch_reply *reply)
{
	int		      ct;
	struct brp_select    *psel;
	struct brp_select   **pselx;
	struct brp_status    *pstsvr;
	int		      rc = 0;
	size_t		      txtlen;

	/* next decode code, auxcode and choice (union type identifier) */

	reply->brp_code    = disrsi(sock, &rc);
	if (rc) return rc;
	reply->brp_auxcode = disrsi(sock, &rc);
	if (rc) return rc;
	reply->brp_choice  = disrui(sock, &rc);
	if (rc) return rc;


	switch (reply->brp_choice) {

		case BATCH_REPLY_CHOICE_NULL:
			break;	/* no more to do */

		case BATCH_REPLY_CHOICE_Queue:
		case BATCH_REPLY_CHOICE_RdytoCom:
		case BATCH_REPLY_CHOICE_Commit:
			rc = disrfst(sock, PBS_MAXSVRJOBID+1, reply->brp_un.brp_jid);
			if (rc)
				return (rc);
			break;

		case BATCH_REPLY_CHOICE_Select:

			/* have to get count of number of strings first */

			reply->brp_un.brp_select = NULL;
			pselx = &reply->brp_un.brp_select;
			ct = disrui(sock, &rc);
			if (rc) return rc;

			while (ct--) {
				psel = (struct brp_select *)malloc(sizeof(struct brp_select));
				if (psel == 0) return DIS_NOMALLOC;
				psel->brp_next = NULL;
				psel->brp_jobid[0] = '\0';
				rc = disrfst(sock, PBS_MAXSVRJOBID+1, psel->brp_jobid);
				if (rc) {
					(void)free(psel);
					return rc;
				}
				*pselx = psel;
				pselx = &psel->brp_next;
			}
			break;

		case BATCH_REPLY_CHOICE_Status:

			/* have to get count of number of status objects first */

			CLEAR_HEAD(reply->brp_un.brp_status);
			ct = disrui(sock, &rc);
			if (rc) return rc;

			while (ct--) {
				pstsvr = (struct brp_status *)malloc(sizeof(struct brp_status));
				if (pstsvr == 0) return DIS_NOMALLOC;

				CLEAR_LINK(pstsvr->brp_stlink);
				pstsvr->brp_objname[0] = '\0';
				CLEAR_HEAD(pstsvr->brp_attr);

				pstsvr->brp_objtype = disrui(sock, &rc);
				if (rc == 0) {
					rc = disrfst(sock, PBS_MAXSVRJOBID+1,
						pstsvr->brp_objname);
				}
				if (rc) {
					(void)free(pstsvr);
					return rc;
				}
				append_link(&reply->brp_un.brp_status,
					&pstsvr->brp_stlink, pstsvr);
				rc = decode_DIS_svrattrl(sock, &pstsvr->brp_attr);
			}
			break;

		case BATCH_REPLY_CHOICE_Text:

			/* text reply */

		  	reply->brp_un.brp_txt.brp_str = disrcs(sock, &txtlen, &rc);
			reply->brp_un.brp_txt.brp_txtlen = txtlen;
			break;

		case BATCH_REPLY_CHOICE_Locate:

			/* Locate Job Reply */

			rc = disrfst(sock, PBS_MAXDEST+1, reply->brp_un.brp_locate);
			break;

		default:
			return -1;
	}

	return rc;
}

/**
 * @brief
 * 		decode a Batch Protocol Reply Structure for Server
 *
 *  	This routine reads reply over TCP by calling decode_DIS_replySvr_inner()
 * 		to read the reply to a batch request. This routine reads the protocol type
 * 		and version before calling decode_DIS_replySvr_inner() to read the rest of
 * 		the reply structure.
 *
 * @see
 *		DIS_reply_read
 *
 * @param[in] sock - socket connection from which to read reply
 * @param[out] reply - The reply structure to be returned
 *
 * @return Error code
 * @retval DIS_SUCCESS(0) - Success
 * @retval !DIS_SUCCESS   - Failure (see dis.h)
 */
int
decode_DIS_replySvr(int sock, struct batch_reply *reply)
{
	int		      rc = 0;
	int		      i;
	/* first decode "header" consisting of protocol type and version */

	i = disrui(sock, &rc);
	if (rc != 0) return rc;
	if (i != PBS_BATCH_PROT_TYPE) return DIS_PROTO;
	i = disrui(sock, &rc);
	if (rc != 0) return rc;
	if (i != PBS_BATCH_PROT_VER) return DIS_PROTO;

	return (decode_DIS_replySvr_inner(sock, reply));
}

/**
 * @brief
 * 		decode a Batch Protocol Reply Structure for Server over RPP
 *
 *  	This routine reads data over RPP by calling decode_DIS_replySvr_inner()
 * 		to read the reply to a batch request. This routine reads the protocol type
 * 		and version before calling decode_DIS_replySvr_inner() to read the rest of
 * 		the reply structure.
 *
 * @see
 * 		DIS_reply_read
 *
 * @param[in] sock - socket connection from which to read reply
 * @param[out] reply - The reply structure to be returned
 *
 * @return Error code
 * @retval DIS_SUCCESS(0) - Success
 * @retval !DIS_SUCCESS   - Failure (see dis.h)
 */
int
decode_DIS_replySvrRPP(int sock, struct batch_reply *reply)
{
	/* for rpp header has already been read */
	return (decode_DIS_replySvr_inner(sock, reply));
}

/**
 * @brief
 * 		Read in an DIS encoded request from the network
 * 		and decodes it:
 *		Read and decode the request into the request structures
 *
 * @see
 * 		process_request and read_fo_request
 *
 * @param[in]		sfds	- the socket descriptor
 * @param[in,out]	request - will contain the decoded request
 *
 * @return int
 * @retval 0 	if request read ok, batch_request pointed to by request is
 *		   updated.
 * @retval -1 	if EOF (no request but no error)
 * @retval >0 	if errors ( a PBSE_ number)
 */

int
dis_request_read(int sfds, struct batch_request *request)
{
#ifndef	PBS_MOM
	int	 i;
#endif	/* PBS_MOM */
	int	 proto_type;
	int	 proto_ver;
	int	 rc; 	/* return code */

	if (!request->isrpp)
		DIS_tcp_setup(sfds);	/* setup for DIS over tcp */

	/* Decode the Request Header, that will tell the request type */

	rc = decode_DIS_ReqHdr(sfds, request, &proto_type, &proto_ver);

	if (rc != 0) {
		if (rc == DIS_EOF)
			return EOF;
		(void)sprintf(log_buffer,
			"Req Header bad, errno %d, dis error %d",
			errno, rc);
		log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_REQUEST, LOG_DEBUG,
			"?", log_buffer);

		return PBSE_DISPROTO;
	}

	if (proto_ver > PBS_BATCH_PROT_VER)
		return PBSE_DISPROTO;

	/* Decode the Request Body based on the type */

	switch (request->rq_type) {
		case PBS_BATCH_Connect:
			break;

		case PBS_BATCH_Disconnect:
			return (-1);		/* set EOF return */

		case PBS_BATCH_QueueJob:
		case PBS_BATCH_SubmitResv:
			CLEAR_HEAD(request->rq_ind.rq_queuejob.rq_attr);
			rc = decode_DIS_QueueJob(sfds, request);
			break;

		case PBS_BATCH_JobCred:
			rc = decode_DIS_JobCred(sfds, request);
			break;

		case PBS_BATCH_UserCred:
			rc = decode_DIS_UserCred(sfds, request);
			break;

		case PBS_BATCH_UserMigrate:
			rc = decode_DIS_UserMigrate(sfds, request);
			break;

		case PBS_BATCH_jobscript:
		case PBS_BATCH_MvJobFile:
			rc = decode_DIS_JobFile(sfds, request);
			break;

		case PBS_BATCH_RdytoCommit:
		case PBS_BATCH_Commit:
		case PBS_BATCH_Rerun:
			rc = decode_DIS_JobId(sfds, request->rq_ind.rq_commit);
			break;

		case PBS_BATCH_DeleteJob:
		case PBS_BATCH_DeleteResv:
		case PBS_BATCH_ResvOccurEnd:
		case PBS_BATCH_HoldJob:
		case PBS_BATCH_ModifyJob:
			rc = decode_DIS_Manage(sfds, request);
			break;

		case PBS_BATCH_MessJob:
			rc = decode_DIS_MessageJob(sfds, request);
			break;

		case PBS_BATCH_Shutdown:
		case PBS_BATCH_FailOver:
			rc = decode_DIS_ShutDown(sfds, request);
			break;

		case PBS_BATCH_SignalJob:
			rc = decode_DIS_SignalJob(sfds, request);
			break;

		case PBS_BATCH_StatusJob:
			rc = decode_DIS_Status(sfds, request);
			break;

		case PBS_BATCH_PySpawn:
			rc = decode_DIS_PySpawn(sfds, request);
			break;

		case PBS_BATCH_AuthExternal:
			rc = decode_DIS_AuthExternal(sfds, request);
			break;

#ifndef PBS_MOM
		case PBS_BATCH_RelnodesJob:
			rc = decode_DIS_RelnodesJob(sfds, request);
			break;

		case PBS_BATCH_LocateJob:
			rc = decode_DIS_JobId(sfds, request->rq_ind.rq_locate);
			break;

		case PBS_BATCH_Manager:
		case PBS_BATCH_ReleaseJob:
			rc = decode_DIS_Manage(sfds, request);
			break;

		case PBS_BATCH_MoveJob:
		case PBS_BATCH_OrderJob:
			rc = decode_DIS_MoveJob(sfds, request);
			break;

		case PBS_BATCH_RunJob:
		case PBS_BATCH_AsyrunJob:
		case PBS_BATCH_StageIn:
		case PBS_BATCH_ConfirmResv:
			rc = decode_DIS_Run(sfds, request);
			break;

		case PBS_BATCH_DefSchReply:
			request->rq_ind.rq_defrpy.rq_cmd = disrsi(sfds, &rc);
			if (rc) break;
			request->rq_ind.rq_defrpy.rq_id = disrst(sfds, &rc);
			if (rc) break;
			request->rq_ind.rq_defrpy.rq_err = disrsi(sfds, &rc);
			if (rc) break;
			i = disrsi(sfds, &rc);
			if (rc) break;
			if (i)
				request->rq_ind.rq_defrpy.rq_txt = disrst(sfds, &rc);
			break;

		case PBS_BATCH_SelectJobs:
		case PBS_BATCH_SelStat:
			CLEAR_HEAD(request->rq_ind.rq_select.rq_selattr);
			CLEAR_HEAD(request->rq_ind.rq_select.rq_rtnattr);
			rc = decode_DIS_svrattrl(sfds,
				&request->rq_ind.rq_select.rq_selattr);
			rc = decode_DIS_svrattrl(sfds,
				&request->rq_ind.rq_select.rq_rtnattr);
			break;

		case PBS_BATCH_StatusNode:
		case PBS_BATCH_StatusResv:
		case PBS_BATCH_StatusQue:
		case PBS_BATCH_StatusSvr:
		case PBS_BATCH_StatusSched:
		case PBS_BATCH_StatusRsc:
		case PBS_BATCH_StatusHook:
			rc = decode_DIS_Status(sfds, request);
			break;

		case PBS_BATCH_TrackJob:
			rc = decode_DIS_TrackJob(sfds, request);
			break;

		case PBS_BATCH_Rescq:
		case PBS_BATCH_ReserveResc:
		case PBS_BATCH_ReleaseResc:
			rc = decode_DIS_Rescl(sfds, request);
			break;

		case PBS_BATCH_RegistDep:
			rc = decode_DIS_Register(sfds, request);
			break;

		case PBS_BATCH_AuthenResvPort:
			rc = decode_DIS_AuthenResvPort(sfds, request);
			break;

		case PBS_BATCH_MomRestart:
			rc = decode_DIS_MomRestart(sfds, request);
			break;

		case PBS_BATCH_ModifyResv:
			decode_DIS_ModifyResv(sfds, request);
			break;

#else	/* yes PBS_MOM */

		case PBS_BATCH_CopyHookFile:
			rc = decode_DIS_CopyHookFile(sfds, request);
			break;

		case PBS_BATCH_DelHookFile:
			rc = decode_DIS_DelHookFile(sfds, request);
			break;

		case PBS_BATCH_CopyFiles:
		case PBS_BATCH_DelFiles:
			rc = decode_DIS_CopyFiles(sfds, request);
			break;

		case PBS_BATCH_CopyFiles_Cred:
		case PBS_BATCH_DelFiles_Cred:
			rc = decode_DIS_CopyFiles_Cred(sfds, request);
			break;

#endif	/* PBS_MOM */

		default:
			sprintf(log_buffer, "%s: %d from %s", msg_nosupport,
				request->rq_type, request->rq_user);
			log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_REQUEST, LOG_DEBUG,
				"?", log_buffer);
			rc = PBSE_UNKREQ;
			break;
	}

	if (rc == 0) {	/* Decode the Request Extension, if present */
		rc = decode_DIS_ReqExtend(sfds, request);
		if (rc != 0) {
			(void)sprintf(log_buffer,
				"Request type: %d Req Extension bad, dis error %d", request->rq_type, rc);
			log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_REQUEST,
				LOG_DEBUG, "?", log_buffer);
			rc = PBSE_DISPROTO;
		}
	} else if (rc != PBSE_UNKREQ) {
		(void)sprintf(log_buffer,
			"Req Body bad, dis error %d, type %d",
			rc, request->rq_type);
		log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_REQUEST,
			LOG_DEBUG, "?", log_buffer);
		rc = PBSE_DISPROTO;
	}

	return (rc);
}


/**
 * @brief
 * 		top level function to read and decode DIS based batch reply
 *
 *  	Calls decode_DIS_replySvrRPP in case of RPP and decode_DIS_replySvr
 *  	in case of TCP to read the reply
 *
 * @see
 *		read_reg_reply, process_Dreply and process_DreplyRPP.
 *
 * @param[in] sock - socket connection from which to read reply
 * @param[out] reply - The reply structure to be returned
 * @param[in] rpp - Whether to read over tcp or rpp
 *
 * @return Error code
 * @retval DIS_SUCCESS(0) - Success
 * @retval !DIS_SUCCESS   - Failure (see dis.h)
 */
int
DIS_reply_read(int sock, struct batch_reply *preply, int rpp)
{
	if (rpp)
		return (decode_DIS_replySvrRPP(sock, preply));


	DIS_tcp_setup(sock);
	return  (decode_DIS_replySvr(sock, preply));
}
