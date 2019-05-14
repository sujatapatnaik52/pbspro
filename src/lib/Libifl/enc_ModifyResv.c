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

#include <pbs_config.h>   /* the master config generated by configure */

#include "libpbs.h"
#include "pbs_error.h"
#include "dis.h"

/**
 * @brief encode the Modify Reservation request for sending to the server.
 *
 * @param[in] sock - socket descriptor for the connection.
 * @param[in] resv_id - Reservation identifier of the reservation that would be modified.
 * @param[in] aoplp - list of attributes that will be modified.
 *
 * @return - error code while writing data to the socket.
 */
int
encode_DIS_ModifyResv(int sock, char *resv_id, struct attropl *aoplp)
{
	int   rc = 0;

	if (resv_id == NULL)
		resv_id = "";

	if (((rc = diswui(sock, MGR_OBJ_RESV)) != 0) ||
		((rc = diswst(sock, resv_id)) != 0))
			return rc;

	return (encode_DIS_attropl(sock, aoplp));
}

