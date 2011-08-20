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

	print("                 rd rdmrg rdsec ms/rd    wr wrmrg wrsec ms/wr");
	for (i = diskstats_idx; ; i++) {
		char *name = mon_name(i);
		char	*dot;
		unsigned long long v, v0;
		
		if (name == NULL || strncmp(name, "diskstats.", 10) != 0)
			break;

		name = strchr(name, '.') + 1;
		if ((dot = strchr(name, '.')) == NULL) {
			print("\n");
			set_attribute(GREEN, BLACK, 0);
			print("%-*s ", width - 10, name);
			print("idx=%d ", i);
			}

		v = mon_get_item(i, 0);
		v0 = mon_get_item(i, -1);

		print_number(i - diskstats_idx, 7, 0, v, v0);
	}
	print("\n");
	clear_to_end_of_screen();
}
