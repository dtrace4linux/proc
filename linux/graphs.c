/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          graphs.c                                           */
/*  Author:        P. D. Fox                                          */
/*  Created:       9 Aug 2011                                         */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Graph drawing page                                  */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 09-Aug-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

# define 	WIDTH 150

void
display_graphs()
{	graph_t	*g = graph_new();
	int	i;
	unsigned long long v;

	clear_to_end_of_screen();
	graph_clear_chain(g);
	graph_refresh(g);

	draw_graph(g, 1, "loadavg", 		0,  100, WIDTH, 50, 100., 0x00c080);
	draw_graph(g, 1, "meminfo.Dirty", 		200, 100, WIDTH, 50, 1., 0x0080c0);
	draw_graph(g, 1, "meminfo.Cached", 	400, 100, WIDTH, 50, 1., 0x40b090);
	draw_graph(g, 1, "meminfo.MemFree", 	0,  170, WIDTH, 50, 1., 0xc04090);
	draw_graph(g, 1, "stat.procs_running", 	200, 170, WIDTH, 50, 1., 0x409080);

	graph_free(g);
}

static void
do_number(char *buf, int len, double f)
{
	if (f < 0.01)
		strcpy(buf, "0");
	else if (f < 10)
		snprintf(buf, len, "%.2f", f);
	else if (f < 1024)
		snprintf(buf, len, "%d", (int) f);
	else if (f < 1024 * 1024)
		snprintf(buf, len, "%dK", (int) (f / 1024));
	else if (f < 1024 * 1024 * 1024)
		snprintf(buf, len, "%.1fM", f / (1024 * 1024));
	else
		snprintf(buf, len, "%.1fG", f / (1024 * 1024 * 1024));
}
void
draw_graph(graph_t *g, int flags, char *item, int x, int y, 
	int width, int height, double scale, int bg_color)
{	int	i;
	char	buf[BUFSIZ];
	unsigned long long v0 = 0;

	graph_clear(g);
	graph_setfont(g, "6x9");
	graph_setforeground(g, 0xffffff);
	graph_setbackground(g, 0xffffff);
	if (flags & 0x01)
		graph_drawstring(g, x, y - 3, item);

	graph_set(g, "x", x);
	graph_set(g, "y", y);
	graph_set(g, "height", height);
	graph_set(g, "width", width);
	graph_set(g, "background_color", bg_color);
	for (i = -width; i <= 0; i++) {
		unsigned long long v = mon_get_rel(item, i);
		if (strncmp(item, "meminfo", 7) == 0)
			v *= 1024;
		if (flags & 0x02 && i == -width)
			;
		else
			graph_add(g, i, (flags & 0x02 ? v - v0 : v) / scale);
		v0 = v;
		}
	graph_plot(g);

	if (height > 30) {	
		do_number(buf, sizeof buf, graph_float_value(g, "max_y"));
		graph_drawstring(g, x + width, y, buf);

		do_number(buf, sizeof buf, graph_float_value(g, "min_y"));
		graph_drawstring(g, x + width, y + height, buf);
		}

	graph_refresh(g);
}
graph_t *
graphs_new()
{	graph_t *g = graph_new();

	graph_clear_chain(g);
	graph_refresh(g);
	graph_setfont(g, "6x9");
	return g;
}

