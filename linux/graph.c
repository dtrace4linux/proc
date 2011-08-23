/**********************************************************************/
/*                                                                    */
/*  File:          graph.c                                            */
/*  Author:        P. D. Fox                                          */
/*  Created:       9 Aug 2011                                         */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Graph drawing via fcterm                            */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 09-Aug-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<termios.h>
# include	<sys/ioctl.h>

static int active;

/**********************************************************************/
/*   Create a new graph object.					      */
/**********************************************************************/
graph_t *
graph_new()
{	graph_t *g = (graph_t *) chk_zalloc(sizeof *g);

	active = TRUE;

	g->g_buf = chk_alloc(1024);
	g->g_size = 1024;

	g->g_coords_size = 16;
	g->g_coords_used = 0;
	g->g_coords = (coords_t *) chk_zalloc(g->g_coords_size * sizeof(coords_t));

	g->g_vars_size = 16;
	g->g_vars_used = 0;
	g->g_vars = (var_t *) chk_zalloc(g->g_vars_size * sizeof(var_t));

	g->g_x = 10;
	g->g_y = 10;
	g->g_width = 200;
	g->g_height = 200;
	g->g_color = 0xff9030;
	g->g_background_color = 0x401010;
	return g;
}

void
graph_add(graph_t *g, double x, double y)
{
	if (g->g_coords_used + 1 >= g->g_coords_size) {
		g->g_coords_size += 200;
		g->g_coords = (coords_t *) chk_realloc(g->g_coords, g->g_coords_size * sizeof(coords_t));
		}
	g->g_coords[g->g_coords_used].x = x;
	g->g_coords[g->g_coords_used].y = y;
	g->g_coords_used++;
}

void
graph_finish()
{
	if (active) {
		write(1, "\033[1925m", 7);
		active = FALSE;
		}
}
/**********************************************************************/
/*   Free the graph object.					      */
/**********************************************************************/
void
graph_free(graph_t *g)
{	int	i;

	chk_free(g->g_buf);
	chk_free(g->g_coords);
	for (i = 0; i < g->g_vars_used; i++) {
		chk_free(g->g_vars[i].v_name);
		}
	chk_free(g->g_vars);
	chk_free(g);
}

void
graph_addstr(graph_t *g, char *str)
{	int	len;

	if (g->g_autoflush) {
		printf("%s", str);
		return;
		}

	len = strlen(str);
	if (g->g_used + len + 1 >= g->g_size) {
		g->g_size += len + 104;
		g->g_buf = chk_realloc(g->g_buf, g->g_size);
		}
	memcpy(g->g_buf + g->g_used, str, len + 1);
	g->g_used += len;
}
void
graph_clear(graph_t *g)
{
	g->g_coords_used = 0;
//	g->g_vars_used = 0;
	g->g_used = 0;
}

void
graph_clear_chain(graph_t *g)
{
	graph_addstr(g, "\033[1924m");
}

void
graph_clear_chain_refresh(graph_t *g)
{
	graph_addstr(g, "\033[1925m");
}

void
graph_drawarc(graph_t *g, int x, int y, int width, int height, int arc1, int arc2)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1928;%d;%d;%d;%d;%d;%dm", x, y, width, height, arc1, arc2);
	graph_addstr(g, buf);
}

void
graph_drawline(graph_t *g, int x1, int y1, int x2, int y2)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1922;%d;%d;%d;%dm", x1, y1, x2, y2);
	graph_addstr(g, buf);
}

void
graph_drawpixel(graph_t *g, int x, int y)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1923;%d;%dm", x, y);
	graph_addstr(g, buf);
}

void
graph_drawimagestring(graph_t *g, int x, int y, char *text)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1933;%d;%d;\007%s\033m", x, y, text);
	graph_addstr(g, buf);
}

void
graph_drawrectangle(graph_t *g, int x, int y, int width, int height)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1920;%d;%d;%d;%dm", x, y, width, height);
	graph_addstr(g, buf);
}

void
graph_drawstring(graph_t *g, int x, int y, char *text)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1927;%d;%d;\007%s\033m", x, y, text);
	graph_addstr(g, buf);
}

void
graph_fillarc(graph_t *g, int x, int y, int width, int height, int arc1, int arc2)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1929;%d;%d;%d;%d;%d;%dm", x, y, width, height, arc1, arc2);
	graph_addstr(g, buf);
}

void
graph_fillrectangle(graph_t *g, int x, int y, int width, int height)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1921;%d;%d;%d;%dm", x, y, width, height);
	graph_addstr(g, buf);
}

/**********************************************************************/
/*   Find a variable in the graph settings.			      */
/**********************************************************************/
var_t *
graph_lookup(graph_t *g, char *name)
{	int	i;

	for (i = 0; i < g->g_vars_used; i++) {
		var_t *vp = &g->g_vars[i];
		if (strcmp(vp->v_name, name) == 0)
			return vp;
		}
	return NULL;
}

void
graph_plot(graph_t *g)
{	float	max_x = 0;
	float	min_y;
	float	max_y = 0;
	int	x0;
	int	y0;
	int	max_ent;
	coords_t *cop = g->g_coords;
	int	free_cop = FALSE;
	int	cop_size = g->g_coords_used;
	int	i;
	int	x, y, width, height;
	char	*name;

	/***********************************************/
	/*   Dont play with too many values.	       */
	/***********************************************/
	max_ent = graph_value(g, "last") || graph_value(g, "width");
	if (max_ent && max_ent > cop_size) {
		cop = cop + cop_size - max_ent;
		cop_size = max_ent;
	}

        /***********************************************/
        /*   Handle  graphs  which  have accumulating  */
        /*   totals - split out into differences.      */
        /***********************************************/
	if (graph_value(g, "delta") && cop_size > 1) {
		coords_t *new_cop = chk_zalloc(sizeof *new_cop * cop_size);
		for (i = 1; i < cop_size; i++) {
			new_cop[i-1].x = cop[i].x; 
			new_cop[i-1].y = cop[i].y - cop[i-1].y;
			}
		cop = new_cop;
		cop_size--;
		free_cop = TRUE;
		}

	/***********************************************/
	/*   Compute max/min Y			       */
	/***********************************************/
	min_y = 0;
	if (graph_value(g, "draw_minmax"))
		min_y = cop[0].y;
	max_y = cop[0].y;
	for (i = 0; i < cop_size; i++) {
		if (cop[i].y < min_y)
			min_y = cop[i].y;
		if (cop[i].y > max_y)
			max_y = cop[i].y;
		}

	graph_set_float(g, "min_y", min_y);
	graph_set_float(g, "max_y", max_y);

	if (graph_value(g, "background_color"))
		graph_setforeground(g, graph_value(g, "background_color"));

	graph_fillrectangle(g, 
		graph_value(g, "x"),
		graph_value(g, "y"),
		graph_value(g, "width"),
		graph_value(g, "height"));

	/***********************************************/
	/*   Now draw the graph.		       */
	/***********************************************/
	graph_setforeground(g, graph_value(g, "color"));
	x = x0 = graph_value(g, "x");
	y = y0 = graph_value(g, "y");
	width = graph_value(g, "width");
	height = graph_value(g, "height");
	if (max_y != min_y) {
		int	j;
		for (j = 0; j < cop_size; j++) {
			int	y1;

			int x1 = x + ((float) j / cop_size) * width;
			y1 = y + height - 
				(cop[j].y - min_y) / 
				((float) max_y - min_y) * height;
//printf("%d, %d, %d, %d min=%f max=%f\n", x1, y1+height, x1, y1, min_y, max_y);
			graph_drawline(g, x1, y + height, x1, y1);

			x0 = x1;
			y0 = y1;
			}
		}

	/***********************************************/
	/*   Draw box if requested.		       */
	/***********************************************/
	if (graph_value(g, "box")) {
		graph_setforeground(g, 0x40c0ff);
		graph_drawrectangle(g, x - 2, y - 2,
			width + 4, height +4);
		}

	/***********************************************/
	/*   For  mouse  clicking,  we  need  to know  */
	/*   which graph it was.		       */
	/***********************************************/
	name = graph_str_value(g, "realname");
	if (*name == '\0')
		name = graph_str_value(g, "name");
	if (*name == '\0')
		name = graph_str_value(g, "legend");
	if (*name == '\0')
		name = "noname";
	graph_setname(g, x - 2, y - 2, width + 4, height + 4, name);

	/***********************************************/
	/*   Draw legend details.		       */
	/***********************************************/
	if (graph_value(g, "legend")) {
		int	lines = 5;
		char	buf[BUFSIZ];
		
		graph_setforeground(g, 0xffffff);
		graph_setbackground(g, 0xffffff);
		graph_setfont(g, "6x9");
		graph_drawstring(g, x - 2, y + height + 16, graph_str_value(g, "legend"));
		
		graph_setforeground(g, 0x90c0c0);
		if ((graph_str_value(g, "legendflags"), "yminmax") != NULL) {
			snprintf(buf, sizeof buf, "%g", min_y);
			graph_drawstring(g, x + width + 6, y + height, buf);
			snprintf(buf, sizeof buf, "%g", max_y);
			graph_drawstring(g, x + width + 6, y, buf);
			}
		else if (min_y != max_y) {
			for (i = 1; i < lines; i++) {
				float n;
				graph_drawline(g, x, 
					y + height / lines * i,
					x + width, 
					y + height / lines * i);
				if (min_y > 50) {
					n = min_y + (int) ((max_y - min_y) * i / lines);
					snprintf(buf, sizeof buf, "%g", n);
					}
				else {
					snprintf(buf, sizeof buf, "%g", n);
					if (strlen(buf) > 5) 
						snprintf(buf, sizeof buf, "%g", 
							min_y + (int) ((max_y - min_y) * i / lines));
					}
				graph_drawstring(g, x + width + 6, y + height / lines * (lines - i), buf);
				}
			}
		/***********************************************/
		/*   Bottom ticks.			       */
		/***********************************************/
		if (width > 300) {
			for (i = 1; i < lines; i++) {
				float y = cop[i / lines * cop_size].y;
				snprintf(buf, sizeof buf, "%g", y);
				graph_drawline(g, 
					x + width * i / lines,
					y + height - 4,
					x + width * i / lines,
					y + height + 4);
				graph_drawstring(g, 
					x + width * i / lines,
					y + height + 13,
					buf);
				}
			}
		}

	if (free_cop) {
		chk_free(cop);
		}
}

void
graph_refresh(graph_t *g)
{
	fflush(stdout);
	out_strn(g->g_buf, g->g_used);
	out_flush();

	g->g_used = 0;
}

void
graph_reset(graph_t *g)
{
	g->g_coords_used = 0;
	g->g_vars_used = 0;
}
/**********************************************************************/
/*   Set overrides on the graph.				      */
/**********************************************************************/
var_t *
graph__set(graph_t *g, char *name)
{	var_t	*vp;

	if ((vp = graph_lookup(g, name)) == NULL) {
		if (g->g_vars_used + 1 >= g->g_vars_size) {
			g->g_vars_size += 16;
			g->g_vars = (var_t *) chk_realloc(g->g_vars,
				g->g_vars_size * sizeof(var_t));
			}
		vp = &g->g_vars[g->g_vars_used++];
		vp->v_name = chk_strdup(name);
		}
	return vp;
}
void
graph_set(graph_t *g, char *name, int val)
{	var_t	*vp = graph__set(g, name);

	vp->v_value = val;
}
void
graph_set_float(graph_t *g, char *name, double val)
{	var_t	*vp = graph__set(g, name);

	vp->v_float_value = val;
}

void
graph_setbackground(graph_t *g, int color)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1930;%dm", color);
	graph_addstr(g, buf);
}

void
graph_setforeground(graph_t *g, int color)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1931;%dm", color);
	graph_addstr(g, buf);
}

void
graph_setfont(graph_t *g, char *font)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1932;\007%s\033m", font);
	graph_addstr(g, buf);
}
void
graph_setname(graph_t *g, int x, int y, int width, int height, char *name)
{	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "\033[1926;%d;%d;%d;%d;\007%s\033m", x, y, width, height, name);
	graph_addstr(g, buf);
}
double
graph_float_value(graph_t *g, char *name)
{	var_t *vp = graph__set(g, name);

	return vp->v_float_value;
}
char *
graph_str_value(graph_t *g, char *name)
{	var_t *vp = graph__set(g, name);

	return vp->v_str_value ? vp->v_str_value : "";
}
int
graph_value(graph_t *g, char *name)
{	var_t	*vp = graph_lookup(g, name);

	if (vp)
		return vp->v_value;
	if (strcmp(name, "x") == 0)
		return g->g_x;
	if (strcmp(name, "y") == 0)
		return g->g_y;
	if (strcmp(name, "width") == 0)
		return g->g_width;
	if (strcmp(name, "height") == 0)
		return g->g_height;
	if (strcmp(name, "color") == 0)
		return g->g_color;
	if (strcmp(name, "background_color") == 0)
		return g->g_background_color;
	return 0;
}
void
graph_windowsize(graph_t *g, int *width, int *height)
{
	static struct winsize	winsize;

	ioctl(0, TIOCGWINSZ, &winsize);

	*width = winsize.ws_xpixel;
	*height = winsize.ws_ypixel;
}

