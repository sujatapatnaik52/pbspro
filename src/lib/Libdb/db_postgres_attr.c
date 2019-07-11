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
 * @file    db_postgres_attr.c
 *
 * @brief
 *	Implementation of the attribute related functions for postgres
 *
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "pbs_db.h"
#include "db_postgres.h"

/*
 * initially allocate some space to buffer, anything more will be
 * allocated later as required. Just allocate 1000 chars, hoping that
 * most common sql's might fit within it without needing to resize
 */
#define INIT_BUF_SIZE 1000

#define   TEXTOID   25

struct str_data {
	int32_t len;
	char str[0];
};

/* Structure of array header to determine array type */
struct pg_array {
	int32_t ndim; /* Number of dimensions */
	int32_t off; /* offset for data, removed by libpq */
	Oid elemtype; /* type of element in the array */

	/* First dimension */
	int32_t size; /* Number of elements */
	int32_t index; /* Index of first element */
	/* data follows this portion */
};

/**
 * @brief
 *	Converts a postgres hstore(which is in the form of array) to attribute list.
 *
 * @param[in]	raw_array - Array string which is in the form of postgres hstore
 * @param[out]  attr_list - List of pbs_db_attr_list_t objects
 *
 * @return      Error code
 * @retval	-1 - On Error
 * @retval	 0 - On Success
 * @retval	>1 - Number of attributes
 *
 */
int
convert_array_to_db_attr_list(char *raw_array, pbs_db_attr_list_t *attr_list)
{
	int i;
	int j;
	int rows;
	char *p;
	char *attr_name;
	char *attr_value;
	pbs_db_attr_info_t *attrs = NULL;
	struct pg_array *array = (struct pg_array *) raw_array;
	struct str_data *val = (struct str_data *)(raw_array + sizeof(struct pg_array));

	if (ntohl(array->ndim) != 1 || ntohl(array->elemtype) != TEXTOID) {
		return -1;
	}

	rows = ntohl(array->size);
	attrs = malloc(sizeof(pbs_db_attr_info_t)*rows/2);
	if (!attrs)
		return -1;

	attr_list->attributes = attrs;

	for (i=0, j = 0; j < rows; i++, j+=2) {
		attr_name = val->str;
		val = (struct str_data *)((char *) val->str + ntohl(val->len));

		attr_value = val->str;
		val = (struct str_data *)((char *) val->str + ntohl(val->len));

		if (attr_name) {
			attrs[i].attr_name[sizeof(attrs[i].attr_name) -1] = '\0';
			strncpy(attrs[i].attr_name, attr_name, sizeof(attrs[i].attr_name));
			if ((p = strchr(attrs[i].attr_name, '.'))) {
				*p = '\0';
				attrs[i].attr_resc[sizeof(attrs[i].attr_resc) -1] = '\0';
				strncpy(attrs[i].attr_resc, p + 1, sizeof(attrs[i].attr_resc));
			} else
				attrs[i].attr_resc[0] = '\0';
		} else
			attrs[i].attr_name[0] = 0;

		if (attr_value && (p = strchr(attr_value, '.'))) {
			*p ='\0';
			attrs[i].attr_flags = atol(attr_value);
			attrs[i].attr_value = strdup(p + 1);
		} else {
			attrs[i].attr_flags = 0;
			attrs[i].attr_value = NULL;
		}
	}
	attr_list->attr_count = i;
	return 0;
}

/**
 * @brief
 *	Converts an attribute list to string array which is in the form of postgres hstore.
 *
 * @param[in]	attr_list - List of pbs_db_attr_list_t objects
 * @param[out]  raw_array - Array string which is in the form of postgres hstore
 *
 * @return      Error code
 * @retval	-1 - On Error
 * @retval	 0 - On Success
 *
 */
int
convert_db_attr_list_to_array(char **raw_array, pbs_db_attr_list_t *attr_list)
{
	int i;
	pbs_db_attr_info_t *attrs = attr_list->attributes;
	struct pg_array *array;
	int len = 0;
	struct str_data *val = NULL;
	int attr_val_len = 0;

	len = sizeof(struct pg_array);
	for (i = 0; i < attr_list->attr_count; i++) {
		len += sizeof(int32_t) + PBS_MAXATTRNAME + PBS_MAXATTRRESC + 1; /* include space for dot */
		attr_val_len = (attr_list->attributes[i].attr_value == NULL? 0:strlen(attr_list->attributes[i].attr_value));
		len += sizeof(int32_t) + 3 + attr_val_len + 1; /* include space for dot */
	}

	array = malloc(len);
	if (!array)
		return -1;
	array->ndim = htonl(1);
	array->off = 0;
	array->elemtype = htonl(TEXTOID);
	array->size = htonl(attr_list->attr_count * 2);
	array->index = htonl(1);

	/* point to data area */
	val = (struct str_data *)((char *) array + sizeof(struct pg_array));

	for (i = 0; i < attr_list->attr_count; ++i) {
		sprintf(val->str, "%s.%s", attrs[i].attr_name, attrs[i].attr_resc);
		val->len = htonl(strlen(val->str));

		val = (struct str_data *)(val->str + ntohl(val->len)); /* point to end */
		sprintf(val->str, "%d.%s", attrs[i].attr_flags, attrs[i].attr_value == NULL ? "": attrs[i].attr_value);
		val->len = htonl(strlen(val->str));

		val = (struct str_data *)(val->str + ntohl(val->len)); /* point to end */
	}
	*raw_array = (char *) array;

	return ((char *) val - (char *) array);
}

/**
 * @brief
 *	Frees attribute list memory
 *
 * @param[in]	attr_list - List of pbs_db_attr_list_t objects
 *
 * @return      None
 *
 */
void
free_db_attr_list(pbs_db_attr_list_t *attr_list)
{
	if (attr_list->attributes != NULL) {
		if (attr_list->attr_count > 0) {
			int i;
			for (i=0; i < attr_list->attr_count; i++) {
				free(attr_list->attributes[i].attr_value);
			}
		}
		free(attr_list->attributes);
		attr_list->attributes = NULL;
	}
}
