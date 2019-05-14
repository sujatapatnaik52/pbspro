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
 * @file	cs_logerr.c
 * @brief
 *	This function is ment to be called by the "CS" library code in the
 *	case where the CS library is being used in a command executable.
 *
 * @note
 *	  A function by the same name but with a different definition
 *	is also part of PBS' Liblog library.  We can do this because the PBS
 *	commands are not linked against the Liblog library.  That function
 *	will be the one used by the CS library when the executable is a PBS
 *	daemon.
 */

#include <stdio.h>   /* the master config generated by configure */


/**
 * @brief
 *	prints error message when cs library is 
 *	being used in a command executable.
 *
 * param[in] ecode - error code
 * param[in] caller - function
 * param[in] txtmsg - error message 
 *
 * @return	Void
 *
 */

void
cs_logerr(int ecode, char *caller, char *txtmsg)
{
	if (caller != NULL && txtmsg != NULL) {

		if (ecode != -1)
			fprintf(stderr, "%s: %s (%d)\n", caller, txtmsg, ecode);
		else
			fprintf(stderr, "%s: %s\n", caller, txtmsg);
	}
}
