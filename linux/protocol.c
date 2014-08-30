/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          protocol.c                                         */
/*  Author:        P. D. Fox                                          */
/*  Created:       6 Sep 2011                                         */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  ip/udp/tcp protocol stats                           */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 06-Sep-2011 1.1 $ */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

static graph_t *g;
static int row;
static int width;

display_common_start()
{
	g = graphs_new();
	row = 0;
}
void
display_common(char *title, char *sname, int *idxp)
{
	int	i;
	int	slen = strlen(sname);

	if (*idxp < 0)
		*idxp = mon_find(sname, &width);

	print("         %s\n", title);
	row++;

	if (*idxp < 0) {
		print("No data available for this protocol screen.\n");
		clear_to_end_of_screen();
		return;
		}

	for (i = *idxp; ; i++) {
		char *name = mon_name(i);
		unsigned long long v, v0;
		if (name == NULL || strncmp(name, sname, slen) != 0 || name[slen] != '.')
			break;

		v = mon_get_item(i, 0);
		if (v == 0)
			continue;

		v0 = mon_get_item(i, -1);

		set_attribute(GREEN, BLACK, 0);
		print("%-*s ", width + 1, name + slen + 1);
		print_number(i - *idxp, 14, 1, v, v0);
		if (v != v0) {
			print("  %s%lld", v > v0 ? "+" : "", v - v0);
			}
		print("\n");

		graph_set(g, "draw_minmax", 1);
//		graph_set(g, "delta", 1);
		graph_set(g, "color", 0xc08020);
		draw_graph(g, 0x02, mon_name(i), 
			400, 13 * (row++ + 5), 150, 11, 1., 0x1000000);
		refresh();
		graph_refresh(g);
	}
}
void
display_common_end()
{
	clear_to_end_of_screen();
	graph_free(g);
}

void
display_IP()
{
static int idx = -1;
static int idx1 = -1;
static int idx2 = -1;
static int idx3 = -1;

	display_common_start();
	display_common("IP Statistics", "Ip", &idx);
	display_common("IP Ext Statistics", "IpExt", &idx1);
	display_common("ICMP Statistics", "Icmp", &idx2);
	display_common("ICMP Msg Statistics", "IcmpMsg", &idx3);
	display_common_end();
}
void
display_ICMP()
{
	display_IP();
}
void
display_TCP()
{
static int idx = -1;
static int idx1 = -1;

	display_common_start();
	display_common("TCP Statistics", "Tcp", &idx);
	display_common("TCP Ext Statistics", "TcpExt", &idx1);
	display_common_end();
}
void
display_UDP()
{
static int idx = -1;
static int idx1 = -1;

	display_common_start();
	display_common("UDP Statistics", "Udp", &idx);
	display_common("UDP Lite Statistics", "UdpLite", &idx1);
	display_common_end();
}

