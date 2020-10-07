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

static int softirqs_idx = -1;
static int width;
extern int num_cpus;

void
display_temperature()
{	int	i;
	struct stat sbuf;
	char	buf[BUFSIZ];
	char	*dn = "/sys/class/hwmon/hwmon0";
	char	*cp, *cp2, *cp3, *cp4;

	if (stat(dn, &sbuf) < 0) {
		print("%s not available on this machine.\n", dn);
		clear_to_end_of_screen();
		return;
		}

	for (i = 0; i < num_cpus; i++) {
		float t;
		float t1;
		float t2;
		float t3;

		snprintf(buf, sizeof buf, "%s/temp%d_input", dn, i + 1);
		if ((cp = read_file(buf, NULL)) == NULL)
			break;
		if (strncmp(cp, " <non", 5) == 0)
			break;

		snprintf(buf, sizeof buf, "%s/temp%d_label", dn, i + 1);
		if ((cp2 = read_file(buf, NULL)) == NULL)
			break;
		cp2[strlen(cp2)-1] = '\0';

		snprintf(buf, sizeof buf, "%s/temp%d_max", dn, i + 1);
		if ((cp3 = read_file(buf, NULL)) == NULL)
			break;
		cp3[strlen(cp3)-1] = '\0';

		snprintf(buf, sizeof buf, "%s/temp%d_crit", dn, i + 1);
		if ((cp4 = read_file(buf, NULL)) == NULL)
			break;
		cp4[strlen(cp4)-1] = '\0';

		t = atof(cp) / 1000.;
		t1 = atof(cp3) / 1000.;
		t2 = atof(cp4) / 1000.;
		print("%-12s : %5.1f C  (high = %5.1f C, crit = %5.1f C)\n", cp2, i, t, t1, t2);

		chk_free(cp);
		chk_free(cp2);
		chk_free(cp3);
		chk_free(cp4);
		}
	print("\n");
	clear_to_end_of_screen();
}
