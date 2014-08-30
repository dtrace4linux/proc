/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          interrupts.c                                       */
/*  Author:        P. D. Fox                                          */
/*  Created:       10 Sep 2011                                        */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  /proc/interrupts                                    */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 10-Sep-2011 1.1 $ */
/**********************************************************************/


# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int interrupt_idx = -1;
static int width;
extern int num_cpus;

void
display_interrupts()
{	int	col, i, j, row = 0;
	graph_t	*g;

	if (interrupt_idx < 0)
		interrupt_idx = mon_find("interrupts", &width);

	if (interrupt_idx < 0) {
		print("Error retrieving /proc/interrupts data.");
		clear_to_end_of_screen();
		return;
		}

	g = graph_new();
	graph_clear_chain(g);
	graph_refresh(g);

	print("%*s ", width - 11 + 4, "");
	for (i = 0; i < num_cpus; i++) {
		if (mode == MODE_ABS)
			print(" ");
		print(" CPU%-2d ", i + 1);
		}
	for (col = 99, i = interrupt_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;

		if (name == NULL || strncmp(name, "interrupts.", 11) != 0)
			break;

		/***********************************************/
		/*   If  all  cpus are zero, dont display the  */
		/*   row.				       */
		/***********************************************/
		name += 11;
		dot = strrchr(name, '.') + 1;
		++col;
		if (strcmp(dot, "0") == 0) {
			char	*dash = strchr(name, '.');
			int	found = 0;
			for (j = i; ; j++) {
				char *name2 = mon_name(j);
				if (name2 == NULL || 
				    strncmp(name2, "interrupts.", 11) != 0 ||
				    (j != i && strcmp(strrchr(name2, '.'), ".0") == 0))
					break;
				if (mon_get_item(j, 0)) {
					found = 1;
					break;
					}
				}
			if (!found) {
				col--;
				i = j - 1;
				continue;
				}

			col = 1;
			if (row) {
				row++;
				print("\n");
				}
			row++;
			print("\n");
			set_attribute(GREEN, BLACK, 0);
			print("%-*.*s ", width - 8, dash - name, name);
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - interrupt_idx, 7, 0, v, v0);

		/***********************************************/
		/*   Draw graph.			       */
		/***********************************************/
		graph_clear(g);
		graph_setforeground(g, 0xffffff);
		graph_setbackground(g, 0x80ffff);

		graph_set(g, "x", (width + 2 + (col-2) * 8) * 7);
		graph_set(g, "y", (row + 6) * 13);
		graph_set(g, "height", 13);
		graph_set(g, "width", 7 * 7 - 5);
		graph_set(g, "background_color", 0x306080);
		graph_set(g, "color", 0xff80c0);
		for (j = 7 * 7; j > 0; j--) {
			unsigned long long v = mon_get_rel(mon_name(i), -j);
			unsigned long long v0 = mon_get_rel(mon_name(i), -j - 1);
			graph_add(g, j, v - v0);
			}
		graph_plot(g);
		graph_refresh(g);

	}
	print("\n");
	clear_to_end_of_screen();
	graph_free(g);
}
