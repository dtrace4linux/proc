/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          meminfo.c                                          */
/*  Author:        P. D. Fox                                          */
/*  Created:       29 Jul 2011                                        */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  /proc/meminfo display interface                     */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 29-Jul-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int meminfo_idx = -1;
static int width;

void
display_meminfo()
{	int	i, row = 0;
	graph_t	*g = graphs_new();

	if (meminfo_idx < 0)
		meminfo_idx = mon_find("meminfo", &width);

	for (i = meminfo_idx; ; i++) {
		char *name = mon_name(i);
		unsigned long long v, v0;
		if (name == NULL || strncmp(name, "meminfo.", 8) != 0)
			break;

		v = mon_get_item(i, 0);
		if (v == 0)
			continue;

		v0 = mon_get_item(i, -1);

		set_attribute(GREEN, BLACK, 0);
		print("%-*s ", width + 1 - 8, name + 8);
		print_number(i - meminfo_idx, 12, 0, v, v0);
		if (v != v0) {
			print("  %s%lld", v > v0 ? "+" : "", v - v0);
			}
		print("\n");

		graph_set(g, "draw_minmax", 1);
///		graph_set(g, "delta", 1);
		graph_set(g, "color", 0x8080c0);
		draw_graph(g, 0, mon_name(i), 
			300, 13 * (row++ + 5), 150, 11, 1., 0x1000000);
		refresh();
		graph_refresh(g);
	}
	clear_to_end_of_screen();
	graph_free(g);
}
