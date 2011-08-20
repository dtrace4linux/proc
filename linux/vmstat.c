/**********************************************************************/
/*                                                                    */
/*  File:          vmstat.c                                           */
/*  Author:        P. D. Fox                                          */
/*  Created:       29 Jul 2011                                        */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  vmstat display interface.                           */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 29-Jul-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int vmstat_idx = -1;
static int width;

void
display_vmstat()
{	int	i, row = 0;
	graph_t	*g = graphs_new();

	if (vmstat_idx < 0)
		vmstat_idx = mon_find("vmstat", &width);

	for (i = vmstat_idx; ; i++) {
		char *name = mon_name(i);
		unsigned long long v, v0;
		if (name == NULL || strncmp(name, "vmstat.", 7) != 0)
			break;

		v = mon_get_item(i, 0);
		if (v == 0)
			continue;

		v0 = mon_get_item(i, -1);

		set_attribute(GREEN, BLACK, 0);
		print("%-*s ", width + 1 - 7, name + 7);
		print_number(i - vmstat_idx, 16, 1, v, v0);
		if (v != v0) {
			print("  %s%lld", v > v0 ? "+" : "", v - v0);
			}
		print("\n");

		graph_set(g, "draw_minmax", 1);
//		graph_set(g, "delta", 1);
		graph_set(g, "color", 0xc08020);
		draw_graph(g, 0, mon_name(i), 
			400, 13 * (row++ + 5), 150, 11, 1., 0x1000000);
		refresh();
		graph_refresh(g);
	}
	clear_to_end_of_screen();
	graph_free(g);
}
