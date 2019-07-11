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
 * @file    job_recov.c
 *
 * @brief
 *	job_recov.c - This file contains the functions to record a job
 *	data struture to disk and to recover it from disk.
 *
 *	The data is recorded in a file whose name is the job_id.
 *
 *	The following public functions are provided:
 *		job_save_fs() -		save the disk image
 *		job_or_resv_save_fs() -	save the disk image (job or reservation)
 *		job_recov_fs() -		recover (read) job from disk
 *		job_or_resv_recov_fs() -	recover (read) job or reservation from disk
 *		recov_514_extend	-	recover an older version of the job extend area
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>

#ifndef WIN64
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

#ifdef WIN64
#include <sys/stat.h>
#include <io.h>
#include <windows.h>
#include "win.h"
#endif

#include "job.h"
#include "reservation.h"
#include "queue.h"
#include "log.h"
#include "pbs_nodes.h"
#include "svrfunc.h"
#include <memory.h>
#include "libutil.h"


#define MAX_SAVE_TRIES 3

/* global data items */

extern char  *path_jobs;
extern char  *path_resvs;
extern time_t time_now;
extern char   pbs_recov_filename[];

/* data global only to this file */

static const size_t fixedsize = sizeof(struct jobfix);
static const size_t extndsize = sizeof(union jobextend);


/**
 * @brief
 *		Saves (or updates) a job structure image on disk
 *
 *		Save does either - a quick update for state changes only,
 *			 - a full update for an existing file, or
 *			 - a full write for a new job
 *
 *		For a quick update, the data written is less than a disk block
 *		size and no size change occurs.
 *
 *		No need of O_SYNC flag as this will improve the performance.
 *		This might lead to data loss from file system in case of system
 *		crash. This is not an issue as data is mostly recovered from the
 *		database.
 *
 *		For a new file write, first time, the data is written directly to
 *		the file.
 *
 * @see
 * 		job_or_resv_save_fs
 *
 * @param[in]	pjob - Pointer to the job structure to save
 * @param[in]	updatetype - 0 quick, 1 full
 *
 * @return      Error code
 * @retval	 0  - Success
 * @retval	-1  - Failure
 *
 */

int
job_save_fs(job *pjob, int updatetype)
{
	int	fds;
	int	i;
	char	*filename;
	char	namebuf1[MAXPATHLEN+1];
	char	namebuf2[MAXPATHLEN+1];
	int	openflags;
	int	redo;
	int	pmode;

#ifdef WIN64
	pmode = _S_IWRITE | _S_IREAD;
#else
	pmode = 0600;
#endif

	(void)strcpy(namebuf1, path_jobs);	/* job directory path */
	if (*pjob->ji_qs.ji_fileprefix != '\0')
		(void)strcat(namebuf1, pjob->ji_qs.ji_fileprefix);
	else
		(void)strcat(namebuf1, pjob->ji_qs.ji_jobid);
	(void)strcpy(namebuf2, namebuf1);	/* setup for later */
	(void)strcat(namebuf1, JOB_FILE_SUFFIX);

	/* if ji_modified is set, ie an attribute changed, then update mtime */

	if (pjob->ji_modified) {
		pjob->ji_wattr[JOB_ATR_mtime].at_val.at_long = time_now;
		pjob->ji_wattr[JOB_ATR_mtime].at_flags |= ATR_VFLAG_MODCACHE;

	}

	if (pjob->ji_qs.ji_jsversion != JSVERSION) {
		/* version of job structure changed, force full write */
		pjob->ji_qs.ji_jsversion = JSVERSION;
		updatetype = SAVEJOB_FULLFORCE;
	}

	if (updatetype == SAVEJOB_QUICK) {

		openflags =  O_WRONLY;
		fds = open(namebuf1, openflags, pmode);
		if (fds < 0) {
			log_err(errno, "job_save", "error on open");
			return (-1);
		}
#ifdef WIN64
		secure_file(namebuf1, "Administrators",
			READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
		setmode(fds, O_BINARY);
#endif

		/* just write the "critical" base structure to the file */

		save_setup(fds);
		if ((save_struct((char *)&pjob->ji_qs, fixedsize) == 0) &&
			(save_struct((char *)&pjob->ji_extended, extndsize) == 0) &&
			(save_flush() == 0)) {
			(void)close(fds);
		} else {
			log_err(errno, "job_save", "error quickwrite");
			(void)close(fds);
			return (-1);
		}

	} else {

		/*
		 * write the whole structure to the file.
		 * For a update, this is done to a new file to protect the
		 * old against crashs.
		 * The file is written in four parts:
		 * (1) the job structure,
		 * (2) the extended area,
		 * (3) if a Array Job, the index tracking table
		 * (4) the attributes in the "encoded "external form, and last
		 * (5) the dependency list.
		 */

		(void)strcat(namebuf2, JOB_FILE_COPY);
		openflags =  O_CREAT | O_WRONLY;

#ifdef WIN64
		fix_perms2(namebuf2, namebuf1);
#endif

		if (updatetype == SAVEJOB_NEW)
			filename = namebuf1;
		else
			filename = namebuf2;

		fds = open(filename, openflags, pmode);
		if (fds < 0) {
			log_err(errno, "job_save",
				"error opening for full save");
			return (-1);
		}

#ifdef WIN64
		secure_file(filename, "Administrators",
			READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
		setmode(fds, O_BINARY);
#endif

		for (i=0; i<MAX_SAVE_TRIES; ++i) {
			redo = 0;	/* try to save twice */
			save_setup(fds);
			if (save_struct((char *)&pjob->ji_qs, fixedsize)
				!= 0) {
				redo++;
			} else if (save_struct((char *)&pjob->ji_extended,
				extndsize) != 0) {
				redo++;
#ifndef PBS_MOM
			} else if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_ArrayJob) &&
				(save_struct((char *)pjob->ji_ajtrk,
				pjob->ji_ajtrk->tkm_size) != 0)) {
				redo++;
#endif
			} else if (save_attr_fs(job_attr_def, pjob->ji_wattr,
				(int)JOB_ATR_LAST) != 0) {
				redo++;
			} else if (save_flush() != 0) {
				redo++;
			}
			if (redo != 0) {
				if (lseek(fds, (off_t)0, SEEK_SET) < 0) {
					log_err(errno, "job_save", "error lseek");
				}
			} else
				break;
		}


		(void)close(fds);
		if (i >= MAX_SAVE_TRIES) {
			if ((updatetype == SAVEJOB_FULL) ||
				(updatetype == SAVEJOB_FULLFORCE))
				(void)unlink(namebuf2);
			return (-1);
		}

		if ((updatetype == SAVEJOB_FULL) ||
			(updatetype == SAVEJOB_FULLFORCE)) {
#ifdef WIN64
			if (MoveFileEx(namebuf2, namebuf1,
				MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) == 0) {

				errno = GetLastError();
				sprintf(log_buffer, "MoveFileEx(%s,%s) failed!",
					namebuf2, namebuf1);
				log_err(errno, "job_save", log_buffer);
			}
			secure_file(namebuf1, "Administrators",
				READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
#else
			if (rename(namebuf2, namebuf1) == -1) {
				log_event(PBSEVENT_ERROR|PBSEVENT_SECURITY,
					PBS_EVENTCLASS_JOB, LOG_ERR,
					pjob->ji_qs.ji_jobid,
					"rename in job_save failed");
			}
#endif
		}
		pjob->ji_modified = 0;
	}
	return (0);
}

/**
 * @brief
 * 		recov_514_extend - recover an older version of the job extend area
 *
 * @see
 * 		job_recov_fs
 *
 * @param[in]	fds - job extend area, file descriptor.
 * @param[out] 	pj - job structure where recovered information is stored.
 *
 * @return	int
 * @retval	0	success
 * @retval	1	failure
 */
static int
recov_514_extend(int fds, job *pj)
{
	union jobextend_514 old;

	if (read(fds, (char *)&old, sizeof(old)) != sizeof(old))
		return 1;
#if defined(__sgi)
	pj->ji_extended.ji_ext.ji_jid = old.ji_ext.ji_jid;
	pj->ji_extended.ji_ext.ji_ash = old.ji_ext.ji_ash;
#else
	memcpy(pj->ji_extended.ji_ext.ji_4jid, old.ji_ext.ji_4jid,
		sizeof(old.ji_ext.ji_4jid));
	memcpy(pj->ji_extended.ji_ext.ji_4ash, old.ji_ext.ji_4ash,
		sizeof(old.ji_ext.ji_4ash));
#endif	/* __sgi */
	pj->ji_extended.ji_ext.ji_credtype = old.ji_ext.ji_credtype;

	/*
	 * for Mom,  ji_extended.ji_ext.ji_nodeidx and
	 * ji_extended.ji_ext.ji_taskid, the newer elements are preset to
	 * zero in job_alloc()
	 */
	return 0;
}

/**
 * @brief
 *		recover (read in) a job from its save file
 *
 *		This function is only needed upon server start up.
 *
 *		The job structure, its attributes strings, and its dependencies
 *		are recovered from the disk.  Space to hold the above is
 *		malloc-ed as needed.
 *
 * @see
 * 		svr_migrate_data_from_fs
 *
 * @param[in]	filename	- Name of job file to load job from
 *
 * @return	pointer to new job structure
 *
 * @retval	 NULL - Failure
 * @retval	!NULL - Success
 *
 */

job *
job_recov_fs(char *filename)
{
	int		 fds;
	char		 basen[MAXPATHLEN+1];
	job		*pj;
	char		*pn;
	char		*psuffix;


	pj = job_alloc();	/* allocate & initialize job structure space */
	if (pj == NULL) {
		return NULL;
	}

	(void)strcpy(pbs_recov_filename, path_jobs);	/* job directory path */
	(void)strcat(pbs_recov_filename, filename);
#ifdef WIN64
	fix_perms(pbs_recov_filename);
#endif

	/* change file name in case recovery fails so we don't try same file */

	(void)strcpy(basen, pbs_recov_filename);
	psuffix = basen + strlen(basen) - strlen(JOB_BAD_SUFFIX);
	(void)strcpy(psuffix, JOB_BAD_SUFFIX);
#ifdef WIN64
	if (MoveFileEx(pbs_recov_filename, basen,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
		errno = GetLastError();
		sprintf(log_buffer, "MoveFileEx(%s, %s) failed!",
			pbs_recov_filename, basen);
		log_err(errno, "nodes", log_buffer);

	}
	secure_file(basen, "Administrators",
		READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
#else
	if (rename(pbs_recov_filename, basen) == -1) {
		sprintf(log_buffer, "error renaming job file %s",
			pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		free((char *)pj);
		return NULL;
	}
#endif

	fds = open(basen, O_RDONLY, 0);
	if (fds < 0) {
		sprintf(log_buffer, "error opening of job file %s",
			pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		free((char *)pj);
		return NULL;
	}
#ifdef WIN64
	setmode(fds, O_BINARY);
#endif

	/* read in job fixed sub-structure */

	errno = -1;
	if (read(fds, (char *)&pj->ji_qs, fixedsize) != (int)fixedsize) {
		sprintf(log_buffer, "error reading fixed portion of %s",
			pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		free((char *)pj);
		(void)close(fds);
		return NULL;
	}
	/* Does file name match the internal name? */
	/* This detects ghost files */

#ifdef WIN64
	pn = strrchr(pbs_recov_filename, (int)'/');
	if (pn == NULL)
		pn = strrchr(pbs_recov_filename, (int)'\\');
	if (pn == NULL) {
		sprintf(log_buffer, "bad path %s", pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		free((char *)pj);
		(void)close(fds);
		return NULL;
	}
	pn++;
#else
	pn = strrchr(pbs_recov_filename, (int)'/') + 1;
#endif

	if (strncmp(pn, pj->ji_qs.ji_jobid, strlen(pn)-3) != 0) {
		/* mismatch, discard job */

		(void)sprintf(log_buffer,
			"Job Id %s does not match file name for %s",
			pj->ji_qs.ji_jobid,
			pbs_recov_filename);
		log_err(-1, "job_recov", log_buffer);
		free((char *)pj);
		(void)close(fds);
		return NULL;
	}

	/* read in extended save area depending on VERSION */

	errno = -1;
	DBPRT(("Job save version %d\n", pj->ji_qs.ji_jsversion))
	if (pj->ji_qs.ji_jsversion < JSVERSION_514) {
		/* If really old version, it wasn't there, abort out */
		sprintf(log_buffer,
			"Job structure version cannot be recovered for job %s",
			pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		free((char *)pj);
		(void)close(fds);
		return NULL;
	} else if (pj->ji_qs.ji_jsversion < JSVERSION_80) {
		/* If older version, read and copy extended area     */
		if (recov_514_extend(fds, pj) != 0) {
			sprintf(log_buffer,
				"error reading extended portion"
				" of %s for prior version",
				pbs_recov_filename);
			log_err(errno, "job_recov", log_buffer);
			free((char *)pj);
			(void)close(fds);
			return NULL;
		}
	} else {
		/* If current version, JSVERSION_80, read into place */
		if (read(fds, (char *)&pj->ji_extended,
			sizeof(union jobextend)) !=
			sizeof(union jobextend)) {
			sprintf(log_buffer,
				"error reading extended portion of %s",
				pbs_recov_filename);
			log_err(errno, "job_recov", log_buffer);
			free((char *)pj);
			(void)close(fds);
			return NULL;
		}
	}
#ifndef PBS_MOM
	if (pj->ji_qs.ji_svrflags & JOB_SVFLG_ArrayJob) {
		size_t xs;

		if (read(fds, (char *)&xs, sizeof(xs)) != sizeof(xs)) {
			sprintf(log_buffer,
				"error reading array section of %s",
				pbs_recov_filename);
			log_err(errno, "job_recov", log_buffer);
			free((char *)pj);
			(void)close(fds);
			return NULL;
		}
		if ((pj->ji_ajtrk = (struct ajtrkhd *)malloc(xs)) == NULL) {
			free((char *)pj);
			(void)close(fds);
			return NULL;
		}
		read(fds, (char *)pj->ji_ajtrk + sizeof(xs), xs - sizeof(xs));
		pj->ji_ajtrk->tkm_size = xs;
	}
#endif	/* not PBS_MOM */

	/* read in working attributes */

	if (recov_attr_fs(fds, pj, job_attr_def, pj->ji_wattr, (int)JOB_ATR_LAST,
		(int)JOB_ATR_UNKN) != 0) {
		sprintf(log_buffer, "error reading attributes portion of %s",
			pbs_recov_filename);
		log_err(errno, "job_recov", log_buffer);
		job_free(pj);
		(void)close(fds);
		return NULL;
	}
	(void)close(fds);

#if defined(PBS_MOM) && defined( WIN64)
	/* get a handle to the job (may not exist) */
	pj->ji_hJob = OpenJobObject(JOB_OBJECT_ALL_ACCESS, FALSE,
		pj->ji_qs.ji_jobid);
#endif

	/* all done recovering the job, change file name back to .JB */

#ifdef WIN64
	if (MoveFileEx(basen, pbs_recov_filename,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
		errno = GetLastError();
		sprintf(log_buffer, "MoveFileEx(%s, %s) failed!",
			basen, pbs_recov_filename);
		log_err(errno, "nodes", log_buffer);

	}
	secure_file(pbs_recov_filename, "Administrators",
		READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
#else
	(void)rename(basen, pbs_recov_filename);
#endif

	return (pj);
}


/**
 * @brief
 *		Saves (or updates) a job or resc_resv structure
 *		image on disk
 *
 *		Save does either - a quick update for state changes only,
 *			 - a full update for an existing file, or
 *			 - a full write for a new job or reservation file
 *
 *		For a quick update, the data written is less than a disk block
 *		size and no size change occurs.
 *
 *		No need of O_SYNC flag as this will improve the performance.
 *		This might lead to data loss from file system in case of system
 *		crash. This is not an issue as data is mostly recovered from the
 *		database.
 *
 *		For a new file write, first time, the data is written directly to
 *		the file.
 *
 * @param[in] pobj - Pointer to the resv/job obj to save
 * @param[in] updatetype - Type of update, 0 = quick, 1=full, 2=new
 * @param[in] objtype - Type of object
 *						JOB_OBJECT,
 *						RESC_RESV_OBJECT,
 *						RESV_JOB_OBJECT
 *
 * @return     Error code
 * @retval	0 - Success
 * @retval	-1 - Failure
 *
 */

int
job_or_resv_save_fs(void *pobj, int updatetype, int objtype)
{
	int	fds;
	int	openflags;
	int	redo;
	char	*filename;
	char	namebuf1[MAXPATHLEN+1];
	char	namebuf2[MAXPATHLEN+1];

	char		*path = NULL;
	char		*err_msg = NULL;
	char		*err_msgl = NULL;
	char		*prefix = NULL;
	char		*suffix = NULL;
	char		*cpsuffix = NULL;

	char		*p_oid = NULL;
	long		*p_mtime = NULL;
	int		*p_modified = NULL;
	void		*pfixed = NULL;
	ssize_t		i;
	size_t		fixed_size;
	attribute_def	*p_attr_def = NULL;
	attribute	*wattr = NULL;
	int		final_attr;
	int		eventclass;
	int		pmode;

#ifdef WIN64
	pmode = _S_IWRITE | _S_IREAD;
#else
	pmode = 0600;
#endif

	if (objtype == RESC_RESV_OBJECT || objtype == RESV_JOB_OBJECT) {
#ifndef PBS_MOM	/* MOM knows not of resc_resv structs */
		resc_resv  *presv;
		presv = (resc_resv *)pobj;
		err_msg = "reservation_save";
		err_msgl = "Link in reservation_save failed";
		path = path_resvs;
		prefix = presv->ri_qs.ri_fileprefix;
		suffix = RESV_FILE_SUFFIX;
		cpsuffix = RESV_FILE_COPY;
		p_modified = &presv->ri_modified;
		pfixed = (void *)&presv->ri_qs;
		fixed_size = sizeof(struct resvfix);
		p_attr_def = resv_attr_def;
		wattr = presv->ri_wattr;
		final_attr = RESV_ATR_LAST;
		p_mtime = &presv->ri_wattr[RESV_ATR_mtime].at_val.at_long;
		p_oid = presv->ri_qs.ri_resvID;
		eventclass = PBS_EVENTCLASS_RESV;
#else	/* PBS_MOM only: Execution will never come here for MOM */
		return (-1);
#endif
	}
	else if (objtype == JOB_OBJECT) {
		job   *pj = (job *)pobj;

#ifndef PBS_MOM	/*MOM knows not of resc_resv structs*/
		if (pj->ji_resvp) {
			int	rc = 0;

			if (updatetype == SAVEJOB_QUICK)
				rc = job_or_resv_save((void *)pj->ji_resvp,
					SAVERESV_QUICK,
					RESC_RESV_OBJECT);
			else if ((updatetype == SAVEJOB_FULL) ||
				(updatetype == SAVEJOB_FULLFORCE)  ||
				(updatetype == SAVEJOB_NEW))
				rc = job_or_resv_save((void *)pj->ji_resvp,
					SAVERESV_FULL,
					RESC_RESV_OBJECT);
			if (rc)
				return (rc);
		}
#endif
		err_msg = "job_save";
		err_msgl = "Link in job_save failed";
		path = path_jobs;
		if (*pj->ji_qs.ji_fileprefix != '\0')
			prefix = pj->ji_qs.ji_fileprefix;
		else
			prefix = pj->ji_qs.ji_jobid;
		suffix = JOB_FILE_SUFFIX;
		cpsuffix = JOB_FILE_COPY;
		p_modified = &pj->ji_modified;
		pfixed = (void *)&pj->ji_qs;
		fixed_size = sizeof(struct jobfix);
		p_attr_def = job_attr_def;
		wattr = pj->ji_wattr;
		final_attr = JOB_ATR_LAST;
		p_mtime = &pj->ji_wattr[JOB_ATR_mtime].at_val.at_long;
		p_oid = pj->ji_qs.ji_jobid;
		eventclass = PBS_EVENTCLASS_JOB;
		return (job_save_fs(pj, updatetype));
	} else {
		/*Don't expect to get here; incorrect object type*/
		return (-1);
	}

	(void)strcpy(namebuf1, path);		/* directory path */
	(void)strcat(namebuf1, prefix);
	(void)strcpy(namebuf2, namebuf1);	/* setup for later */
	(void)strcat(namebuf1, suffix);

	/*if an attribute changed (modified==1) update mtime*/

	if (*p_modified) {
		*p_mtime = time_now;
	}

	if (updatetype == SAVEJOB_QUICK || updatetype == SAVERESV_QUICK) {

		openflags =  O_WRONLY;
		fds = open(namebuf1, openflags, pmode);
		if (fds < 0) {
			log_err(errno, err_msg, "error on open");
			return (-1);
		}
#ifdef WIN64
		secure_file(namebuf1, "Administrators",
			READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
		setmode(fds, O_BINARY);
#endif
		/* just write the "critical" base structure to the file */

		while ((i = write(fds, (char *)pfixed, fixed_size)) !=
			fixed_size) {
			if ((i < 0) && (errno == EINTR)) {
				/* retry the write */
				if (lseek(fds, (off_t)0, SEEK_SET) < 0) {
					log_err(errno, err_msg, "lseek");
					(void)close(fds);
					return (-1);
				}
				continue;
			} else {
				log_err(errno, err_msg, "quickwrite");
				(void)close(fds);
				return (-1);
			}
		}
		(void)close(fds);

	} else {

		/*
		 * write the whole structure to the file.
		 * For a update, this is done to a new file to protect the
		 * old against crashs.
		 * The file is written in four parts:
		 * (1) the job (resc_resv) structure,
		 * (2) the attributes in "encoded" form,
		 * (3) the attributes in the "external" form, and last
		 * (4) the dependency list.
		 */

		(void)strcat(namebuf2, cpsuffix);
		openflags =  O_CREAT | O_WRONLY;
#ifdef WIN64
		fix_perms2(namebuf2, namebuf1);
#endif
		if (updatetype == SAVEJOB_NEW || updatetype == SAVERESV_NEW)
			filename = namebuf1;
		else
			filename = namebuf2;

		fds = open(filename, openflags, pmode);
		if (fds < 0) {
			log_err(errno, err_msg, "open for full save");
			return (-1);
		}

#ifdef WIN64
		secure_file(filename, "Administrators",
			READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
		setmode(fds, O_BINARY);
#endif

		redo = 0;	/* try to save twice */
		do {
			save_setup(fds);
			if (save_struct((char *)pfixed, fixed_size) != 0) {
				redo++;
			} else if (save_attr_fs(p_attr_def, wattr, final_attr) != 0) {
				redo++;
			} else if (save_flush() != 0) {
				redo++;
			}
			if (redo != 0) {
				if (lseek(fds, (off_t)0, SEEK_SET) < 0) {
					log_err(errno, err_msg, "full lseek");
					redo++;
				}
			}
		} while (redo == 1);


		(void)close(fds);
		if (redo > 1) {
			if (updatetype == SAVEJOB_FULL ||
				updatetype == SAVEJOB_FULLFORCE ||
				updatetype == SAVERESV_FULL)
				(void)unlink(namebuf2);
			return (-1);
		}

		if (updatetype == SAVEJOB_FULL ||
			updatetype == SAVEJOB_FULLFORCE ||
			updatetype == SAVERESV_FULL) {

#ifdef WIN64
			if (MoveFileEx(namebuf2, namebuf1,
				MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) ==
				0) {
				errno = GetLastError();
				sprintf(log_buffer, "MoveFileEx(%s,%s) failed!",
					namebuf2, namebuf1);
				log_err(errno, err_msg, log_buffer);
			}
			secure_file(namebuf1, "Administrators",
				READS_MASK|WRITES_MASK|STANDARD_RIGHTS_REQUIRED);
#else
			if (rename(namebuf2, namebuf1) == -1) {
				log_event(PBSEVENT_ERROR|PBSEVENT_SECURITY,
					eventclass, LOG_ERR, p_oid, err_msgl);
			}
#endif
		}
		*p_modified = 0;
	}
	return (0);
}

/**
 * @brief
 *		Recover (read in) a job or reservation from its save file
 *
 *		This function is only needed upon server start up.
 *		It is a more general version of function job_recov ()
 *		which is only good for jobs.
 *
 *		The job (resc_resv) structure, its attributes strings,
 *		and its dependencies are recovered from the disk.
 *		Space to hold the above is malloc-ed as needed.
 *
 * @see
 * 		svr_migrate_data_from_fs
 *
 * @param[in] filename - Name of file to load object from
 * @param[in] objtype - Type of object
 *					JOB_OBJECT,
 *					RESC_RESV_OBJECT,
 *					RESV_JOB_OBJECT
 *
 * @return	Pointer to the new job (resc_resv) struct
 * @retval	NULL - Failure
 * @retval	!NULL - Success
 *
 */

void*
job_or_resv_recov_fs(char *filename, int objtype)
{
	int		 fds;
	job		*pj;
	void		*pobj = NULL;
#ifndef PBS_MOM
	resc_resv	*presv;
#endif
	void		*p_fixed = NULL;
	int		fixed_size;
	char		*prefix = NULL;
	char		*path = NULL;
	char		*err_msg;
	char		*ptcs;		/*text control string for err msg*/
	char		*pobjID = NULL;
	char		*pn;		/*name of the file "root" (prefix)*/
	attribute	*wattr = NULL;
	attribute_def	*p_attr_def = NULL;
	int		final_attr;
	int		attr_unkn;
	char		namebuf[MAXPATHLEN + 1];

	if (objtype == RESC_RESV_OBJECT) {

#ifndef PBS_MOM		/*MOM doesn't know about resource reservations*/
		presv = resc_resv_alloc();   /* allocate & init resc_rescv struct */
		if (presv == NULL) {
			return NULL;
		}
		pobj = (void *)presv;
		path = path_resvs;
		err_msg = "error opening reservation file";
		ptcs = "reservation Id %s does not match file name for %s";
		pobjID = presv->ri_qs.ri_resvID;
		p_fixed = (void *)&presv->ri_qs;
		fixed_size = sizeof(struct resvfix);
		prefix = presv->ri_qs.ri_fileprefix;
		p_attr_def = resv_attr_def;
		wattr = presv->ri_wattr;
		attr_unkn = RESV_ATR_UNKN;
		final_attr = RESV_ATR_LAST;
#else	/* PBS_MOM only: This will never come here for MOM!!! */
		return NULL;
#endif

	} else {

		pj = job_alloc();           /* allocate & initialize job struct */
		if (pj == NULL) {
			return NULL;
		}
		pobj = (void *)pj;
		path = path_jobs;
		err_msg = "error opening job file";
		ptcs = "Job Id %s does not match file name for %s";
		pobjID = pj->ji_qs.ji_jobid;
		p_fixed = (void *)&pj->ji_qs;
		fixed_size = sizeof(struct jobfix);
		if (*pj->ji_qs.ji_fileprefix != '\0')
			prefix = pj->ji_qs.ji_fileprefix;
		else
			prefix = pj->ji_qs.ji_jobid;
		p_attr_def = job_attr_def;
		wattr = pj->ji_wattr;
		attr_unkn = JOB_ATR_UNKN;
		final_attr = JOB_ATR_LAST;
	}

	(void)strcpy(namebuf, path);	/* job (reservation) directory path */
	(void)strcat(namebuf, filename);
#ifdef WIN64
	fix_perms(namebuf);
#endif
	fds = open(namebuf, O_RDONLY, 0);
	if (fds < 0) {
		sprintf(log_buffer, "%s on %s", err_msg, namebuf);
		log_err(errno, __func__, log_buffer);
		free((char *)pobj);
		return NULL;
	}
#ifdef WIN64
	setmode(fds, O_BINARY);
#endif

	/* read in job or resc_resv quick save sub-structure */

	if (read(fds, (char *)p_fixed, fixed_size) != fixed_size) {
		char err_buf[MAXPATHLEN + 32];
		snprintf(err_buf, sizeof(err_buf), "problem reading %s", namebuf);
		log_err(errno, __func__, err_buf);
		free((char *)pobj);
		(void)close(fds);
		return NULL;
	}
	/* Does file name match the internal name? */
	/* This detects ghost files */

#ifdef WIN64
	pn = strrchr(namebuf, (int)'/');
	if (pn == NULL)
		pn = strrchr(namebuf, (int)'\\');
	if (pn == NULL) {
		sprintf(log_buffer, "bad path %s", namebuf);
		log_err(errno, __func__, log_buffer);
		free((char *)pj);
		(void)close(fds);
		return NULL;
	}
	pn++;
#else
	pn = strrchr(namebuf, (int)'/') + 1;
#endif

	if (strncmp(pn, prefix, strlen(prefix)) != 0) {
		char *msgbuf;

		/* mismatch, discard job (reservation) */
		pbs_asprintf(&msgbuf, ptcs, pobjID, namebuf);
		log_err(-1, __func__, msgbuf);
		free(msgbuf);
		free((char *)pobj);
		(void)close(fds);
		return NULL;
	}

	/* read in working attributes */

	if (recov_attr_fs(fds, pobj, p_attr_def, wattr,
		final_attr, attr_unkn) != 0) {

		log_err(errno, __func__, "error from recov_attr");
		if (objtype == RESC_RESV_OBJECT) {

#ifndef PBS_MOM		/*MOM doesn't know about resource reservations*/
			resv_free((resc_resv *)pobj);
#endif
		} else {
			job_free((job *)pobj);
		}

		(void)close(fds);
		return NULL;
	}

	(void)close(fds);

#if defined(PBS_MOM) && defined( WIN64)
	/* get a handle to the job (may not exist) */
	pj->ji_hJob = OpenJobObject(JOB_OBJECT_ALL_ACCESS, FALSE,
		pj->ji_qs.ji_jobid);
#endif

	/* all done recovering the job (reservation) */

	return (pobj);
}
