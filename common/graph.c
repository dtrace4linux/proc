typedef struct graph_t {
	unsigned long g_color;
	int		g_count;
	int		g_max;
	unsigned long 	*g_values;
	} graph_t;

graph_t *
graph_new()
{	graph_t	*gp = chk_zalloc(sizeof(graph_t));

	return gp;
}
void
graph_draw(graph_t *gp, int x, int y, int w, int h)
{
}
void
graph_add_value(graph_t *gp, unsigned long val)
{
	if (gp->g_count + 1 >= gp->g_max) {
		if (gp->g_count == 0)
			gp->g_values = chk_alloc(sizeof *gp->g_values);
		else
			gp->g_values = chk_alloc(sizeof *gp->g_values, (gp->g_count + 1) * sizeof *gp->g_values);
		gp->g_count++;
		}
	gp->g_values[gp->g_count-1] = val;
}
