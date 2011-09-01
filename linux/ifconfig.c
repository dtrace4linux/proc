# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int ifconfig_idx = -1;
static int width;

void
display_ifconfig()
{	int	i, j;
	int	row = 0;
	int	col = 0;
	int	n = 0;
	graph_t	*g;

	if (ifconfig_idx < 0) {
		ifconfig_idx = mon_find("ifconfig", &width);
		width -= 10;
		}

	g = graph_new();
	graph_clear_chain(g);
	graph_refresh(g);

	print("%*s RX bytes    pkts    errs   drops    fifo   frame   compr   mcast\n", width, "");
	print("%*s TX bytes    pkts    errs   drops    fifo   colls   carr    compr\n", width, "");
	for (i = ifconfig_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;
		
		if (name == NULL || strncmp(name, "ifconfig.", 8) != 0)
			break;

		name = strchr(name, '.') + 1;
		if ((dot = strchr(name, '.')) == NULL) {
			goto_rc(6 + row + 2, 1);
			set_attribute(GREEN, BLACK, 0);
			print("%-*s RX", width, name);
			row += 2;
			col = 1;
			}
		else if (col++ == 8) {
			print("\n\n%*s TX", width, "");
			row += 2;
			col = 1;
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - ifconfig_idx, 6, 0, v, v0);
		print(" ");

		/***********************************************/
		/*   Draw graph.			       */
		/***********************************************/
		graph_clear(g);
		graph_setfont(g, "6x9");
		graph_setforeground(g, 0xffffff);
		graph_setbackground(g, 0x80ffff);

		graph_set(g, "x", (width + 3 + (col-1) * 8) * 7);
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
	graph_free(g);
	print("\n");
	clear_to_end_of_screen();
}
