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
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>

#include <dis.h>

unsigned dis_dmx10 = 0;
double *dis_dp10 = NULL;
double *dis_dn10 = NULL;
/**
 * @file	disi10d_.c
 */
/**
 * @brief
 *	-Allocate and fill tables with all powers of 10 that fit the forms:
 *
 *				  n
 *				 2
 *		*dis_dp10[n] = 10
 *
 *				  ( n)
 *				 -(2 )
 *		*dis_dn10[n] = 10
 *
 *	For all values of n supported by the floating point format.  Set
 *	dis_dmx10 equal to the largest value of n that fits the format.
 */

void
disi10d_()
{
	int		i;
	unsigned long	ul;
	dis_long_double_t	accum;
	size_t		tabsize;

	assert(dis_dp10 == NULL);
	assert(dis_dn10 == NULL);
	assert(dis_dmx10 == 0);

#if DBL_MAX_10_EXP + DBL_MIN_10_EXP > 0
	ul = DBL_MAX_10_EXP;
#else
	ul = -DBL_MIN_10_EXP;
#endif
	while (ul >>= 1)
		dis_dmx10++;
	tabsize = (dis_dmx10 + 1) * sizeof(double);
	dis_dp10 = (double *)malloc(tabsize);
	assert(dis_dp10 != NULL);
	dis_dn10 = (double *)malloc(tabsize);
	assert(dis_dn10 != NULL);
	assert(dis_dmx10 > 0);
	dis_dp10[0] = accum = 10.0L;
	dis_dn10[0] = 1.0L / accum;
	for (i = 1; i <= dis_dmx10; i++) {
		accum *= accum;
		dis_dp10[i] = accum;
		dis_dn10[i] = 1.0L / accum;
	}
}
