/**********************************************************************/
/*                                                                    */
/*  File:          cpu.c                                              */
/*  Author:        P. D. Fox                                          */
/*  Created:       29 Jul 2011                                        */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  CPU statistics                                      */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 29-Jul-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

extern char *cpu_names[];
extern char	cpu_info[256];
extern int	num_cpus;
extern const int Hertz;

/**********************************************************************/
/*   Display cpu stats.						      */
/**********************************************************************/
void
display_cpu()
{	int	cpu, j;
	unsigned long long tot_new, tot_old, tot;
	int	idx = 0;
	time_t	t;
	char	buf[BUFSIZ];

	t = (mon_get("time") - mon_get_rel("time", -1)) * Hertz;

	print("%s\n", cpu_info);
	print("      ");
	for (j = 0; j < MAX_CPU_TIMES; j++) {
		print("  %5s", cpu_names[j]);
	}
	print("\n");

	for (cpu = 0; cpu < num_cpus; cpu++) {
		unsigned long long v, v0;

		set_attribute(GREEN, BLACK, 0);
		print("CPU%-2d ", cpu);
		for (j = 0, tot_new = 0, tot_old = 0; j < MAX_CPU_TIMES; j++) {
			if (j == 0)
				snprintf(buf, sizeof buf, "stat.cpu%d", cpu);
			else
				snprintf(buf, sizeof buf, "stat.cpu%d.%02d", cpu, j + 1);
			tot_new += mon_get_rel(buf, 0);
			tot_old += mon_get_rel(buf, -1);
			}
		tot = tot_new == tot_old ? 1 : tot_new - tot_old;
		for (j = 0; j < MAX_CPU_TIMES; j++) {
			if (j == 0)
				snprintf(buf, sizeof buf, "stat.cpu%d", cpu);
			else
				snprintf(buf, sizeof buf, "stat.cpu%d.%02d", cpu, j + 1);

			v = mon_get_rel(buf, 0);
			v0 = mon_get_rel(buf, -1);

			print(" ");
			print__number(idx, 5, 1, 1, v, v0, tot);
			idx++;
			}
		print("\n");
		}


	print("\n");
	print("           MHz    Cache  Bogomips\n");
	for (cpu = 0; cpu < num_cpus; cpu++) {
		set_attribute(GREEN, BLACK, 0);
		print("CPU%-2d %8s %8s %8s\n", 
			cpu, 
			cpuinfo[cpu].cpu_mhz,
			cpuinfo[cpu].cache_size,
			cpuinfo[cpu].bogomips
			);
		}
	clear_to_end_of_screen();
}

