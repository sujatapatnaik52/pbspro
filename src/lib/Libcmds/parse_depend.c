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
 * @file	parse_depend.c
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include "pbs_ifl.h"
#include "cmds.h"


static char *deptypes[] = {
	"on",		/* "on" and "synccount" must be first two */
	"synccount",
	"after",
	"afterok",
	"afternotok",
	"afterany",
	"before",
	"beforeok",
	"beforenotok",
	"beforeany",
	"syncwith",
	NULL
};

/**
 * @brief
 *	Append to an allocated string which will be expanded as needed.
 *
 * @param[in/out]	dest	destination location (malloc'ed)
 * @param[in]		str	source string
 * @param[in/out]	size	length of destination allocation
 *
 * @return 0	success
 * @return 1	failure
 */
static int
append_string(char **dest, char *str, int *size)
{
	size_t	used, add;

	if (dest == NULL || *dest == NULL || str == NULL ||
		size == NULL || *size == 0)
		return 1;

	used = strlen(*dest);
	add = strlen(str);
	if (used + add > *size) {
		char	*temp;
		int	newsize = 2*(used + add);

		temp = (char *)realloc(*dest, newsize);
		if (temp == NULL)
			return 1;
		*dest = temp;
		*size = newsize;
	}
	strcat(*dest, str);
	return 0;
}

/**
 * @brief
 * 	Parse a string of depend jobs.
 *
 * @param[in]		depend_list	depend jobs syntax: "jobid[:jobid...]"
 * @param[in/out]	rtn_list	expanded jobids appended here
 * @param[in/out]	rtn_size	size of rtn_list
 *
 * @return      int
 * @retval      0       success
 * @retval      1       failure
 *
 */
static int
parse_depend_item(char *depend_list, char **rtn_list, int *rtn_size)
{
	char *at;
	int i = 0;
	int first = 1;
	char *b1, *b2;
	char *s = NULL;
	char *c;
	char full_job_id[PBS_MAXCLTJOBID+1];
	char server_out[PBS_MAXSERVERNAME + PBS_MAXPORTNUM + 2];

	/* Begin the parse */
	c = depend_list;

	/* Loop on strings between colons */
	while ((c != NULL) && (*c != '\0')) {
		s = c;
		while (((*c != ':') || ((c != depend_list) && (*(c-1) == '\\'))) && (*c != '\0')) c++;
		if (s == c) return 1;

		if (*c == ':') {
			*c++ = '\0';
		}

		if (first) {
			first = 0;
			for (i=0; deptypes[i]; ++i) {
				if (strcmp(s, deptypes[i]) == 0)
					break;
			}
			if (deptypes[i] == NULL)
				return 1;
			if (append_string(rtn_list, deptypes[i], rtn_size))
				return 1;

		} else {

			if (i < 2) {		/* for "on" and "synccount", number */
				if (append_string(rtn_list, s, rtn_size))
					return 1;
			} else {		/* for others, job id */
				at = strchr(s, (int)'@');
				if (get_server(s, full_job_id, server_out) != 0)
					return 1;
				/* disallow subjob or range of subjobs, [] ok */
				if ((b1 = strchr(full_job_id, (int)'[')) != NULL) {
					if ((b2 = strchr(full_job_id, (int)']')) != NULL)
						if (b2 != b1+1) {
							fprintf(stderr,
								"cannot have "
								"dependancy on subjob "
								"or range\n");
							return 1;
						}
				}
				if (append_string(rtn_list, full_job_id, rtn_size))
					return 1;
				if (at) {
					if (append_string(rtn_list, "@", rtn_size))
						return 1;
					if (append_string(rtn_list, server_out,
						rtn_size))
						return 1;
				}
			}
		}
		if (*c) {
			if (append_string(rtn_list, ":", rtn_size))
				return 1;
		}
	}
	if (s == c) return 1;

	return 0;
}

/**
 * @brief
 *	Parse dependency lists with
 * 	syntax depend_list[,depend_list...]
 *
 * @param[in]		list		dependency list
 * @param[in/out]	rtn_list	address of allocated string for parsed result
 * @param[in]		rtn_size	size of rtn_list buffer
 *
 * @return	int
 * @retval 	0 	success
 * @retval 	1 	failure
 *
 * @par Side Effects:
 * May exit if malloc fails.
 */
int
parse_depend_list(char *list, char **rtn_list, int rtn_size)

{
	char *b, *c, *s, *lc;
	int comma = 0;

	if (list == NULL || rtn_list == NULL || *rtn_list == NULL || rtn_size == 0)
		return 1;

	if (strlen(list) == 0) return (1);

	if ((lc = (char *)malloc(strlen(list)+1)) == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	strcpy(lc, list);
	c = lc;
	**rtn_list = '\0';

	while (*c != '\0') {
		/* Drop leading white space */
		while (isspace(*c)) c++;

		/* Find the next comma */
		s = c;
		while (*c != ',' && *c) c++;

		/* Drop any trailing blanks */
		comma = (*c == ',');
		*c = '\0';
		b = c - 1;
		while (isspace((int)*b)) *b-- = '\0';


		/* Parse the individual list item */

		if (parse_depend_item(s, rtn_list, &rtn_size)) {
			free(lc);	
			return 1;
		}

		if (comma) {
			c++;
			append_string(rtn_list, ",", &rtn_size);
		}
	}
	free(lc);

	if (comma) return 1;

	return 0;
}
