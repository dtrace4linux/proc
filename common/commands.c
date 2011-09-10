/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          commands.c                                         */
/*  Author:        P. D. Fox                                          */
/*  Created:       25 Jan 2005                                        */
/*                                                                    */
/*  Copyright (c) 1995-2011, Foxtrot Systems Ltd                      */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Generic command handler for 'proc'                  */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 06-Sep-2011 1.7 $ 			      */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	"sigdesc.h"
# include	"../include/build.h"

FILE	*log_fp;

char	switches[26];
int	flush_content = -1;
int	proc_view;

int	mode = MODE_ABS;
int	mode_clear;

extern int auto_hilite;
extern int first_display;
static int old_display_mode;
extern char	**global_argv;

static void	abs_func(int argc, char **argv);
static void	agg_func(int argc, char **argv);
static void	clear_func(int argc, char **argv);
static void	count_func(int argc, char **argv);
static void	delay_func(int argc, char **argv);
static void	delta_func(int argc, char **argv);
static void	files_func(int argc, char **argv);
static void	flush_func(int argc, char **argv);
static void	grep_func(int argc, char **argv);
static void	help_func(int argc, char **argv);
static void	hide_func(int argc, char **argv);
static void	kill_func(int argc, char **argv);
static void	log_func(int argc, char **argv);
static void	netstat_func(int argc, char **argv);
static void	order_func(int argc, char **argv);
static void	proc_func(int argc, char **argv);
static void	quit_func(int argc, char **argv);
static void	range_func(int argc, char **argv);
static void	set_func(int argc, char **argv);
static void	show_func(int argc, char **argv);
static void	snap_func(int argc, char **argv);
static void	thread_func(int argc, char **argv);
static void	units_func(int argc, char **argv);
void	shell_command();

struct commands {
	char	*c_command;
	void	(*c_func)();
	int	c_mode;
	char	*c_help;
	} commands[] = {
	{"abs",		abs_func,	-1,	"Show absolute values."},
	{"agg",		agg_func,	-1,     "Aggregate thread cpu into parent."},
	{"clear",	clear_func,	-1,     "Clear counter values."},
	{"count",	count_func,	-1,     "Start counting mode."},
	{"cpu",		NULL,		DISPLAY_CPU,     "Show CPU details."},
	{"delay",	delay_func,	-1,     "Set refresh delay."},
	{"delta",	delta_func,	-1,     "Show delta values."},
	{"disk",	NULL,		DISPLAY_DISK,     "Show disk statistics.."},
	{"files",	files_func,	-1,     "Show open file information"},
	{"flush",	flush_func,	-1,     "Flush open file from disk cache"},
	{"graph",	NULL,		DISPLAY_GRAPHS,     "Show common graphs"},
	{"grep",	grep_func,	-1,     "Filter process output"},
	{"help",	help_func,	-1,     "Display help"},
	{"hide",	hide_func,	-1,     "Hide a column"},
	{"icmp",	NULL,		DISPLAY_ICMP,	"Show ICMP stats"},
	{"ifconfig",	NULL,		DISPLAY_IFCONFIG,     "Show network interface stats"},
	{"ip",		NULL,		DISPLAY_IP,	"Show IP interface stats"},
	{"interrupts",	NULL,		DISPLAY_INTERRUPTS,	"Show IRQ stats"},
	{"log",		log_func,	-1,     "[on | off] Enable/disable logging"},
	{"meminfo",	NULL,		DISPLAY_MEMINFO,     "Memory info"},
	{"netstat",	netstat_func,	-1,     "Show socket connections"},
	{"order",	order_func,	-1,     "[[-]nprstuc] Normal; Proc; RSS; Size; Time; User; CPU"},
	{"proc",	proc_func,	-1,     "Show specific proc"},
	{"ps",		NULL,		DISPLAY_PS,     "Display process mode"},
	{"quit",	quit_func,	-1,     "Exit from tool"},
	{"range",	range_func,	-1,     "Show numbers in units/ranges"},
	{"set",		set_func,	-1,     "Set options/limits."},
	{"show",	show_func,	-1,     "Show a hidden column"},
	{"snap",	snap_func,	-1,     "Dump snapshots to file to see history"},
	{"softirqs",	NULL,		DISPLAY_SOFTIRQS,     "Show /proc/softirqs"},
	{"tcp",		NULL,		DISPLAY_TCP,     "Show TCP stats"},
	{"thread",	thread_func,	-1,     "Toggle thread mode"},
	{"udp",		NULL,		DISPLAY_UDP,     "Show UDP stats"},
	{"units",	units_func,	-1,     "Show units on numbers"},
	{"vmstat",	NULL,		DISPLAY_VMSTAT,     "Show vmstats"},
	{"?",		help_func,	-1,     "Display help"},
	{"!",		shell_command,	-1,     "Run a unix command repeatedly"},
	{0,		NULL,		-1,     (char *) NULL}
	};

char	*grep_filter;
extern int proc_id;
int	delay_time = 5;

char	*chk_strdup(char *);

static void
command_switches(int argc, char **argv)
{	int	i;

	memset(switches, 0, sizeof switches);
	for (i = 1; i < argc; i++) {
		char *cp = argv[i];
		if (*cp++ != '-')
			continue;
		while (isalpha(*cp)) {
			switches[toupper(*cp) - '@'] = 1;
			cp++;
			}
		}
}

static void
display_error(char *str, ...)
{	char	ch;
	char	buf[1024];
	va_list pvar;

	va_start(pvar, str);
	vsnprintf(buf, sizeof buf - 1, str, pvar);
	va_end(pvar);
	mvprint(6, 0, "%s\n", buf);
	print("\nPress any key to continue: ");
	refresh();
	if (read(1, &ch, 1) != 0) {
		}
}

static void
press_to_continue()
{	char	ch;

	print("\nPress any key to continue: ");
	refresh();
	if (read(1, &ch, 1) != 1) {
		}
}

int
process_command(char *str)
{	char	*argv[128];
	int	argc;
	char	*cp;
	struct commands *cmdp;

	if (*str == '!') {
		shell_command(str + 1);
		return 0;
		}

	old_display_mode = display_mode;
/*	display_mode = DISPLAY_PS;*/
	argc = 0;
	argv[0] = NULL;
	for (cp = strtok(str, " "); cp; cp = strtok((char *) NULL, " ")) {
		argv[argc++] = cp;
		}

	if (argv[0] == NULL)
		return 0;

	for (cmdp = commands; cmdp->c_command; cmdp++) {
		if (strncasecmp(argv[0], cmdp->c_command, strlen(argv[0])) == 0) {
			if (cmdp->c_mode != -1)
				display_mode = cmdp->c_mode;
			else
				(*cmdp->c_func)(argc, argv);
			break;
			}
		}
	return 0;
}

static void
show_columns()
{	int	i;

	mvprint(6, 0, "Columns you can hide:\n\n");
	for (i = 0; columns[i].name; i++) {
		if (columns[i].flags & COL_HIDDEN)
			print("hidden  %s\n", columns[i].name);
		else
			print("        %s\n", columns[i].name);
		}
 	press_to_continue();
}

static void
abs_func(int argc, char **argv)
{
	mode = MODE_ABS;
}

static void
clear_func(int argc, char **argv)
{
	mode = MODE_COUNT;
	mode_clear = TRUE;
}

static void
count_func(int argc, char **argv)
{
	mode = MODE_COUNT;
}

static void
delta_func(int argc, char **argv)
{
	mode = MODE_DELTA;
}

static void
agg_func(int argc, char **argv)
{
	if (argc != 2) {
		agg_mode = !agg_mode;
		}
	else
		agg_mode = atoi(argv[1]);
	display_error(agg_mode ? "Aggregate mode: on" : "Aggregate mode: off");
}
static void
delay_func(int argc, char **argv)
{
	if (argc != 2) {
		display_error("Usage: delay nn");
		return;
		}
	delay_time = atoi(argv[1]);
	if (delay_time < 0)
		delay_time = 1;
}
static void
files_func(int argc, char **argv)
{
	display_mode = DISPLAY_FILES;
	chk_free_ptr((void **) &grep_filter);
	if (argc > 1) {
		grep_filter = chk_strdup(argv[1]);
		}
}
static void
flush_func(int argc, char **argv)
{
	if (old_display_mode != DISPLAY_FILES)
		return;
	display_mode = DISPLAY_FILES;
	if (argc != 2) {
		display_error("Usage: flush <fileno>");
		return;
		}
	flush_content = atoi(argv[1]);
}
static void
grep_func(int argc, char **argv)
{
	if (argc != 2) {
		chk_free_ptr((void **) &grep_filter);
		return;
		}
	grep_filter = chk_strdup(argv[1]);
}
static struct columns *
find_column(char *name)
{	int	j;

	for (j = 0; columns[j].name; j++) {
		char	*cp = columns[j].name;
		while (*cp == ' ')
			cp++;
		if (strncasecmp(name, cp, strlen(name)) == 0) {
			return &columns[j];
			}
		}
	return NULL;
}
static void
help_func(int argc, char **argv)
{	char	ch;
	struct commands *cmdp;

	if (argc > 1 && strcmp(argv[1], "all") == 0) {
		char	buf[BUFSIZ];
		char	*path;
		char	*cp;
		struct stat sbuf;

		path = realpath(global_argv[0], NULL);
		if ((cp = strrchr(path, '/')) != NULL) {
			*cp = '\0';
			}
		clear_screen();
		refresh();
		if (stat("/bin/more", &sbuf) == 0)
			snprintf(buf, sizeof buf, "more %s/../README", path);
		else
			snprintf(buf, sizeof buf, "less %s/../README", path);
		system(buf);
		clear_screen();
		free(path);
		return;
	}
	mvprint(6, 0, "proc: real-time process display v%d.%0d-%d\n",
		MAJ_VERSION, MIN_VERSION, version_build_no);
	print("\n");
	print("Type 'help all' to see detailed help.\n");
	print("\n");
	print("Modes:\n");
	print("  Aggregate mode: %s\n", agg_mode ? "on" : "off");
	print("  Display mode:   %s\n", 
		mode == MODE_DELTA ? "delta" :
		mode == MODE_COUNT ? "count" :
			"abs");
	print("  Process view:   %s\n", display_mode == DISPLAY_PROC ? "process" : "thread");
	print("\nThe following commands are available:\n");
	for (cmdp = commands; cmdp->c_command; cmdp++) {
		print("%-10s %s\n",
			cmdp->c_command,
			cmdp->c_help ? cmdp->c_help : "");
		}
	print("\n");
	print("Ctrl-B - page/back; Ctrl-F - page/fwd; Ctrl-R - reset paging\n");
	print("Ctrl-L - refresh screen.\n");
	print("\nPress any key to continue: ");
	refresh();
	if (read(1, &ch, 1) != 0) {
		}
}
/**********************************************************************/
/*   Hide columns from view.					      */
/**********************************************************************/
static void
hide_func(int argc, char **argv)
{	int	i;

	if (argc == 1) {
		show_columns();
		return;
	}

	for (i = 1; i < argc; i++) {
		char	*name = argv[i];
		struct columns *col = find_column(name);
		if (col == NULL)
			break;
		col->flags |= COL_HIDDEN;
		}
	if (i >= argc)
		return;

	mvprint(6, 0, "Unknown column name %s\n", argv[i]);
	press_to_continue();
}
static void
kill_func(int argc, char **argv)
{	int	sig = SIGTERM;
	char	*cp;
	int	i, j;
	struct map	*mp;
	char	*str;

	str = argc > 1 ? argv[1] : "";

	if (*str == '\0' || strcmp(str, "-l") == 0) {
		mvprint(6, 0, "The following signals are available:\n");
		for (i = 0, mp = sigdesc; mp->name; mp++, i++) {
			if ((i % 4) == 0)
				print("\n");
			print(" %s:", mp->name);
			for (j = strlen(mp->name); j < 8; j++)
				print(" ");
			print("%2d   ", mp->number);
			}
		print("\n");
		wait_for_key();
		return;
		}

	if (*str == '-') {
		cp = strtok(str, " ");
		cp++;
		if (isdigit(*cp))
			sig = atoi(cp);
		else 
			sig = map_to_int(sigdesc, cp);
		if (sig <= 0 || sig >= NSIG) {
			error("Bad signal number.");
			return;
			}
		str = (char *) NULL;
		}

	for (i = 2; i < argc; i++) {
		j = atoi(argv[i]);
		if (j <= 0)
			continue;
		if (kill(j, sig) < 0) {
			sys_error("kill");
			break;
			}
		}
}
# if CR_SOLARIS2_INTEL || CR_SOLARIS2_SPARC
static void
kstat_func(argc, argv)
int	argc;
char	**argv;
{	char	*str;

	if (argc > 1) {
		str = argv[1];
		if (*str == '+')
			kstat_level++;
		else if (*str == '-')
			kstat_level--;
		else if (isdigit(*str))
			kstat_level = atoi(str);
		else
			kstat_mask = chk_strdup(str);
		}
	else {
		kstat_level = 0;
		if (kstat_mask)
			chk_free(kstat_mask);
		kstat_mask = (char *) NULL;
		}
	display_mode = DISPLAY_KSTAT;
}
# endif
static void
log_func(int argc, char **argv)
{
	if (argc != 2) {
		display_error("Usage: log [on | off]");
		return;
		}
	if (strcmp(argv[1], "on") == 0) {
		char	buf[PATH_MAX];
		int	i;
		struct stat sbuf;

		for (i = 0; i < 999; i++) {
			sprintf(buf, "/tmp/proc.%03d.log", i);
			if (stat(buf, &sbuf) < 0)
				break;
			}
		log_fp = fopen(buf, "w");
		return;
		}
	if (strcmp(argv[1], "off") == 0) {
		fclose(log_fp);
		log_fp = NULL;
		return;
		}
	display_error("Usage: log [on | off]");
}
static void
netstat_func(int argc, char **argv)
{
	display_mode = DISPLAY_NETSTAT;
	command_switches(argc, argv);
}
static void
order_func(int argc, char **argv)
{	char	*tmp = "npustrc";
	int	ch;
	char	*str;

	if (argc <= 1)
		return;

	str = argv[1];
	if (*str == '-') {
		sort_order = -1;
		str++;
		}
	else
		sort_order = 1;
	ch = *str;
	if (ch >= 'A' && ch <= 'Z')
		ch += 0x20;
	if (ch && strchr(tmp, ch) != NULL)
		sort_type = strchr(tmp, ch) - tmp;
}
static void
proc_func(int argc, char **argv)
{
	if (argc <= 1)
		return;

	proc_id = atoi(argv[1]);
	display_mode = DISPLAY_PROC;
}
static void
quit_func(int argc, char **argv)
{
	quit_flag = TRUE;
}

static void
range_func(int argc, char **argv)
{
	mode = MODE_RANGED;
}

/**********************************************************************/
/*   Set/display various options.				      */
/**********************************************************************/
static void
set_func(int argc, char **argv)
{
	if (argc <= 1)
		return;
}
void
shell_command(char *cmd)
{	FILE	*fp;
	char	lbuf[BUFSIZ];
	int	i;
	extern	int	rows;
	int	first_cmd = cmd != (char *) NULL;
static char	last_command[BUFSIZ];

	signal(SIGCLD, SIG_IGN);

	if (cmd == (char *) NULL)
		cmd = last_command;
	else
		strcpy(last_command, cmd);

	mvprint(6, 0, "");
	fp = popen(cmd, "r");
	if (fp == (FILE *) NULL) {
		clear_to_end_of_screen();
		return;
		}
	if (!first_cmd)
		auto_hilite++;
	for (i = 7; !feof(fp) && i < rows; i++) {
		if (fgets(lbuf, sizeof lbuf, fp) == NULL)
			break;
		print("%s", lbuf);
		}
	if (!first_cmd)
		auto_hilite--;
	clear_to_end_of_screen();
	fclose(fp);
	display_mode = DISPLAY_CMD;
}
/**********************************************************************/
/*   Show columns						      */
/**********************************************************************/
static void
show_func(int argc, char **argv)
{	int	i;

	if (argc == 1) {
		show_columns();
		return;
	}

	for (i = 1; i < argc; i++) {
		char	*name = argv[i];
		struct columns *col = find_column(name);
		if (col == NULL)
			break;
		col->flags &= ~COL_HIDDEN;
		}
	if (i >= argc)
		return;

	mvprint(6, 0, "Unknown column name %s\n", argv[i]);
	press_to_continue();
}
static void
snap_func(int argc, char **argv)
{	char	buf[PATH_MAX];
	int	i;
	struct stat sbuf;
	FILE	*fp;

	for (i = 0; i < 999; i++) {
		sprintf(buf, "/tmp/proc.snap.%03d.log", i);
		if (stat(buf, &sbuf) < 0)
			break;
		}
	fp = fopen(buf, "w");
	snap_to_file(fp);
	fclose(fp);
	display_error("Snapped to /tmp/proc.snap.%03d.log", i);
}
/**********************************************************************/
/*   Toggle thread display.					      */
/**********************************************************************/
static void
thread_func(int argc, char **argv)
{
	proc_view = !proc_view;
	first_display = FALSE;

	mvprint(6, 0, "%s view mode enabled\n",
		proc_view ? "Process" : "Thread");
}
static void
units_func(int argc, char **argv)
{
	mode = MODE_RANGED;
}
