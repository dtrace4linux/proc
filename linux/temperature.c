/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          temperature.c                                      */
/*  Author:        P. D. Fox                                          */
/*  Created:       7 Oct 2020                                         */
/*                                                                    */
/*  Copyright (c) 2020, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Check CPU temperatures                              */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 07-Oct-2020 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

# define	MAX_CPUS	256

static int temp_idx = -1;
static int width;
extern int num_cpus;

void
display_temperature()
{	int	i;
	struct stat sbuf;
	char	buf[BUFSIZ];
	char	*dn = "/sys/class/hwmon/hwmon0";
	char	*cp, *cp2, *cp3, *cp4;
	int	row = 0;
	unsigned long long v, v1, v2;
	graph_t	*g = graphs_new();

	if (temp_idx < 0)
		temp_idx = mon_find("temperature", &width);

	for (i = 1; ; i++) {
		snprintf(buf, sizeof buf, "temperature.cpu%d.t", i);
		if (!mon_exists(buf)) {
			break;
			}
		v = mon_get(buf);
		snprintf(buf, sizeof buf, "temperature.cpu%d.tmax", i);
		v1 = mon_get(buf);
		snprintf(buf, sizeof buf, "temperature.cpu%d.tcrit", i);
		v2 = mon_get(buf);

		snprintf(buf, sizeof buf, "%s/temp%d_label", dn, i);
		cp = read_file2(buf, NULL);
		if (cp) {
			cp[strlen(cp)-1] = '\0';
			}
		else
			cp = chk_strdup("hello");

		
		print("%-12s : %5.1f C  (high = %5.1f C, crit = %5.1f C)\n", 
			cp ? cp : "no-label",
			v / 1000., 
			v1 / 1000., 
			v2 / 1000.);
		chk_free(cp);

		graph_set(g, "draw_minmax", 1);
//		graph_set(g, "delta", 1);
		graph_set(g, "color", 0xc08020);

		int flags = 0;
		snprintf(buf, sizeof buf, "temperature.cpu%d.t", i);
		draw_graph(g, flags, buf,
			400, 13 * (row++ + 5), 150, 11, 1., 0x1000000);
		refresh();
		graph_refresh(g);

		}
	print("\n");
	clear_to_end_of_screen();
	graph_free(g);
}
