typedef struct var_t {
	char	*v_name;
	int	v_value;
	double	v_float_value;
	char	*v_str_value;
	} var_t;

typedef struct coords_t {
	float	x, y;
	} coords_t;

typedef struct graph_t {
	int	g_autoflush;
	char	*g_buf;
	int	g_size;
	int	g_used;

	coords_t *g_coords;
	int	g_coords_size;
	int	g_coords_used;

	var_t	*g_vars;
	int	g_vars_size;
	int	g_vars_used;

	int	g_x;
	int	g_y;
	int	g_width;
	int	g_height;
	int	g_color;
	int	g_background_color;
	} graph_t;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
graph_t *graph_new(void);
void graph_add(graph_t *g, double x, double y);
void graph_reset(graph_t *g);
int	graph_value(graph_t *, char *);
char	*graph_str_value(graph_t *g, char *name);
double graph_float_value(graph_t *g, char *name);
void graph_set(graph_t *g, char *name, int val);
void graph_set_float(graph_t *g, char *name, double val);
void graph_setfont(graph_t *g, char *font);
void graph_setbackground(graph_t *g, int color);
void graph_setforeground(graph_t *g, int color);
void graph_setname(graph_t *g, int x, int y, int width, int height, char *name);
void graph_free(graph_t *g);

