/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          monitor.c                                          */
/*  Author:        P. D. Fox                                          */
/*  Created:       29 Jul 2011                                        */
/*                                                                    */
/*  Copyright (c) 2011, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Code to implement shared memory access to /proc entries. */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 27-Aug-2011 1.2 $ 			      */
/**********************************************************************/

/**********************************************************************/
/*   Code to implement a memory mapped background status monitor.     */
/*    								      */
/*   Author: Paul Fox						      */
/*   Date: July 2011						      */
/*   $Header:$							      */
/**********************************************************************/

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<../foxlib/hash.h>
# include	<sys/mman.h>
# include	<dirent.h>
# include	<assert.h>
# include	<ctype.h>

# define	VERSION		10
# define	MAX_SYMS	3000
# define	MAX_TICKS	1024

int	proc_zombie;
int	proc_running;
int	proc_stopped;
int	mon_num_procs;
int	mon_num_threads;
int	too_small;
int	noexit;
int	mon_snap;
int	mon_snap2;
int	num_procs;

static char mon_fname[256];
static ino_t inode;
static int old_pos;
hash_t	*hash_syms;
static unsigned long max_syms = MAX_SYMS;
static unsigned long max_ticks = MAX_TICKS;

typedef struct mdir_t {
	int		md_version;
	int		md_revision;
	int		md_pid;
	time_t		md_mtime;
	time_t		md_readers;
	unsigned long	md_count;
	unsigned long	md_max_syms;
	unsigned long	md_max_ticks;
	unsigned long	md_first;
	} mdir_t;

typedef struct mdir_entry_t {
	char		me_name[60];
	unsigned long	me_offset;
	} mdir_entry_t;
typedef unsigned long long tick_t;

/**********************************************************************/
/*   Pointer to the shm area.					      */
/**********************************************************************/
void	*mmap_addr;
size_t	mmap_size;
mdir_t	*mdir;

# define MDIR_ENTRY(base, offset) (mdir_entry_t *) ((char *) base + offset)
# define MDIR_VALUES(base, ep) ((tick_t *) ((char *) base + ep->me_offset))
# define entry(n) \
	(mdir_entry_t *) ((char *) mmap_addr + sizeof(mdir_t) + n * sizeof(mdir_entry_t))

static int is_monitor;
extern struct timeval last_tv;
int	have_task_dir;

int	killmon;
int	debugmon;
int	mon_pos = -1;

#if defined(MAIN)
char **global_argv;
#else
extern char **global_argv;
extern int first_display;
extern struct timeval last_tv;
#endif

#define TY_ONE_PER_LINE		0x001
#define	TY_ONE_PER_LINE_UNITS	0x002
#define	TY_NO_LABEL		0x004
#define	TY_IGNORE_HEADER	0x008
#define	TY_DISKSTATS		0x010
#define	TY_IGNORE_HEADER2       0x020
#define	TY_NETSTAT		0x040
#define	TY_INTERRUPTS		0x080

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
static void usage(void);
static void mon_item(char *fname, char *name, int type);
static void mon_set(char *vname, tick_t v);
void monitor_list(int);
char	*basename(char *);
static void mon_rehash(void);
void mon_netstat(void);
void mon_procs(void);
void monitor_read(void);
void monitor_init(void);
void	reset_terminal(void);
static void read_proc_status(char *name, int pid, procinfo_t *pip);

#if defined(MAIN)
void do_switches(int argc, char **argv);

int main(int argc, char **argv)
{
	do_switches(argc, argv);

	is_monitor = TRUE;
	monitor_list(argc > 1 ? atoi(argv[1]) : -1);
	return 0;
}
void
do_switches(int argc, char **argv)
{
	int	i;
	char	*cp;

	for (i = 1; i < argc; i++) {
		cp = argv[i];
		if (strcmp(cp, "noexit") == 0) {
			noexit = TRUE;
			continue;
			}
		usage();
		}
}
static void
usage()
{
	fprintf(stderr, "procmon -- background data collector for 'proc'\n");
	fprintf(stderr, "\n");
	exit(1);
}

void
reset_terminal()
{
	exit(1);
}
#endif

void
main_procmon()
{	mdir_t	old_mdir;
	struct timeval tval;
	struct stat sbuf;
	struct stat sbuf1;

	monitor_init();

	snprintf(mon_fname, sizeof mon_fname, "%s/proc.mmap", mon_dir());
	stat(mon_fname, &sbuf);

	mdir->md_pid = getpid();
	old_mdir = *mdir;
	if (getenv("PROCMON_SPAWNED_BY_PROC") == NULL)
		noexit = 1;

	time_str();
	printf("%s procmon, pid=%d\n", time_str(), getpid());
	while (1) {
		if (mdir->md_version != old_mdir.md_version)
			break;
		if (mdir->md_revision != old_mdir.md_revision)
			break;
		if (mdir->md_pid != old_mdir.md_pid)
			break;

		/***********************************************/
		/*   If nobody is watching, then give up.      */
		/***********************************************/
		if (mdir->md_readers && !noexit &&
		    time(NULL) > mdir->md_readers + 20 * 60) {
		    	printf("%s no more readers, and have been idle for 20m. Exiting\n", time_str());
		    	break;
			}

		/***********************************************/
		/*   Watch out for someone deleting the file.  */
		/***********************************************/
		if (stat(mon_fname, &sbuf1) < 0)
			break;
		if (sbuf.st_ino != sbuf1.st_ino)
			break;

		if (debugmon) {
			printf("Tick, pid=%d first=%ld\n", getpid(), mdir->md_first);
			}

		mon_snap2 = (mdir->md_first + 1) % max_ticks;
		gettimeofday(&tval, NULL);
		mon_set("time", (tick_t) tval.tv_sec * 100 + tval.tv_usec / 10000);

		printf("%s getting data %d\n", time_str(), mdir->md_first);
		monitor_read();

		mdir->md_mtime = time(NULL);
		mdir->md_first = mon_snap2;

		sleep(1);
	}
}

static int
mon_sort(const void *p1, const void *p2)
{	const mdir_entry_t *e1 = p1;
	const mdir_entry_t *e2 = p2;

	return strcmp(e1->me_name, e2->me_name);
}

void
monitor_init()
{
	int	i, fd;
	int	pid;
	int	size;
	int	created = FALSE;
	char	*cp;
	mdir_t	hdr;
	static int first_time = TRUE;
	struct stat sbuf;

	if (mmap_addr)
		return;

	if (first_time) {
		if ((cp = getenv("PROC_MAX_SYMS")) != NULL)
			max_syms = atoi(cp);
		if ((cp = getenv("PROC_MAX_TICKS")) != NULL)
			max_ticks = atoi(cp);
		first_time = FALSE;
		}

	size = sizeof(mdir_t) + max_syms * sizeof(mdir_entry_t) + 
			max_syms * max_ticks * sizeof(tick_t);

	snprintf(mon_fname, sizeof mon_fname, "%s", mon_dir());
	mkdir(mon_fname, 0777);

	hash_syms = hash_create(128, 128);
	strcat(mon_fname, "/proc.mmap");

	if ((fd = open(mon_fname, O_RDWR)) < 0) {
		char buf[1024];
		if (is_monitor) {
			printf("No monitor file available (%s)\n", mon_fname);
			exit(1);
			}
		if ((fd = open(mon_fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
			perror(mon_fname);
			exit(1);
		}
		chmod(mon_fname, 0666);
		created = TRUE;
		memset(buf, 0, sizeof buf);
		for (i = 0; i < size; i += sizeof buf) {
			if (write(fd, buf, sizeof buf) != sizeof buf) {
				perror("write");
				exit(1);
				}
			}
		}
	else {
		if (read(fd, &hdr, sizeof hdr) != sizeof hdr) {
			printf("Error reading header - please retry\n");
			exit(1);
			}
		if (hdr.md_max_syms)
			max_syms = hdr.md_max_syms;
		if (hdr.md_max_ticks)
			max_ticks = hdr.md_max_ticks;
		size = sizeof(mdir_t) + max_syms * sizeof(mdir_entry_t) + 
				max_syms * max_ticks * sizeof(tick_t);
		}
	stat(mon_fname, &sbuf);
	if (size > sbuf.st_size) {
		printf("procmon: %s corrupt size\n", mon_fname);
		printf("  expected %d\n", size);
		printf("  actual   %lu\n", sbuf.st_size);
		printf("  hdr.max_syms =%d\n", hdr.md_max_syms);
		printf("  hdr.max_ticks=%d\n", hdr.md_max_ticks);
		unlink(mon_fname);
		exit(1);
		}

	mmap_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	mmap_size = size;
	close(fd);

	inode = sbuf.st_ino;

	mdir = mmap_addr;
	pid = mdir->md_pid;

	if (is_monitor)
		return;

	if (mdir->md_version != VERSION) {
		int rev = mdir->md_revision++;
		if (pid)
			kill(pid, SIGTERM);
		memset(mdir, 0, size);
		mdir->md_version = VERSION;
		mdir->md_revision = rev;
		mdir->md_max_syms = max_syms;
		mdir->md_max_ticks = max_ticks;
		mdir->md_pid = 0;
		}

	/***********************************************/
	/*   Check if monitor is dead.		       */
	/***********************************************/
	if (pid && kill(pid, 0) < 0)
		mdir->md_pid = 0;

	if (created) {
		monitor_read();

		qsort(mdir + 1, mdir->md_count, sizeof(mdir_entry_t), mon_sort);
		}

	mon_rehash();
}

/**********************************************************************/
/*   Dump the mmap in human readable format.			      */
/**********************************************************************/
void
monitor_list(int entry)
{	char	buf[BUFSIZ];
	int	i, j;
	struct stat sbuf;

	monitor_init();
	printf("Version : %d\n", mdir->md_version);
	printf("Revision: %d\n", mdir->md_revision);
	printf("PID     : %d", mdir->md_pid);
	snprintf(buf, sizeof buf, "/proc/%d", mdir->md_pid);
	if (mdir->md_pid == 0)
		printf(" not running\n");
	else if (kill(mdir->md_pid, 0) < 0)
		printf(" (non-existant)\n");
	else if (stat(buf, &sbuf) < 0)
		printf(" %s entry missing\n", buf);
	else {
		printf("\n");
		snprintf(buf, sizeof buf, "ps -fp %d", mdir->md_pid);
		if (system(buf) != 0) {
			}
		}
	printf("Ticks   : %lu\n", mdir->md_max_ticks);
	printf("Count   : %lu\n", mdir->md_count);
	printf("Max syms: %lu\n", mdir->md_max_syms);

	printf("      Offset First Value\n");
	for (i = 0; i <= (int) mdir->md_count; i++) {
		mdir_entry_t	*ep = (mdir_entry_t *) ((char *) (mdir + 1) + i * sizeof(mdir_entry_t));
		tick_t *tp = (tick_t *) ((char *) mdir + ep->me_offset + 
			mdir->md_first * sizeof(tick_t));
		if (entry >= 0 && i != entry)
			continue;
		printf("%4d: %06lx %8llu %s\n", 
			i, ep->me_offset, *tp, ep->me_name);
		if (entry < 0)
			continue;
		for (j = 0; j < (int) max_ticks; j++) {
			tick_t *tp = (tick_t *) ((char *) mdir + ep->me_offset + j * sizeof(tick_t));
			printf("    %04d: %8llu %s\n", j, *tp, 
				(int) mdir->md_first == j ? " (first)" : "");
			}
		}
}
/**********************************************************************/
/*   One  loop  for  procmon. If the proc.mmap file is too small, we  */
/*   discard it and restart with a bigger size.			      */
/**********************************************************************/
void
monitor_read()
{

	while (1) {
		monitor_init();
		mon_item("/proc/stat", 	    NULL,	TY_ONE_PER_LINE);
		mon_item("/proc/vmstat",    NULL,	TY_ONE_PER_LINE);
		mon_item("/proc/meminfo",   NULL,	TY_ONE_PER_LINE_UNITS);
		mon_item("/proc/loadavg",   NULL,	TY_NO_LABEL);
		mon_item("/proc/softirqs",  NULL,	TY_IGNORE_HEADER);
		mon_item("/proc/diskstats", NULL,	TY_DISKSTATS);
		mon_item("/proc/net/dev",   "ifconfig", TY_IGNORE_HEADER | TY_IGNORE_HEADER2);
		mon_item("/proc/net/netstat","proto",	TY_NETSTAT);
		mon_item("/proc/net/snmp",   "proto",	TY_NETSTAT);
		mon_item("/proc/interrupts",   "irq",	TY_INTERRUPTS);
		mon_netstat();
		mon_procs();

		if (!too_small)
			break;

		printf("Symbol table too small max_syms=%lu, needed=%lu .. resizing\n", max_syms, mdir->md_count);

		max_syms = mdir->md_count + 128;
		monitor_uninit();
		sleep(1);
		}
}
void
monitor_start()
{	char	buf[PATH_MAX];
	char	buf2[PATH_MAX];
	int	ret;

	monitor_init();

	if (mdir->md_pid && (killmon || debugmon)) {
		kill(mdir->md_pid, SIGTERM);
		mdir->md_pid = 0;
		}
	
	signal(SIGCHLD, SIG_IGN);
	if (mdir->md_pid != 0 || (!debugmon && fork() != 0)) {
		int	cnt;
		sleep(1);
		for (cnt = 0; mon_exists("time") == FALSE && cnt < 1000; cnt++) {
			mon_rehash();
			if (mon_exists("time"))
				break;
			printf("waiting for mon data...\n");
			fflush(stdout);
			sleep(1);
			}
		if (cnt >= 1000) {
			printf("procmon is not running.\n");
			reset_terminal();
			exit(1);
			}
		return;
		}

	/***********************************************/
	/*   Detach from the parent.		       */
	/***********************************************/
	if (fork()) {
		exit(0);
	}

	snprintf(buf, sizeof buf, "/proc/%d/exe", getpid());
	ret = readlink(buf, buf2, sizeof buf2);
	if (ret < 0) {
		printf("Cannot realpath(%s)\n", global_argv[0]);
		printf("Cannot execute procmon for you.\n");
		exit(1);
		}
	buf2[ret] = '\0';
	snprintf(buf, sizeof buf, "%smon", buf2);
	global_argv[0] = buf;
	/***********************************************/
	/*   Close  stdout  since  procmon  writes to  */
	/*   there.  We can use strace to detect what  */
	/*   is  going  on,  or  maybe  redirect to a  */
	/*   logfile.				       */
	/***********************************************/
	close(1);
	open("/dev/null", O_WRONLY);
	putenv("PROCMON_SPAWNED_BY_PROC=1");
	execve(buf, global_argv, NULL);
	printf("exeve %s failed\n", buf);
	exit(1);
}

void
monitor_uninit()
{
	munmap(mmap_addr, mmap_size);
	unlink(mon_fname);
	hash_clear(hash_syms, 0);
	hash_syms = NULL;
	mmap_addr = NULL;
	too_small = 0;
}

char *
mon_dir()
{	static char buf[128];

	snprintf(buf, sizeof buf, "%s/proc",
			getenv("TMPDIR") ? getenv("TMPDIR") : "/var/tmp");
	return buf;
}

int
mon_exists(char *name)
{	int	n;

	if ((n = (int) (long) hash_lookup(hash_syms, name)) == 0)
		return 0;
	return 1;
}
time_t
mon_is_stale()
{
	time_t t = time(NULL) - mdir->md_mtime;

	return t;
}
/**********************************************************************/
/*   Read a /proc entry and parse it away into the mmap.	      */
/**********************************************************************/
static void
mon_item(char *fname, char *lname, int type)
{	FILE	*fp;
	char	buf[4096];
	char	vname[BUFSIZ];
	char	*cp, *vp, *vp1;
	int	i, n;
	unsigned long long v;

	if ((fp = fopen(fname, "r")) == NULL)
		return;

	if (type & TY_INTERRUPTS) {
		if (fgets(buf, sizeof buf, fp) == NULL) {
			fclose(fp);
			return;
			}
		while (fgets(buf, sizeof buf, fp) != NULL) {
			char *title = strtok(buf, " :");
			for (i = 0; (cp = strtok(NULL, ": ")) != NULL; i++) {
				if (!isdigit(*cp))
					break;
				sscanf(cp, "%llu", &v);
				snprintf(vname, sizeof vname, "interrupts.%s.%d", title, i);
				mon_set(vname, v);
				}
			}
		fclose(fp);
		return;
		}

	/***********************************************/
	/*   Handle netstat/stats here.		       */
	/***********************************************/
	if (type & TY_NETSTAT) {
		char	tbuf[4096];
		char	*titles[256];
		while (fgets(tbuf, sizeof tbuf, fp)) {
			for (n = 0, cp = strtok(tbuf, ": \n"); cp; cp = strtok(NULL, ": \n")) {
				titles[n++] = cp;
				if (n + 1 >= (int) (sizeof titles / sizeof titles[0]))
					break;
				}
			if (fgets(buf, sizeof buf, fp) == NULL)
				break;
			cp = strtok(buf, ": ");
			for (i = 1; i < n; i++) {
				if ((cp = strtok(NULL, " \n")) == NULL)
					break;
				sscanf(cp, "%llu", &v);
				snprintf(vname, sizeof vname, "%s.%s", titles[0], titles[i]);
				mon_set(vname, v);
				}
			}
		fclose(fp);
		return;
		}

	if (type & TY_IGNORE_HEADER) {
		if (fgets(buf, sizeof buf, fp)) {
			}
		}
	if (type & TY_IGNORE_HEADER2) {
		if (fgets(buf, sizeof buf, fp)) {
			}
		}

	while (fgets(buf, sizeof buf, fp)) {
		int	len = strlen(buf);
		
		cp = buf;
		if (len && buf[len-1] == '\n')
			buf[--len] = '\0';
		if (lname)
			strcpy(vname, lname);
		else
			strcpy(vname, basename(fname));
		vp = vname + strlen(vname);
		while (isspace(*cp)) cp++;
		if (type & TY_DISKSTATS) {
			while (!isspace(*cp)) cp++;
			while (isspace(*cp)) cp++;
			while (!isspace(*cp)) cp++;
			while (isspace(*cp)) cp++;
			}
		if ((type & TY_NO_LABEL) == 0) {
			*vp++ = '.';
			while (!isspace(*cp)) {
				if (*cp == ':') {
					cp++;
					break;
					}
				*vp++ = *cp++;
				}
			}
		vp1 = vp;
		/***********************************************/
		/*   Now get the values.		       */
		/***********************************************/
		for (n = 0; *cp; ) {
			while (isspace(*cp) || *cp == '/') cp++;
			if (*cp == '\0')
				break;
			if (n++)
				sprintf(vp1, ".%02d", n);
			else
				*vp1 = '\0';
			v = atoll(cp);
			while (isdigit(*cp)) cp++;
			if (*cp == '.') {
				cp++;
				v = 100 * v + atoll(cp);
				while (isdigit(*cp)) cp++;
				}
			mon_set(vname, v);
			if (type & TY_ONE_PER_LINE_UNITS)
				break;
			}
		}
	fclose(fp);
}
static hash_element_t **harray;
int
mon_find(char *prefix, int *width)
{
	int	i, j, len;

	/**width = 0;*/

	if (harray == NULL)
		harray = hash_linear(hash_syms, HASH_SORTED);

	len = strlen(prefix);
	for (i = 0; i < hash_size(hash_syms); i++) {
		int	n = (int) (long) hash_data(harray[i]);
		mdir_entry_t *ep = entry(n);
		if (strncmp(ep->me_name, prefix, len) == 0 &&
		   ep->me_name[len] == '.') {
		   	for (j = i; j < hash_size(hash_syms); j++) {
				int n = (int) (long) hash_data(harray[j]);
				mdir_entry_t *ep = entry(n);
				int	w = strlen(ep->me_name);
				if (strncmp(ep->me_name, prefix, len) != 0)
					break;
				if (w > *width)
					*width = w;
				}
		   	return n;
			}
		}
	return -1;
}

unsigned long long
mon_get(char *name)
{	int	n;
	mdir_entry_t *ep;
	tick_t	*tp;

	if ((n = (int) (long) hash_lookup(hash_syms, name)) == 0)
		return -1;
	ep = entry(n);
	tp = (tick_t *) ((char *) mmap_addr + ep->me_offset);
	return tp[mon_pos < 0 ? mon_snap : mon_pos];
}

unsigned long long
mon_get_rel(char *name, int rel)
{	mdir_entry_t *ep;
	tick_t	*tp;
	int	n;

	if ((n = (int) (long) hash_lookup(hash_syms, name)) == 0)
		return -1;
	ep = entry(n);
	tp = (tick_t *) ((char *) mmap_addr + ep->me_offset);
	n = mon_pos < 0 ? mon_snap : mon_pos;
	n += rel;
	if (n < 0)
		n += max_ticks;
	return tp[n];
}

unsigned long long
mon_get_item(int n, int rel)
{	mdir_entry_t *ep;
	tick_t	*tp;

	ep = entry(n);
	tp = (tick_t *) ((char *) mmap_addr + ep->me_offset);
	n = mon_pos < 0 ? mon_snap : mon_pos;
	n += rel;
	if (n < 0)
		n += max_ticks;
	return tp[n];
}
/**********************************************************************/
/*   Get the current time, in 1/100th secs.			      */
/**********************************************************************/
unsigned long long
mon_get_time()
{	struct stat sbuf;

	if (stat(mon_fname, &sbuf) < 0 || sbuf.st_ino != inode) {
		return (unsigned long long) -1;
		}
	/***********************************************/
	/*   Check in case the monitor changed pid.    */
	/***********************************************/
	
	/***********************************************/
	/*   Let mon proc know someone is watching.    */
	/***********************************************/
	mdir->md_readers = time(NULL);
	return mon_get("time");
}

/**********************************************************************/
/*   Let caller know if we are playing back.			      */
/**********************************************************************/
int
mon_history()
{
	return old_pos >= 0;
}
/**********************************************************************/
/*   Lock the time slot so we dont get partial results for each call  */
/*   into monitor.c.						      */
/**********************************************************************/
void
mon_lock()
{
	mon_snap = mdir->md_first;
	old_pos = mon_pos;
	if (mon_pos < 0)
		mon_pos = mdir->md_first;
}
void
mon_unlock()
{
	mon_pos = old_pos;
}
/**********************************************************************/
/*   Move forward/back in the timeline history.			      */
/**********************************************************************/
void
mon_move(int arg)
{
	int	orig_mon = mon_pos;

	if (arg == 0) {
		mon_pos = -1;
		return;
		}

	/***********************************************/
	/*   Dont  let  us  go  too  far  forward  or  */
	/*   backward.				       */
	/***********************************************/
	if (arg == 1 && (mon_pos + 1) % (int) max_ticks == (int) mdir->md_first) {
		mon_pos = -1;
		return;
		}
	if (arg == -1 && (int) (mon_pos + max_ticks - 1) % (int) max_ticks == (int) mdir->md_first)
		return;

	if (mon_pos < 0 && arg == 1)
		return;
	if (mon_pos < 0)
		mon_pos = mdir->md_first;
	mon_pos += arg;
	if (mon_pos < 0)
		mon_pos = max_ticks - 1;
	else if (mon_pos >= (int) max_ticks)
		mon_pos = 0;

	/***********************************************/
	/*   Dont let us go back too far.	       */
	/***********************************************/
	if (mon_get("time") == 0)
		mon_pos = orig_mon;
}

char *
mon_name(int i)
{	mdir_entry_t *ep;

	ep = entry(i);
	return ep->me_name;
}

/**********************************************************************/
/*   Get delta netstat info.					      */
/**********************************************************************/
static socket_t *fd_list;
static int	fd_size;
static int	fd_used;

void
mon_netstat(void)
{
#if !defined(MAIN)
	int	i;
	socket_t *old_fd_list;
	int	old_used;
	FILE	*fp;
	FILE	*fp1;
	char	buf[BUFSIZ];
	char	buf1[BUFSIZ];
	char	buf2[BUFSIZ];

	snprintf(buf1, sizeof buf1, "%s/netstat.%04d.tmp", mon_dir(),
		(int) mon_snap2);
	if ((fp1 = fopen("/proc/net/tcp", "r")) == NULL)
		return;
	if ((fp = fopen(buf1, "w")) == NULL) {
		fclose(fp1);
		return;
		}
	chmod(buf, 0666);

	old_fd_list = fd_list;
	old_used = fd_used;

	fd_size = 512;
	fd_used = 0;
	fd_list = (socket_t *) chk_alloc(fd_size * sizeof *fd_list);

	while (fgets(buf, sizeof buf, fp1) != NULL) {
		socket_t *sp;
		int	ret;
		if (fd_used + 1 >= fd_size) {
			fd_size += 128;
			fd_list = (socket_t *) chk_realloc(fd_list, fd_size * sizeof *fd_list);
			}

		sp = &fd_list[fd_used++];
		sp->s_time = time(NULL);
		ret = sscanf(buf, " %*s %lx:%x %lx:%x %x %x:%x %*x:%*x %*x %d %*d %lu",
			&sp->l_ip, &sp->l_port,
			&sp->r_ip, &sp->r_port,
			&sp->state,
			&sp->sndwin,
			&sp->rcvwin,
			&sp->uid,
			&sp->inode);
		if (ret != 9) {
			fd_used--;
			continue;
			}
		for (i = 0; i < old_used; i++) {
			if (sp->r_ip == old_fd_list[i].r_ip &&
			    sp->r_port == old_fd_list[i].r_port &&
			    sp->l_ip == old_fd_list[i].l_ip &&
			    sp->l_port == old_fd_list[i].l_port) {
			    	sp->s_time = old_fd_list[i].s_time;
			    	sp->uid = old_fd_list[i].uid;
				break;
				}
			}
	}
	fwrite(fd_list, fd_used, sizeof *fd_list, fp);
	fclose(fp);
	fclose(fp1);
	strcpy(buf2, buf1);
	buf2[strlen(buf2)-4] = '\0';
	if (rename(buf1, buf2) != 0) {
		fprintf(stderr, "rename(%s, %s)\n", buf1, buf2);
		perror("rename");
		exit(1);
		}
	chk_free(old_fd_list);
#endif
}
/**********************************************************************/
/*   Get the process info and write to the shared files.	      */
/**********************************************************************/
void
mon_procs(void)
{
#if !defined(MAIN)
	procinfo_t	*pinfo;
	int	i, num;
	FILE	*fp = stdout;
	char	buf[BUFSIZ];
	char	buf1[BUFSIZ];

	snprintf(buf, sizeof buf, "%s/proc.%04d.tmp", mon_dir(),
		(int) mon_snap2);
	if ((fp = fopen(buf, "w")) == NULL)
		return;
	chmod(buf, 0666);

	pinfo = raw_proc_get_proclist(&num);
	for (i = 0; i < num; i++) {
		prpsinfo_t *p = &pinfo[i].pi_psinfo;
		p->pr_syscall = read_syscall(pinfo[i].pi_pid, p->pr_uid);

		fprintf(fp, "%d ", p->pr_pid);
		fprintf(fp, "%d ", pinfo[i].pi_flags);
		fprintf(fp, "%d ", p->pr_isnew);
		fprintf(fp, "%d ", p->pr_state);
		fprintf(fp, "%d ", p->pr_ppid);
		fprintf(fp, "%d ", p->pr_pgrp);
		fprintf(fp, "%d ", p->pr_session);
		fprintf(fp, "%d ", p->pr_tty);
		fprintf(fp, "%d ", p->pr_num_threads);
		fprintf(fp, "%d ", p->pr_tpgrp);
		fprintf(fp, "%d ", p->pr_flags);
		fprintf(fp, "%lu ", p->pr_min_flt);
		fprintf(fp, "%lu ", p->pr_cmin_flt);
		fprintf(fp, "%lu ", p->pr_maj_flt);
		fprintf(fp, "%lu ", p->pr_cmaj_flt);
		fprintf(fp, "%lu ", p->pr_utime);
		fprintf(fp, "%lu ", p->pr_stime);
		fprintf(fp, "%lu ", p->pr_cutime);
		fprintf(fp, "%lu ", p->pr_cstime);
		fprintf(fp, "%d ", p->pr_priority);
		fprintf(fp, "%d ", p->pr_nice);
		fprintf(fp, "%d ", p->pr_start_time);
		fprintf(fp, "%lu ", p->pr_pctcpu);
		fprintf(fp, "%lu ", p->pr_size);
		fprintf(fp, "%lu ", p->pr_vsize);
		fprintf(fp, "%lu ", p->pr_rssize);
		fprintf(fp, "%lu ", p->pr_rss_rlim);
		fprintf(fp, "%llu ", p->pr_wchan);
		fprintf(fp, "%u ", p->pr_lproc);
		fprintf(fp, "%u ", p->pr_uid);
		fprintf(fp, "%d ", p->pr_syscall);
		fprintf(fp, "\n");
		fprintf(fp, "%s\n", pinfo[i].pi_cmdline);
		}
	fclose(fp);
	strcpy(buf1, buf);
	buf1[strlen(buf1)-4] = '\0';
	rename(buf, buf1);
#endif
}
static unsigned long long
get_num(char **str)
{	char *cp = *str;
	unsigned long long v = 0;
	int	neg = 1;

	if (*cp == '-') {
		cp++;
		neg = -1;
		}

	while (isdigit(*cp))
		v = 10 * v + *cp++ - '0';
	while (*cp == ' ')
		cp++;
	*str = cp;
	return neg * v;
}

/**********************************************************************/
/*   Get the netstat data.					      */
/**********************************************************************/
int
mon_read_netstat(int rel, socket_t **tblp)
{	int	m;
	struct stat sbuf;
	char	buf[BUFSIZ];
	int	fd;
	socket_t *tbl;

	rel = rel;

	m = mon_pos < 0 ? mon_snap : mon_pos;
	snprintf(buf, sizeof buf, "%s/netstat.%04d", mon_dir(), m);
	if (stat(buf, &sbuf) < 0)
		return 0;

	tbl = (socket_t *) chk_alloc(sbuf.st_size);
	if ((fd = open(buf, O_RDONLY)) < 0)
		return 0;
	if (read(fd, tbl, sbuf.st_size) != sbuf.st_size) {
		chk_free(tbl);
		close(fd);
		return 0;
		}
	close(fd);

	*tblp = tbl;
	return sbuf.st_size / sizeof *tbl;
}
/**********************************************************************/
/*   Read the proc files.					      */
/**********************************************************************/
procinfo_t	*last_proc_array;
int	last_proc_count;
int	last_proc_size;
procinfo_t	*proc_array;
int	proc_count;
int	proc_size;

static int
last_sort_pid(const void *v1, const void *v2)
{	const procinfo_t *p1 = v1;
	const procinfo_t *p2 = v2;
	int	t1, t2;

	if (p1->pi_psinfo.pr_pid != p2->pi_psinfo.pr_pid)
		return p1->pi_psinfo.pr_pid - p2->pi_psinfo.pr_pid;
	t1 = p1->pi_flags & PI_IS_THREAD;
	t2 = p1->pi_flags & PI_IS_THREAD;
	return t1 - t2;
}
static int
last_sort_search(const void *v, const void *elem)
{	const procinfo_t	*pv = (procinfo_t *) v;
	const procinfo_t	*p = elem;

	if (pv->pi_psinfo.pr_pid != p->pi_psinfo.pr_pid)
		return pv->pi_psinfo.pr_pid - p->pi_psinfo.pr_pid;
//printf("last_sort_search: %p %x %p %x\n", pv, pv->pi_flags & PI_IS_THREAD, p, p->pi_flags & PI_IS_THREAD);
	return (pv->pi_flags & PI_IS_THREAD) - (p->pi_flags & PI_IS_THREAD);
}
#if !defined(MAIN)
int
mon_read_procs()
{	char	buf[4096];
	FILE	*fp;
	int	i, m, nproc;
	procinfo_t	*p;
	procinfo_t *lp;

	/***********************************************/
	/*   Swap  parray  and  last_proc_array so we can  */
	/*   avoid malloc/free.			       */
	/***********************************************/
	for (i = 0; i < last_proc_count; i++) {
		chk_free_ptr((void **) &last_proc_array[i].pi_cmdline);
		}
	for (i = 0; i < proc_count; i++) {
		chk_free_ptr((void **) &proc_array[i].pi_cmdline);
		}
# define SWAP(a, b, t) t = a; a = b; b = t

	SWAP(last_proc_array, proc_array, p);
	SWAP(last_proc_count, proc_count, i);
	SWAP(last_proc_size, proc_size, i);

	proc_running = proc_zombie = proc_stopped = 0;
	mon_num_procs = mon_num_threads = 0;

	m = mon_pos < 0 ? (int) mon_snap : mon_pos;
	snprintf(buf, sizeof buf, "%s/proc.%04d", mon_dir(), m);
	if ((fp = fopen(buf, "r")) == NULL) {
		return 0;
		}
	for (i = nproc = 0; fgets(buf, sizeof buf, fp); i++) {
		char	*cp = buf;
		prpsinfo_t *p;

		if (nproc + 1 >= proc_size) {
			if (proc_size == 0) {
				proc_size = 200;
				proc_array = chk_zalloc(proc_size * sizeof *proc_array);
				}
			else {
				int	delta = 250;
				proc_array = (procinfo_t *) chk_realloc(proc_array, 
					(proc_size + delta) * sizeof *proc_array);
				memset(proc_array + proc_size, 0, delta * sizeof *proc_array);
				proc_size += delta;
				}
			}
		p = &proc_array[nproc].pi_psinfo;
		proc_array[nproc].pi_pid = p->pr_pid = get_num(&cp);
		proc_array[nproc].pi_flags = get_num(&cp);
		p->pr_isnew = get_num(&cp);
		p->pr_state = get_num(&cp);
		p->pr_ppid = get_num(&cp);
		p->pr_pgrp = get_num(&cp);
		p->pr_session = get_num(&cp);
		p->pr_tty = get_num(&cp);
		p->pr_num_threads = get_num(&cp);
		p->pr_tpgrp = get_num(&cp);
		p->pr_flags = get_num(&cp);
		p->pr_min_flt = get_num(&cp);
		p->pr_cmin_flt = get_num(&cp);
		p->pr_maj_flt = get_num(&cp);
		p->pr_cmaj_flt = get_num(&cp);
		p->pr_utime = get_num(&cp);
		p->pr_stime = get_num(&cp);
		p->pr_cutime = get_num(&cp);
		p->pr_cstime = get_num(&cp);
		p->pr_priority = get_num(&cp);
		p->pr_nice = get_num(&cp);
		p->pr_start_time = get_num(&cp);
		p->pr_pctcpu = get_num(&cp);
		p->pr_size = get_num(&cp);
		p->pr_vsize = get_num(&cp);
		p->pr_rssize = get_num(&cp);
		p->pr_rss_rlim = get_num(&cp);
		p->pr_wchan = get_num(&cp);
		p->pr_lproc = get_num(&cp);
		p->pr_uid = get_num(&cp);
		p->pr_syscall = get_num(&cp);

		proc_array[nproc].pi_flags |= PI_PSINFO_VALID;
		if (proc_array[nproc].pi_flags & PI_IS_THREAD)
			mon_num_threads++;
		else
			mon_num_procs++;

		switch (p->pr_state) {
		  case 'R':
		  	proc_running++;
			break;
		  case 'T':
		  	proc_stopped++;
			break;
		  case 'Z':
		  	proc_zombie++;
			break;
		  }

		/***********************************************/
		/*   Cache the 'last' values.		       */
		/***********************************************/
		p->pr_last_size = p->pr_size;
		p->pr_last_vsize = p->pr_vsize;
		p->pr_last_rssize = p->pr_rssize;
		p->pr_last_utime = 0;
		p->pr_last_stime = 0;
		p->pr_isnew = first_display ? 0 : 4;

		lp = bsearch((void *) p,
			last_proc_array, last_proc_count, sizeof *last_proc_array,
			last_sort_search);
		if (lp) {
//assert((lp->pi_flags & PI_IS_THREAD) == (proc_array[nproc].pi_flags & PI_IS_THREAD));
			p->pr_last_size = lp->pi_psinfo.pr_size;
			p->pr_last_vsize = lp->pi_psinfo.pr_vsize;
			p->pr_last_rssize = lp->pi_psinfo.pr_rssize;

			p->pr_last_stime = lp->pi_psinfo.pr_stime;
			p->pr_last_utime = lp->pi_psinfo.pr_utime;
			p->pr_tlast = lp->pi_psinfo.pr_tnow;
			p->pr_isnew = lp->pi_psinfo.pr_isnew-1;
			if (p->pr_isnew < 0)
				p->pr_isnew = 0;
			}
		else {
			p->pr_tlast = last_tv;
			p->pr_last_vsize = p->pr_vsize;
			}

		/***********************************************/
		/*   Get command line.			       */
		/***********************************************/
		if (fgets(buf, sizeof buf, fp) == NULL)
			break;
		chk_free_ptr((void **) &proc_array[nproc].pi_cmdline);

		if (!proc_view && proc_array[nproc].pi_flags & PI_IS_THREAD)
			continue;

		buf[strlen(buf)-1] = '\0';
		proc_array[nproc].pi_cmdline = chk_strdup(buf);

		nproc++;
		}
	proc_count = nproc;

	fclose(fp);

	return proc_count;
}
#endif

/**********************************************************************/
/*   Rebuild the local hash table.				      */
/**********************************************************************/
static void
mon_rehash()
{	int	i;

	hash_clear(hash_syms, 0);

	for (i = 1; i <= (int) mdir->md_count; i++) {
		mdir_entry_t *ep = entry(i);
//printf("adding %2d %s\n", i, ep->me_name);
		if (hash_insert(hash_syms, ep->me_name, (void *) (long) i) == FALSE) {
			printf("hash-reinit failure, i=%d ep=%p (%s)\n", i, ep, ep->me_name);
			if (mon_fname[0])
				unlink(mon_fname);
			reset_terminal();
			exit(1);
			}
		}
}

static void
mon_set(char *vname, tick_t v)
{	unsigned long offset;
	mdir_entry_t *ep;
	tick_t	*tp;
	int	n;

	if ((n = (int) (long) hash_lookup(hash_syms, vname)) == 0) {
		n = ++mdir->md_count;
		if (mdir->md_count >= max_syms) {
			too_small++;
			return;
			}
		offset = sizeof(mdir_t) + 
			max_syms * sizeof(mdir_entry_t) +
			n * sizeof(tick_t) * max_ticks;
		ep = entry(n);
		strncpy(ep->me_name, vname, sizeof(ep->me_name));
		ep->me_offset = offset;
		if (hash_insert(hash_syms, ep->me_name, (void *) (long) n) == FALSE) {
			printf("Hash table too small or name clash?\n");
			}
		}
	ep = entry(n);
	tp = (tick_t *) ((char *) mmap_addr + ep->me_offset);
	tp[mon_snap2] = v;
}
int
mon_tell()
{
	return mdir->md_first;
}

/**********************************************************************/
/*   Read one subdir.						      */
/**********************************************************************/
int first_display = TRUE;

static int
proc_get_procdir(char *name, int is_proc, int *countp)
{	DIR	*dirp;
	struct dirent *de;
	int	count = *countp;
	prpsinfo_t *pip;
	procinfo_t *lp;
	struct stat sbuf;
	char	buf[BUFSIZ];
	int	num = 0;
	int	n;

	dirp = opendir(name);
	if (dirp == NULL)
		return 0;

	while ((de = readdir(dirp)) != NULL) {
		int	is_thread = FALSE;

		if (proc_view) {
			if (!isdigit(de->d_name[0]))
				continue;
		} else {
			if (isdigit(de->d_name[0]))
				;
 			else {
				if (de->d_name[0] != '.' || !isdigit(de->d_name[1]))
					continue;
				is_thread = TRUE;
				}
		}

		num++;
		/***********************************************/
		/*   Just attribute the sub-threads.	       */
		/***********************************************/
		if (proc_view && !is_proc)
			continue;
		/***********************************************/
		/*   Skip  top proc entry since we may go for  */
		/*   the underlying threads instead.	       */
		/***********************************************/
		if (have_task_dir && !proc_view && is_proc)
			goto do_thread;

		/***********************************************/
		/*   Watch  out  for  a  thread  matching the  */
		/*   parent pid.			       */
		/***********************************************/
		if (count + 1 >= proc_size) {
			proc_size += 400;
			proc_array = (procinfo_t *) chk_realloc((void *) proc_array,
				proc_size * sizeof(procinfo_t));
			}

		memset(&proc_array[count], 0, sizeof (procinfo_t));
		pip = &proc_array[count].pi_psinfo;
		proc_array[count].pi_flags |= PI_PSINFO_VALID;
		if (!is_proc) {
			proc_array[count].pi_flags |= PI_IS_THREAD;
			}

		strcpy(proc_array[count].pi_name, de->d_name);
		pip->pr_pid = proc_array[count].pi_pid = atoi(de->d_name[0] == '.' ? de->d_name+1 : de->d_name);
// performance/memory issue
		if (is_proc || 1) {
			snprintf(buf, sizeof buf, "%s/%s", name, de->d_name);
			proc_array[count].pi_cmdline = read_file(buf, "cmdline");
			}
		else 
			proc_array[count].pi_cmdline = chk_strdup("(xx)");
		gettimeofday(&pip->pr_tnow, 0);
/*if (strstr(proc_array[count].pi_cmdline, "proc") == 0) continue;*/
		if (stat(buf, &sbuf) >= 0)
			pip->pr_uid = sbuf.st_uid;
		else
			pip->pr_uid = -1;

		pip->pr_last_size = pip->pr_size;
		pip->pr_last_vsize = pip->pr_vsize;
		pip->pr_last_rssize = pip->pr_rssize;
		pip->pr_isnew = first_display ? 0 : 4;

		lp = bsearch((void *) &proc_array[count],
			last_proc_array, last_proc_count, sizeof *last_proc_array,
			last_sort_search);
		if (lp) {
//assert((lp->pi_flags & PI_IS_THREAD) == (pip->pr_flags & PI_IS_THREAD));
			pip->pr_last_size = lp->pi_psinfo.pr_size;
			pip->pr_last_vsize = lp->pi_psinfo.pr_vsize;
			pip->pr_last_rssize = lp->pi_psinfo.pr_rssize;

			pip->pr_last_stime = lp->pi_psinfo.pr_stime;
			pip->pr_last_utime = lp->pi_psinfo.pr_utime;
			pip->pr_tlast = lp->pi_psinfo.pr_tnow;
			pip->pr_isnew = lp->pi_psinfo.pr_isnew-1;
			pip->pr_pctcpu0 = lp->pi_psinfo.pr_pctcpu;
			if (pip->pr_isnew < 0)
				pip->pr_isnew = 0;
			}
		else {
			pip->pr_tlast = last_tv;
			pip->pr_last_vsize = pip->pr_vsize;
			}

		read_proc_status(name, pip->pr_pid, &proc_array[count]);
		count++;

		/***********************************************/
		/*   See if we have any child threads.	       */
		/***********************************************/
do_thread:
		if (!is_proc)
			continue;

		snprintf(buf, sizeof buf, "%s/%s/task", name, de->d_name);
		*countp = count;
		n = proc_get_procdir(buf, FALSE, countp);
		if (proc_view) {
			pip = &proc_array[count-1].pi_psinfo;
			pip->pr_num_threads = n;
			}
		count = *countp;

		}
	closedir(dirp);

	*countp = count;
	return num;
}

static void
read_proc_status(char *name, int pid, procinfo_t *pp)
{	char	*mem, *cp;
	char	buf[1024];
	char	cmd[1024], *bp;
	int	t;
	prpsinfo_t *pip = &pp->pi_psinfo;

	pp->pi_pid = pid;
	pip->pr_pid = pid;

	sprintf(buf, "%s/%d/stat", name, pid);
	mem = read_file(buf, (char *) NULL);

	cp = strchr(mem, '(');
	if (cp && (pp->pi_cmdline[0] == '\0' || pp->pi_cmdline[0] == ' ')) {
		cp++;
		bp = cmd;
		*bp++ = ' ';
		*bp++ = '(';
		while (*cp && *cp != ')')
			*bp++ = *cp++;
		*bp++ = ')';
		*bp = '\0';
		chk_free(pp->pi_cmdline);
		pp->pi_cmdline = chk_strdup(cmd);
		}
	cp = strrchr(mem, ')');
	if (cp) {
		cp++;
		while (isspace(*cp))
			cp++;
		sscanf(cp, 
			"%c "			/* 0 */
			"%d %d %d %d %d "	/* 1 ppid pgrp session tty tpgrp */
			"%lu %lu %lu %lu %lu %lu %lu " /* 6-12 flags min_flt cmin_flt maj_flt cmaj_flt utime stime */
			"%ld %ld %ld %ld %ld %ld " /* 13-18 cutime cstime pri nice timeout it_real_value */
			"%lu %llu " /* 19-20 start_time vsize */
			"%ld " /* 21 rssize */
		        "%lu %lu %lu %lu " 	/* 22-25 rsslim startcode endcode startstack */
			"%lu %lu " 		/* 26-27 kstkesp kstkeip */
			"%*s %*s %*s %*s " /* 28-31 signal blocked sigignore sigcatch */
			"%lu %llu %lu %*d %d " /* 32-36 wchan nswap cnswap exit_sig processor */
			"%u %u %llu %lu %ld" /* rt_prio, policy, delayacct_blkio_ticks, guest_time cguest_time */
			,
			&pip->pr_state,
			&pip->pr_ppid, &pip->pr_pgrp, &pip->pr_session, &pip->pr_tty, &pip->pr_tpgrp,
			&pip->pr_flags, 
				&pip->pr_min_flt, 
				&pip->pr_cmin_flt, 
				&pip->pr_maj_flt, 
				&pip->pr_cmaj_flt, 
				&pip->pr_utime, 
				&pip->pr_stime,
			&pip->pr_cutime, 
				&pip->pr_cstime, 
				&pip->pr_priority, 
				&pip->pr_nice, 
				&pip->pr_num_threads, 
				&pip->pr_it_real_value,
			&pip->pr_start_time, &pip->pr_vsize,
			&pip->pr_rssize,
			&pip->pr_rss_rlim, 
				&pip->pr_start_code, 
				&pip->pr_end_code, 
				&pip->pr_start_stack, 
				&pip->pr_kstk_esp, 
				&pip->pr_kstk_eip,
			&pip->pr_wchan, 
				&pip->pr_nswap, 
				&pip->pr_cnswap 
				/* , &pip->pr_exit_signal  */, 
				&pip->pr_lproc,
			&pip->pr_rt_priority,
				&pip->pr_policy,
				&pip->pr_delayacct_blkio_ticks,
				&pip->pr_guest_time,
				&pip->pr_cguest_time);

		}

	switch (pip->pr_state) {
	  case 'T':
	  	proc_stopped++;
		break;
	  case 'Z':
	  	proc_zombie++;
		break;
	  }

	chk_free((void *) mem);

	/***********************************************/
	/*   Create  history  of  samples. We sort on  */
	/*   these  to  get  a  decaying  average  of  */
	/*   activity.				       */
	/***********************************************/
/*if (pp->pi_pid == 1792) //strstr(pp->pi_cmdline, "firefox"))
printf("%d/%x hello %s stime=%u utime=%u now=%lu sec=%lu diff=%d\n", 
pp->pi_pid, pp->pi_flags & PI_IS_THREAD, pp->pi_cmdline, 
(unsigned) pip->pr_stime, (unsigned) pip->pr_utime,
pip->pr_tnow.tv_sec,
pip->pr_tlast.tv_sec,
diff_time(&pip->pr_tnow, &pip->pr_tlast)
);
*/
	memmove(&pip->pr_last_pctcpu[1], &pip->pr_last_pctcpu[0], (NUM_PCTCPU - 1) * sizeof(unsigned));
	pip->pr_last_pctcpu[0] = pip->pr_pctcpu;
	pip->pr_pctcpu = ((pip->pr_stime + pip->pr_utime) - (pip->pr_last_stime + pip->pr_last_utime));

/*
if (pp->pi_pid == 1792 && (pip->pr_stime + pip->pr_utime) < (pip->pr_last_stime + pip->pr_last_utime)) {
abort();
}
if (pp->pi_pid == 1792) //strstr(pp->pi_cmdline, "firefox"))
printf("  stime=%lu+%lu - %lu+%lu\n", pip->pr_stime, pip->pr_utime, pip->pr_last_stime, pip->pr_last_utime);
if (pp->pi_pid == 1792) //strstr(pp->pi_cmdline, "firefox"))
printf("  -> pctcpu=%lu tnow=%lu tlast=%lu\n", (unsigned long) pip->pr_pctcpu, pip->pr_tnow.tv_sec, pip->pr_tlast.tv_sec);
*/
	t = diff_time(&pip->pr_tnow, &pip->pr_tlast);
	pip->pr_pctcpu = (100 * pip->pr_pctcpu) / t;
/*
if (pp->pi_pid == 1792) //strstr(pp->pi_cmdline, "firefox"))
printf("  -> %lu t=%d cpu0=%lu\n", (unsigned long) pip->pr_pctcpu, (int) t, (unsigned long) pip->pr_pctcpu0);
*/
	pip->pr_pctcpu = 0.8 * pip->pr_pctcpu + 
			 0.2 * pip->pr_pctcpu0;

/*
if (pp->pi_pid == 1792) //strstr(pp->pi_cmdline, "firefox"))
printf("  -> %p %p %lu\n", pp, pip, (unsigned long) pip->pr_pctcpu);
*/

}

procinfo_t *
raw_proc_get_proclist(int *nump)
{
	procinfo_t	*p;
	int	i;

	proc_running = proc_zombie = proc_stopped = 0;

	/***********************************************/
	/*   Swap  parray  and  last_proc_array so we can  */
	/*   avoid malloc/free.			       */
	/***********************************************/
	for (i = 0; i < last_proc_count; i++) {
		chk_free_ptr((void **) &last_proc_array[i].pi_cmdline);
		}
	for (i = 0; i < proc_count; i++) {
		chk_free_ptr((void **) &proc_array[i].pi_cmdline);
		}
# define SWAP(a, b, t) t = a; a = b; b = t

	SWAP(last_proc_array, proc_array, p);
	SWAP(last_proc_count, proc_count, i);
	SWAP(last_proc_size, proc_size, i);


	proc_count = 0;
	if (proc_size == 0) {
		proc_size = 200;
		proc_array = (procinfo_t *) chk_alloc(sizeof(procinfo_t) * proc_size);
		}

	num_procs = 0;
	proc_get_procdir("/proc", TRUE, &proc_count);
	*nump = proc_count;
	num_procs = proc_count;
	first_display = FALSE;

	qsort(proc_array, proc_count, sizeof *proc_array, last_sort_pid);

	return proc_array;
}
/**********************************************************************/
/*   Read syscall (if we can).					      */
/**********************************************************************/
# define	MAX_SYSCALLS 1024
char *syscalls[MAX_SYSCALLS];

int
read_syscall(int pid, int uid)
{	static char	buf[256];
	int	fd, n;
static	int first_time = 1;
static int myuid = -1;

	if (myuid < 0)
		myuid = getuid();

	if (first_time < 0)
		return -1;

	/***********************************************/
	/*   Avoid opening a file if it doesnt exist.  */
	/***********************************************/
	if (first_time == 1) {
		if ((fd = open("/proc/self/syscall", O_RDONLY)) < 0) {
			first_time = -1;
			return -1;
			}
		first_time = FALSE;
		close(fd);
		}

	/***********************************************/
	/*   Dont try this if we are going to fail.    */
	/***********************************************/
	if (uid != myuid || myuid != 0)
		return -1;

	snprintf(buf, sizeof buf, "/proc/%d/syscall", pid);
	if ((fd = open(buf, O_RDONLY)) < 0)
		return -1;
	n = read(fd, buf, 128);
	close(fd);
	n = atoi(buf);
	return n;
}
char *
syscall_name(int n)
{	static char	buf[128];
static int first_time = TRUE;

	if (first_time) {
		FILE	*fp;
		char	*name;
		char	*val;
		int	n;

		first_time = FALSE;

		if (sizeof(void *) == 8)
			fp = fopen("/usr/include/asm/unistd_64.h", "r");
		else
			fp = fopen("/usr/include/asm/unistd_32.h", "r");
		while (fp && fgets(buf, sizeof buf, fp) != NULL) {
			if (strncmp(buf, "#define __NR_", 13) != 0)
				continue;
			buf[strlen(buf)-1] = '\0';
			name = buf + 13;
			val = strtok(name, "\t ");
			val = strtok(NULL, "\t ");
			if (val == NULL)
				continue;
			n = atoi(val);
			if (n < 0 || n >= MAX_SYSCALLS)
				continue;
			syscalls[n] = chk_strdup(name);
			}
		if (fp)
			fclose(fp);
		}

	if (n >= 0 && n < MAX_SYSCALLS && syscalls[n])
		return syscalls[n];

	snprintf(buf, sizeof buf, "%d", n);
	return buf;
}

