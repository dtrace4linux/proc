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
	int	i;
	int	n = 0;

	if (diskstats_idx < 0)
		diskstats_idx = mon_find("diskstats", &width);

	print("            rd  rdmrg rdsect  ms/rd     wr  wrmrg wrsect  ms/wr    #io   msio  wtio");
	for (i = diskstats_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;
		
		if (name == NULL || strncmp(name, "diskstats.", 10) != 0)
			break;

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
			print("\n");
			set_attribute(GREEN, BLACK, 0);
			print("%-*s", width - 10, name);
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - diskstats_idx, 6, 0, v, v0);
	}
	print("\n");

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
	clear_to_end_of_screen();
}
