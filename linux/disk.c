# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static int diskstats_idx = -1;
static int width;

typedef struct disk_t {
	char	d_name[16];
	unsigned long long d_reads;
	unsigned long long d_rdmrg;
	unsigned long long d_rdsec;
	unsigned long long d_msread;
	unsigned long long d_writes;
	unsigned long long d_wrmrg;
	unsigned long long d_wrsec;
	unsigned long long d_mswrite;
	unsigned long long d_curios;
	} disk_t;

void
display_disk()
{	disk_t *np;
	int	i, j;
	int	n = 0;
	int	row = 0;
	int	col = 0;
	graph_t	*g;

	if (diskstats_idx < 0)
		diskstats_idx = mon_find("diskstats", &width);

	g = graph_new();
	graph_clear_chain(g);
	graph_refresh(g);
	
	print("            rd  rdmrg rdsect  ms/rd     wr  wrmrg wrsect  ms/wr    #io   msio  wtio");
	for (i = diskstats_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;
		
		if (name == NULL || strncmp(name, "diskstats.", 10) != 0)
			break;

		col++;
		/***********************************************/
		/*   If entire row is zero, then ignore it.    */
		/***********************************************/
		name = strchr(name, '.') + 1;
		if ((dot = strchr(name, '.')) == NULL) {
			int	ok = 0;
			int	j;
			for (j = i; ; j++) {
				char	*name2 = mon_name(j);
				if (name2 == NULL || strncmp(name2, "diskstats.", 10) != 0)
					break;
				name2 = strchr(name2, '.') + 1;
				if (strncmp(name2, name, strlen(name)) != 0)
					break;
				if (mon_get_item(j, 0))
					ok++;
				}
			if (!ok) {
				i = j - 1;
				continue;
				}
			goto_rc(5 + row + 2, 1);
			set_attribute(GREEN, BLACK, 0);
			print("%-*s", width - 10, name);
			row += 2;
			col = 0;
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - diskstats_idx, 6, 0, v, v0);

		/***********************************************/
		/*   Draw graph.			       */
		/***********************************************/
		graph_clear(g);
		graph_setfont(g, "6x9");
		graph_setforeground(g, 0xffffff);
		graph_setbackground(g, 0x80ffff);

		graph_set(g, "x", (width - 10) * 7 + col * 7 * 7);
		graph_set(g, "y", (row + 5) * 13);
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

/*
1 rd     -- # of reads issued
2 rdmrg  -- # of reads merged
3 rdsect -- # of sectors read
4 ms/rd  -- # of milliseconds spent reading
5 wr     -- # of writes completed
6 wrmrg  -- # of writes merged
7 wrsect -- # of sectors written
8 ms/wr  -- # of milliseconds spent writing
9 #io    -- # of I/Os currently in progress
10 msio  -- # of milliseconds spent doing I/Os
11 mswio -- weighted # of milliseconds spent doing I/Os 
*/
	print("\n");
	clear_to_end_of_screen();
}
