# include	"psys.h"
# include	"screen.h"

/*extern int  sys_nerr;
extern char *sys_errlist[];
*/
/**********************************************************************/
/*   Commarise  a  number  so  that  we  get  the commas every three  */
/*   digits so its easier to read large numbers.		      */
/**********************************************************************/
char *
comma(unsigned long n)
{
	static char bufs[80];
	static int index = 0;
	char	*buf;
	char	buf1[32];
	char	*cp1, *cp;
	int	i;

	index += 20;
	if (index >= sizeof bufs)
		index = 0;
	buf = &bufs[index];
	sprintf(buf, "%lu", n);
	cp1 = buf1;
	for (i = 0, cp = buf + strlen(buf) - 1; cp >= buf; ) {
		if (i++ >= 3) {
			*cp1++ = ',';
			i = 1;
			}
		*cp1++ = *cp--;
		}
	cp = buf;
	while (cp1 > buf1)
		*cp++ = *--cp1;
	*cp = '\0';
	return buf;
}
/**********************************************************************/
/*   Display error message on command line.			      */
/**********************************************************************/
void
error(char *str)
{
	mvprint(6, 0, "");
	print("%s\n", str);
	refresh();
}
/**********************************************************************/
/*   Map a string into a number.				      */
/**********************************************************************/
int
map_to_int(struct map *tbl, char *str)
{	static char buf[12];

	struct map *mp;
	
	for (mp = tbl; mp->name; mp++)
		if (mp->name[0] == *str && strcmp(mp->name, str) == 0)
			return mp->number;
	return -1;
}
/**********************************************************************/
/*   Display the error from a previous system call on command line.   */
/**********************************************************************/
void
sys_error(char *str)
{	int	err = errno;

	mvprint(6, 0, "");
#if 0
	if (errno >= 0 && errno <= sys_nerr)
		print("%s: %s\n", str, sys_errlist[err]);
	else
		print("%s: Error %d\n", str, err);
#else
	print("%s: %s\n", strerror(err));
#endif
	refresh();
}
void
wait_for_key()
{	char	ch;

	clear_to_end_of_screen();
	refresh();
	print("\nHit any key to continue:");
	refresh();
	while (read(0, &ch, 1) != 1)
		;
}

