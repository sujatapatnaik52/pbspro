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
 * @file	cvtdate.c
 * @brief
 * 	cvtdate - convert POSIX touch date/time to seconds since epoch time
 */


#include <pbs_config.h>   /* the master config generated by configure */

#include "cmds.h"


/**
 * @brief
 * 	cvtdate - convert POSIX touch date/time to seconds since epoch time
 *
 * @param[in]	datestr - date/time string in the form: [[[[CC]YY]MM]DD]hhmm[.SS]
 *			  as defined by POSIX.
 *
 *		CC = centry, ie 19 or 20
 *		YY = year, if CC is not provided and YY is < 69, then
 *		     CC is assumed to be 20, else 19.
 *		MM = Month, [1,12], if YY is not provided and MM is less than
 *		     the current month, YY is next year, else it is the
 *		     current year.
 *		DD = Day of month, [1,31], if MM is not provided and DD is less
 *		     than the current day, MM is next month, else it is the
 *		     next month.
 *		hh = hour, [00, 23], if DD is not provided and hh is less than
 *		     the current hour, DD is tomorrow, else it is today.
 *		mm = minute, [00, 59]
 *		SS = seconds, [00, 59]
 *
 * @return	time_t
 * @retval	number of seconds since epoch (Coordinated Univ. Time)
 * @retval	-1 if error.
 */

time_t
cvtdate(char *datestr)
{
	char	   buf[3];
	time_t	   clock;
	int	   i;
	char      *pc;
	struct tm  tm;
	int	   year = 0;
	int	   month = -1;
	int	   day = 0;
#ifdef WIN64
	SYSTEMTIME win_ltm;
#endif /* WIN64 */
	struct tm  ltm;
	struct tm *ptm;

	if ((pc = strchr(datestr, (int)'.')) != 0) {
		*pc++ = '\0';
		if ((strlen(pc) != 2) ||
			(isdigit((int)*pc) == 0) ||
			(isdigit((int)*(pc+1)) == 0))
			return (-1);
		tm.tm_sec = atoi(pc);
		if (tm.tm_sec > 59)
			return (-1);
	} else
		tm.tm_sec = 0;

	for (pc = datestr; *pc; ++pc)
		if (isdigit((int)*pc) == 0)
			return (-1);

	buf[2] = '\0';
	clock = time(NULL);
#ifdef WIN64
	GetLocalTime(&win_ltm);
	ltm.tm_year = win_ltm.wYear - 1900; /* unix is counted from 1900 */
	ltm.tm_mon = win_ltm.wMonth - 1; /* unix starts from 0 */
	ltm.tm_mday = win_ltm.wDay;
	ltm.tm_hour = win_ltm.wHour;
	ltm.tm_min = win_ltm.wMinute;
	ltm.tm_sec = win_ltm.wSecond;
	ltm.tm_isdst = -1;
#else
	localtime_r(&clock, &ltm);
#endif /* WIN64 */
	ptm = &ltm;
	tm.tm_year = ptm->tm_year;	/* default year to current */
	tm.tm_mon  = ptm->tm_mon;	/* default month to current */
	tm.tm_mday = ptm->tm_mday;	/* default day to current */

	switch (strlen(datestr)) {

		case 12:		/* CCYYMMDDhhmm */
			buf[0] = datestr[0];
			buf[1] = datestr[1];
			year   = atoi(buf) * 100;
			datestr += 2;

			/* no break, fall into next case */

		case 10:		/* YYMMDDhhmm */
			buf[0] = datestr[0];
			buf[1] = datestr[1];
			i      = atoi(buf);
			if (year == 0)
				if (i > 68)
					year = 1900 + i;
			else
				year = 2000 + i;

			else
				year += i;
			tm.tm_year = year - 1900;
			datestr += 2;

			/* no break, fall into next case */

		case 8:		/* MMDDhhmm */
			buf[0] = datestr[0];
			buf[1] = datestr[1];
			i = atoi(buf);
			if (i < 1 || i > 12)
				return (-1);
			if (year == 0)
				if (i <= ptm->tm_mon)
					tm.tm_year++;
			month = i - 1;
			tm.tm_mon = month;
			datestr += 2;

			/* no break, fall into next case */

		case 6:		/* DDhhmm */
			buf[0] = datestr[0];
			buf[1] = datestr[1];
			day = atoi(buf);
			if (day < 1 || day > 31)
				return (-1);
			if (month == -1)
				if (day < ptm->tm_mday)
					tm.tm_mon++;
			tm.tm_mday = day;
			datestr += 2;

			/* no break, fall into next case */

		case 4:		/* hhmm */
			buf[0] = datestr[0];
			buf[1] = datestr[1];
			tm.tm_hour = atoi(buf);
			if (tm.tm_hour > 23)
				return (-1);

			tm.tm_min = atoi(&datestr[2]);	/* mm -  minute portion */
			if (tm.tm_min > 59)
				return (-1);
			if (day == 0)			/* day not specified */
				if ((tm.tm_hour < ptm->tm_hour) ||
					((tm.tm_hour == ptm->tm_hour) &&
					(tm.tm_min <= ptm->tm_min)))
					tm.tm_mday++;	/* time for tomorrow */

			break;

		default:
			return (-1);
	}

	tm.tm_isdst = -1;

	return (mktime(&tm));
}

/**
 * @brief
 *	convert_time - convert a string time_t into
 *		       a short human readable string
 *		       today     : time of resv (i.e. 15:30)
 *		       this year : day of month & time of resv (Mar 24 15:30)
 *		       else      : day of month and year (Mar 24 2000)
 *
 * @param[in]  ptime - the string time_t
 *
 *
 * @return 	a pointer to a static string
 * @retval	converted time			success
 *
 */
char *
convert_time(char *ptime)
{
	static char buf[64];
	struct tm *ptm;		/* used to get a struct tm from localtime() */
	struct tm now_tm;		/* current time */
	struct tm then_tm;		/* time to print */
	time_t then;
	time_t now;

	time(&now);
	then = atol(ptime);

	ptm = localtime(&now);
	now_tm = *ptm;

	ptm = localtime(&then);
	then_tm = *ptm;

	if (then_tm.tm_year == now_tm.tm_year) {
		/* time input is a time within this current year */

		if (then_tm.tm_yday == now_tm.tm_yday)

			/* time input is a time within the current day */
			strftime(buf, 64, "Today %H:%M", &then_tm);

		else if ((then_tm.tm_yday >= now_tm.tm_yday - now_tm.tm_wday) &&
			(then_tm.tm_yday <= now_tm.tm_yday +  6 - now_tm.tm_wday))

			/* time input is a time within the current week */
			strftime(buf, 64, "%a %H:%M", &then_tm);
		else

			/* time input is in the current year and outside the current week */
			strftime(buf, 64, "%a %b %d %H:%M", &then_tm);

	} else {

		/* time input outside the current year */
		strftime(buf, 64, "%a %b %d %Y %H:%M", &then_tm);
	}

	return buf;
}
