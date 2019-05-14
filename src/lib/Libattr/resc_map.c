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

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"


/**
 * @file	resc_map.c
 * @brief
 * This file contains functions for mapping a known resource type into the
 * corresponding functions.  They are used for adding custom resources where
 * the type is specified as a string such as "boolean" or "string".
 *
 * The mapping is accomplished by the resc_type_map structure which has an
 * entry for the various types of resouces.
 *
 * At some point, it might make sense to merge this with the resource_def
 * used by the Server.
 */

static struct resc_type_map resc_type_map_arr[] = {
	{	"boolean",
		ATR_TYPE_BOOL,
		decode_b,
		encode_b,
		set_b,
		comp_b,
		free_null },

	{	"long",
		ATR_TYPE_LONG,
		decode_l,
		encode_l,
		set_l,
		comp_l,
		free_null },

	{	"string",
		ATR_TYPE_STR,
		decode_str,
		encode_str,
		set_str,
		comp_str,
		free_str },

	{	"size",
		ATR_TYPE_SIZE,
		decode_size,
		encode_size,
		set_size,
		comp_size,
		free_null },

	{	"float",
		ATR_TYPE_FLOAT,
		decode_f,
		encode_f,
		set_f,
		comp_f,
		free_null },

	{	"string_array",
		ATR_TYPE_ARST,
		decode_arst,
		encode_arst,
		set_arst,
		comp_arst,
		free_arst }

};

/**
 * @brief
 *	Return a pointer to the resc_type_map entry corresponding to the
 *	numerical resource type as defined by ATR_TYPE_*.
 *
 * @par Functionality:
 *	Indexes through the resc_type_map array until a match is found or the
 *	end of the table is reached.
 *
 * @param[in]	typenum - Resource type value
 *
 * @return	resc_type_map *
 * @retval	pointer to map entry on success.
 * @retval	NULL if no matching entry found.
 *
 * @par Side Effects: None
 *
 * @par MT-safe: yes
 *
 */
struct resc_type_map *
find_resc_type_map_by_typev(int typenum)
{
	int i;
	int s = sizeof(resc_type_map_arr) / sizeof(struct resc_type_map);

	for (i=0; i< s; ++i) {
		if (resc_type_map_arr[i].rtm_type == typenum)
			return (&resc_type_map_arr[i]);
	}
	return NULL;	/* didn't find the matching type */
}

/**
 * @brief
 *	Return a pointer to the resc_type_map entry corresponding to the
 *	resource type as specified as a string.
 *
 * @par Functionality:
 *	Indexes through the resc_type_map array until a match is found or the
 *	end of the table is reached.
 *
 * @param[in]	typestr - Resource type as a string
 *
 * @return	resc_type_map *
 * @retval	pointer to map entry on success.
 * @retval	NULL if no matching entry found.
 *
 * @par Side Effects: None
 *
 * @par MT-safe: yes
 *
 */
struct resc_type_map *
find_resc_type_map_by_typest(char *typestr)
{
	int i;
	int s = sizeof(resc_type_map_arr) / sizeof(struct resc_type_map);

	if (typestr == NULL)
		return NULL;

	for (i=0; i< s; ++i) {
		if (strcmp(typestr, resc_type_map_arr[i].rtm_rname) == 0)
			return (&resc_type_map_arr[i]);
	}
	return NULL;	/* didn't find the matching type */
}


/**
 * @brief
 * 	Returns a string representation of numeric permission flags associated
 * 	to a resource
 *
 * @param[in] perms - The permission flags of the resource
 *
 * @par
 * 	Caller is responsible of freeing the returned string
 *
 * @return	string
 * @retval	flag val as string	success
 * @retval	NULL			error
 */
char *
find_resc_flag_map(int perms)
{
	char *flags;
	int i = 0;

	/* 10 is a bit over the max number of flags that could be set */
	flags = malloc(10 * sizeof(char));
	if (flags == NULL) {
		return NULL;
	}
	if (perms & ATR_DFLAG_CVTSLT)
		flags[i++] = 'h';
	if (perms & ATR_DFLAG_RASSN)
		flags[i++] = 'q';
	if (perms & ATR_DFLAG_ANASSN)
		flags[i++] = 'n';
	else if (perms & ATR_DFLAG_FNASSN)
		flags[i++] = 'f';
	if ((perms & (ATR_DFLAG_USRD | ATR_DFLAG_USWR)) == 0)
		flags[i++] = 'i';
	else if ((perms & ATR_DFLAG_USWR) == 0)
		flags[i++] = 'r';

	flags[i] = '\0';
	return flags;
}
