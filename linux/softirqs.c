/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          softirqs.c                                         */
/*  Author:        P. D. Fox                                          */
/*  Created:       1 Aug 2011                                         */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  softirqs                                            */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 01-Aug-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int softirqs_idx = -1;
static int width;
extern int num_cpus;

void
display_softirqs()
{	int	i;
	struct stat sbuf;

	if (stat("/proc/softirqs", &sbuf) < 0) {
		print("/proc/softirqs not available on this machine.\n");
		clear_to_end_of_screen();
		return;
		}

	if (softirqs_idx < 0)
		softirqs_idx = mon_find("softirqs", &width);
	if (softirqs_idx < 0) {
		print("cannot find softirqs in mmap file.\n");
		clear_to_end_of_screen();
		return;
		}

	print("%*s  ", width - 9, "");
	for (i = 0; i < num_cpus; i++) {
		if (mode == MODE_ABS)
			print(" ");
		print(" CPU%-2d ", i + 1);
		}
	for (i = softirqs_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;

		if (name == NULL || strncmp(name, "softirqs.", 9) != 0)
			break;

		name += 9;
		if ((dot = strchr(name, '.')) == NULL) {
			print("\n");
			set_attribute(GREEN, BLACK, 0);
			print("%-*s ", width - 9, name);
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - softirqs_idx, 7, 0, v, v0);
	}
	print("\n");
	clear_to_end_of_screen();
}
