/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

long long gettimestamp_us()
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	long long result = tv.tv_sec * 1000000LL + tv.tv_usec;

//	fprintf(stderr, "got ts in sec %ld\n", tv.tv_sec);
//	fprintf(stderr, "got ts in us %ld\n", tv.tv_usec);
//	fprintf(stderr, "result is %g\n", result * 1.0e-6);

	return result;
}

void print_timestamp_us(char *buf, long long ts)
{
	struct timeval tv;
	tv.tv_sec = ts / 1000000;
	tv.tv_usec = ts % 1000000;

	strcpy(buf, ctime(&tv.tv_sec));

	int len = strlen(buf);
	buf[len-1] = ' ';

	char bufms[50];
	sprintf(bufms, "%7.3f ms", tv.tv_usec / 1000.0);

	strcat(buf, bufms);
}
