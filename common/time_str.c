/**********************************************************************/
/*                                                                    */
/*  File:          time_str.c                                         */
/*  Author:        P. D. Fox                                          */
/*  Created:       18 Mar 1992                     		      */
/*                                                                    */
/*  Copyright (c) 1992, Foxtrot Systems Ltd.                          */
/*                All Rights Reserved.                                */
/*                                                                    */
/* $Header: Last edited: 31-Mar-2021 1.5 $ 			      */
/**********************************************************************/

//# include	<machine.h>
# include	<stdio.h>
# include	<string.h>
# if defined(HAVE_MEMORY_H)
#	include	<memory.h>
# endif
# include	<time.h>
# include	<sys/time.h>
# if CR_WIN32 || CR_WIN64
#	include <windows.h>
# endif
# if defined(HAVE_WINSOCK_H)
#	include	<winsock.h>
# endif

# if defined(NO_TIME_T)
typedef long	time_t;
# endif

#define	TRUE	1
#define	FALSE	0
static int	force_datestamp;

time_t	time();
/**********************************************************************/
/*   Call  this  function  to force a date stamp on the next call to  */
/*   time_str().						      */
/**********************************************************************/
void
time_force_date()
{
	force_datestamp = 1;
}
/**********************************************************************/
/*   Function  to  return  current  time  as  hh:mm:ss  for printing  */
/*   messages.							      */
/**********************************************************************/
char *
time_str_common(int micro)
{	static char	buf[64];
	static char	time_before[64];
static time_t	last_time;
static int messages;
	time_t	curr_time;
	char	*timptr;
	int	len;

	curr_time = time((time_t *) 0);
	messages++;
	if (!micro && curr_time == last_time)
		return buf;

	if (force_datestamp)
		messages = 32767;

	timptr = ctime(&curr_time);
	if (last_time != (time_t) -1 && 
	    (curr_time > last_time + 3600 ||
	    messages > 1000 ||
	    strncmp(timptr, time_before, 10) != 0)) {
	    	messages = 0;
		strcpy(time_before, timptr);
		strcpy(buf, timptr);
		/***********************************************/
		/*   Avoid   getting   date   stamp  on  next  */
		/*   message.				       */
		/***********************************************/
		last_time = -1;
		}
	else {
		buf[0] = '\0';
		last_time = curr_time;
		}

	len = strlen(buf);
	memcpy(buf + len, timptr + 11, 8);
	buf[len+8] = '\0';
	if (micro) {
		struct timeval tval;
		gettimeofday(&tval, NULL);
		snprintf(buf+len+8, sizeof buf - 9, ".%06lu", tval.tv_usec);
		}

#if CR_WIN32 || CR_WIN64
	{
	FILETIME usrTime;
	FILETIME sysTime;
	FILETIME createTime;
	FILETIME exitTime;
	BOOL c2 = GetProcessTimes(GetCurrentProcess(), 
		&createTime, 
		&exitTime, 
		&sysTime, 
		&usrTime);
	__int64 u = *(__int64 *) &usrTime;
	__int64 s = *(__int64 *) &sysTime;
	int	len = strlen(buf);

	u = (u + s) / (10 * 1000);

	snprintf(buf + len, sizeof buf - len, " %d.%03d ",
		(int) (u / 1000), (int) (u % 1000));
	}
#endif

	force_datestamp = 0;
	return buf;
}
char *
time_str()
{
	return time_str_common(FALSE);
}
char *
time_str2()
{
	return time_str_common(TRUE);
}
char *
time_string()
{	static char buf[32];
	time_t	t = time(NULL);

	strftime(buf, sizeof buf, "%Y%m%d %H:%M:%S ", localtime(&t));
	return buf;
}
