/**********************************************************************/
/*   (C) Paul Fox, 1995-2011					      */
/**********************************************************************/
/*   $Header: $							      */

# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<sys/stat.h>
# include	<sys/sysinfo.h>
# include	<sys/utsname.h>
# include	<dirent.h>
# include	<stdarg.h>
# include	<time.h>
# include	<pwd.h>
# include	<sys/swap.h>
# include	<sys/file.h>
# include	<sys/socket.h>
# include	<netinet/in.h>
# include	<netdb.h>
# include 	<sys/mman.h>
# include	<dstr.h>
# include	"../foxlib/hash.h"

struct columns columns[] = {
	{"USERNAME", "%-8s", 8, 0},
	{"EVENT   ", "%-8s", 8, 0},
	{"SYSCALL ", "%-8s", 8, 0},
	{"%CPU", "%-5s", 5, 0},
	{"%MEM", "%-7s", 7, 0},
	{" SIZE/RSS  ", "%-8s", 8, 0},
	{" TIME", "%-8s", 8, 0},
	{"PID  ", "%-8s", 8, 0},
	{"TH", "%-2s", 8, 0},
	{"COMMAND", "%-8s", 8, 0},
	{0, NULL, 0, 0}
	};

/**********************************************************************/
/*   Symbol table for WCHAN.					      */
/**********************************************************************/
typedef struct symtab_t {
	unsigned long	s_value;
	char	*s_name;
	} symtab_t;
int	sym_count;
int	sym_size;
symtab_t *sym_table;

struct utsname	utsbuf;
unsigned long tot_rss;

/**********************************************************************/
/*   CPU data.							      */
/**********************************************************************/
int	num_cpus;
static	char	cpu_id[256];
char	cpu_info[256];
static	int	hyperthreaded;
char *cpu_names[] = {"usr", "nice", "sys", "idle", "iow", 
	"irq", "sirq", "steal", "guest", "gnice"};

unsigned long min_filesize = 256 * 1024;
unsigned long long main_mem_size = 32 * 1024 * 1024;
unsigned long	mem_free;
unsigned long	mem_cached;
unsigned long	mem_dirty;
unsigned long	swap_total;
unsigned long	swap_free;
unsigned long	swap_used;

const int Hertz = 100;
int	killmon;
int	debugmon;
int	pagesize;
int	fast_poll;
int	sort_order = -1;
int	sort_type = SORT_NORMAL;
int	proc_id;	/* Process ID to display in detail. */
int	quit_flag;
int	c_flag;
int	batch_flag;
int	need_resize;
int	display_mode = DISPLAY_PS;
int	agg_mode;
int	have_task_dir;
int	W_flag;

extern procinfo_t	*proc_array;
extern int	proc_zombie;
extern int	proc_running;
extern int	proc_stopped;
static int	arg_rows;
static int	arg_cols;

struct timeval tv_now;
struct timeval last_tv;
procinfo_t	*pinfo;
extern int	num_procs;
extern int	mon_num_procs;
extern int	mon_num_threads;
int	linux_version;
int	page_size;
char	**global_argv;

extern int	flush_content;
extern int rows;
extern int delay_time;
extern char *grep_filter;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
void int_handler(void);
void settings_load(void);
void settings_save(void);

static unsigned long display_files_mincore(char *filename, struct stat *, int *);
char	*basename(char *);
static void usage(void);
int is_visible(char *name);
static int do_switches(int argc, char **argv);
static void read_system_map(void);
static void read_mem(void);
long	get_counter(char *, char *, long *);
void	stop_handler();
void	win_handler();
int	process_command(char *);
void	wait_for_command(void);
char	*username(int);
char	*event_name(int pid, unsigned long);
int	sort_normal();
void	sort_procs(procinfo_t *pinfo, int num_procs);
void	main_loop(void);
void	update_headers(void);
void	usage(void);
void	print_vnode(long);
int	print_addr(long);
void	main_procmon(int, char **);
static void	get_cpu_info(void);

int
main(int argc, char **argv)
{	char	*cp;
	int	arg_index;
	struct stat sbuf;

	if (strcmp(basename(argv[0]), "procmon") == 0) {
		main_procmon(argc, argv);
		exit(1);
		}

	page_size = getpagesize();

	global_argv = argv;

	arg_index = do_switches(argc, argv);
	if (arg_index < argc) {
		delay_time = atoi(argv[arg_index++]);
		}
	if (batch_flag)
		disp_set_batch(arg_rows, arg_cols);

	pagesize = getpagesize();
	uname(&utsbuf);

	get_cpu_info();
	if (stat("/proc/1/task", &sbuf) == 0)
		have_task_dir = 1;

	/***********************************************/
	/*   Decode     version     into    something  */
	/*   manageable.			       */
	/***********************************************/
	linux_version = atoi(utsbuf.release) * 10 +
		atoi(utsbuf.release + 2);

	read_system_map();

/*	load_nlist();*/

	if (chdir("/proc") < 0) {
		}

	init_display(arg_rows, arg_cols);
	init_termcap(arg_rows, arg_cols);
	init_screen();

	/***********************************************/
	/*   Clear to end of line when we see a \n.    */
	/***********************************************/
	dsp_crmode = TRUE;

	signal(SIGINT, int_handler);
	signal(SIGWINCH, win_handler);
	signal(SIGTSTP, stop_handler);

	main_loop();

	graph_finish();
	end_screen();

	return 0;
}
/**********************************************************************/
/*   Show kstat types.						      */
/**********************************************************************/
static int
sort_kstat(char **p1, char **p2)
{
	return strcmp(*p1, *p2);
}
/**********************************************************************/
/*   Return time diff in 1/10th's of a second.			      */
/**********************************************************************/
int
diff_time(struct timeval *tnow, struct timeval *tlast)
{	int	t = tnow->tv_sec - tlast->tv_sec;

	if (t == 0)
		t = tnow->tv_usec - tnow->tv_usec;
	else {
		t--;
		t *= 1000 * 1000;
		t += tnow->tv_usec;
		t += 1000 * 1000 - tlast->tv_usec;
		}
	t /= 100 * 1000;
	return t ? t : 1;
}

typedef struct file_info {
	long	f_blks;
	char	*f_name;
	unsigned long long f_size;
	} file_info_t;
int
file_sort(file_info_t *p1, file_info_t *p2)
{
	return p2->f_blks - p1->f_blks;
}
static time_t last_time;

/**********************************************************************/
/*   Display files for a process.				      */
/**********************************************************************/
void
display_files()
{	FILE	*fp;
	char	buf[BUFSIZ];
	hash_t	*hash;
	char	*cp;
	unsigned long blks;
	int	fd, i, cnt, err;
static dstr_t old_dstr;
	dstr_t	dstr;
	file_info_t fi;
	file_info_t *fip, *fip1;
	int	perm_errors = 0;
	int	n = 0;

	/***********************************************/
	/*   See if user is trying to flush.	       */
	/***********************************************/
	if (flush_content >= 0 && DSTR_SIZE(&old_dstr)) {
		fip = (file_info_t *) DSTR_STRING(&old_dstr) + flush_content;
		flush_content = -1;
		if (fip < (file_info_t *) DSTR_STREND(&old_dstr) &&
		   (fd = open64(fip->f_name, O_RDONLY )) >= 0) {
		   	struct stat sbuf;
			fstat(fd, &sbuf);

			if (fdatasync(fd)) {
				perror("fdatasync");
				}
			if (posix_fadvise(fd, 0, (long long) sbuf.st_size, 4)) {
				perror("posix_fadvise");
				}
			close(fd);
			}
		}

	dstr_free(&old_dstr);

	/***********************************************/
	/*   Avoid hitting the machine too hard.       */
	/***********************************************/
	if (time(NULL) < last_time + 10)
		return;

	set_attribute(YELLOW, BLACK, 0);
	if ((fp = popen("lsof -f", "r")) == NULL) {
		print("Error: cannot spawn lsof\n");
		return;
		}

	hash = hash_create(64, 64);
	dstr_init(&dstr, 512);
	while (fgets(buf, sizeof buf, fp)) {
		char	proc[BUFSIZ];
		char	pid[BUFSIZ];
		char	user[BUFSIZ];
		char	fd[BUFSIZ];
		char	type[BUFSIZ];
		char	majmin[BUFSIZ];
		char	size[BUFSIZ];
		char	inode[BUFSIZ];
		char	fname[BUFSIZ];
		struct stat sbuf;

		fname[0] = '\0';
		if (sscanf(buf, "%s %s %s %s %s %s %s %s %s",
			proc, pid, user, fd,
			type, majmin, size, inode, fname) != 9)
			continue;
		if (fname[0] != '/')
			continue;
		if (strncmp(fname, "/dev", 4) == 0)
			continue;
		if (strcmp(type, "REG") != 0)
			continue;

		if (fname[strlen(fname) - 1] == '\n')
			fname[strlen(fname)-1] = '\0';

		if (grep_filter && grep_compare(fname))
			continue;

		if (hash_lookup(hash, fname))
			continue;
		blks = display_files_mincore(fname, &sbuf, &err);
		/***********************************************/
		/*   Ignore very small files.		       */
		/***********************************************/
		if (err == -1 || err == -2)
			perm_errors++;
		if (err < 0)
			continue;

		cp = chk_strdup(fname);
		hash_insert(hash, cp, (void *) 1);
		
		fi.f_blks = blks;
		fi.f_name = cp;
		fi.f_size = sbuf.st_size;

		dstr_add_mem(&dstr, (void *) &fi, sizeof fi);
	}

	print("#n  In-Mem  Perc Filename (EPERM=%d)\n", perm_errors);
	fip = (file_info_t *) DSTR_STRING(&dstr);
	cnt = DSTR_SIZE(&dstr) / sizeof fi;
	qsort(fip, cnt, sizeof fi, file_sort);
	for (i = 0; i < DSTR_SIZE(&dstr) / (int) sizeof fi; i++, fip++) {
		int	pc = 0;
		int	divisor;
		float	f;

		pc = 100 * ((float) fip->f_blks * page_size) / fip->f_size;
		if (fip->f_blks == (unsigned long) -2) {
			print("EPERM   %s\n", fip->f_name);
			continue;
			}

		if (fip->f_blks == (unsigned long) -1) {
			print("Deleted %s\n", fip->f_name);
			continue;
			}

		if (fip->f_blks * page_size > 100 * 1000 * 1000) {
			set_attribute(CYAN, BLACK, 0);
			divisor = 1024 * 1024;
			}
		else if (fip->f_blks * page_size > 1000 * 1000) {
			set_attribute(GREEN, BLACK, 0);
			divisor = 1024 * 1024;
			}
		else {
			set_attribute(WHITE, BLACK, 0);
			divisor = 1024;
			}

		/***********************************************/
		/*   See if file has changed.		       */
		/***********************************************/
		fip1 = (file_info_t *) DSTR_STRING(&old_dstr);
		for (; fip1 < (file_info_t *) DSTR_STREND(&old_dstr); fip1++) {
			if (strcmp(fip->f_name, fip1->f_name) == 0) {
				if (fip->f_blks != fip1->f_blks)
					set_attribute(YELLOW, BLACK, 0);
				break;
				}
			}

		f = ((float) fip->f_blks * page_size) / divisor;
		print("%2d: %6lu%s %3d%% %s\n", 
			n++, (unsigned long) f,
			divisor == 1024 ? "K" : "M",
			pc, fip->f_name);
		}
	pclose(fp);
	hash_destroy(hash, 0);
	
	/***********************************************/
	/*   Save current file state		       */
	/***********************************************/
	fip = (file_info_t *) DSTR_STRING(&old_dstr);
	for (; fip < (file_info_t *) DSTR_STREND(&old_dstr); fip++) {
		chk_free(fip->f_name);
		}
	dstr_free(&old_dstr);
	old_dstr = dstr;

	clear_to_end_of_screen();
	last_time = time(NULL);
}
/**********************************************************************/
/*   Compute number of blocks in memory.			      */
/**********************************************************************/
static unsigned long
display_files_mincore(char *filename, struct stat *sbuf, int *err)
{
	int fd;
	unsigned cnt;
	void *file_mmap;
	unsigned char *mincore_vec;
	unsigned long size;
	size_t page_index;

	*err = 0;
	if (lstat(filename, sbuf) == 0 && S_ISLNK(sbuf->st_mode)) {
		*err = -1;
		return 0;
		}
	size = sbuf->st_size;
	if (size > 3000 * 1024 * 1024UL)
		size = 3000 * 1024 * 1024UL;
	if (size < min_filesize) {
		*err = -3;
		return 0;
		}
	if ((fd = open64(filename, O_RDONLY)) < 0) {
		*err = -2;
		return 0;
		}

	file_mmap = mmap((void *)0, size, PROT_NONE, MAP_SHARED, fd, 0);
	mincore_vec = calloc(1, (size+page_size-1)/page_size);
	mincore(file_mmap, size, mincore_vec);
	cnt = 0;
	for (page_index = 0; page_index <= size/page_size; page_index++) {
		if (mincore_vec[page_index]&1) {
		    cnt++;
		}
	}
	munmap(file_mmap, size);
	free(mincore_vec);
	close(fd);
	return cnt;
}
void
display_kstat()
{
	clear_to_end_of_screen();
}


void
display_proc()
{	char	*cp, *cp1, *cp2;
	char	buf[BUFSIZ];
	int	ln;

	clear_to_end_of_screen();
	snprintf(buf, sizeof buf, "/proc/%d/status", proc_id);
	cp = read_file(buf, NULL);
	for (cp1 = cp; *cp1; cp1++) {
		if (*cp1 == '\t')
			*cp1 = ' ';
		}
	print("%s", cp);
	chk_free(cp);

	/***********************************************/
	/*   Syscall.				       */
	/***********************************************/
	snprintf(buf, sizeof buf, "/proc/%d/syscall", proc_id);
	cp = read_file(buf, NULL);
	print("syscall: %s\n", syscall_name(atoi(cp)));
	print("  %s", cp);
	chk_free(cp);

	clear_to_end_of_screen();

	/***********************************************/
	/*   Stack.				       */
	/***********************************************/
	snprintf(buf, sizeof buf, "/proc/%d/stack", proc_id);
	cp = read_file(buf, NULL);
	mvprint(8, 42, "Stack:");
	for (cp1 = cp, ln = 7; *cp1; ln++) {
		char *cp2 = strchr(cp1, '\n');
		int	len;
		if (cp2 == NULL)
			break;
		len = cp2 - cp1;
		if (len > screen_width - 42)
			cp1[screen_width - 42] = '\0';
		else
			*cp2 = '\0';
		mvprint(ln, 42, "%s", cp1);
		cp1 = cp2 + 1;
		}
	chk_free(cp);

# if 0
	struct proc *pp;
	struct proc proc;
	struct pid	pid;
	struct uf_entry uf_entry;
	struct file file;
	int	found_proc = FALSE;
	int	i;

	pp = (struct proc *) get_ksym("*practive");
	for (i = 0; pp && i < 2000; i++) {
		get_struct((long) pp, &proc, sizeof proc);
		get_struct((long) proc.p_pidp, &pid, sizeof pid);
		pp = proc.p_next;
		if (pid.pid_id == proc_id) {
			found_proc = TRUE;
			break;
			}
		}
	if (!found_proc) {
		print("Proc %d: Couldn't find process!\n", proc_id);
		clear_to_end_of_screen();
		display_mode = DISPLAY_PS;
		return;
		}

	print_number_string("PID: %d  PPID: %d\n", proc_id, proc.p_ppid);
	print("Threads: %d\n", proc.p_lwpcnt);
	print("Started: %s", ctime(&proc.p_user.u_start));
	print("Cmd: %s\n", proc.p_user.u_comm);
	print("Ps args: %s\n", proc.p_user.u_psargs);

	print("Files: (num-open=%d)\n", proc.p_user.u_nofiles);
	for (i = 0; i < proc.p_user.u_nofiles; i++) {
		get_struct((long) proc.p_user.u_flist + 
			i * sizeof(struct uf_entry), 
			&uf_entry, sizeof uf_entry);
		if (uf_entry.uf_ofile == NULL)
			continue;

		get_struct((long) uf_entry.uf_ofile, &file, sizeof file);
		print(" #%2d: ", i);
		print(" pos=%9s ", comma(file.f_offset));
		if ((file.f_flag & (FREAD|FWRITE)) == (FREAD|FWRITE))
			print("RW ");
		else {
			if (file.f_flag & FREAD)
				print("RDONLY ");
			if (file.f_flag & FWRITE)
				print("WRONLY ");
			}
		if (file.f_flag & FNDELAY)
			print("NDELAY ");
		if (file.f_flag & FAPPEND)
			print("APPEND ");
		if (file.f_flag & FSYNC)
			print("SYNC ");
		if (uf_entry.uf_refcnt != 1) {
			print(" ref=%d ", 
				uf_entry.uf_refcnt);
			}
		print_vnode((long) file.f_vnode);
		print("\n");
		}
	clear_to_end_of_screen();
# endif
}
int
grep_compare(char *str)
{	char *re = grep_filter;
	int not, ret;

	if (!re)
		return TRUE;

	not = *re == '!';
	if (not)
		re++;
	ret = strstr(str, re) != NULL;
	return not ? !ret : ret;
}
/**********************************************************************/
/*   Process display mode.					      */
/**********************************************************************/
void
display_ps()
{	int	i, j, fg, bg;
	float	f;
	unsigned long t;
	int	len, max_len;
	int	last_uid;
	char	*cmd;
	char	*cp;
	procinfo_t	*pip, *pip1;
	int	nswap;
	char	buf[BUFSIZ];

	set_attribute(YELLOW, BLACK, 0);
	for (j = 0; columns[j].name; j++) {
		if ((columns[j].flags & COL_HIDDEN) == 0)
			print("%s ", columns[j].name);
		}
	print("\n");

	last_uid = -1;
	tot_rss = 0;

	/***********************************************/
	/*   If  in proc mode, then aggregate the cpu  */
	/*   time for threads to the parent.	       */
	/***********************************************/
	if (!proc_view && agg_mode) {
		for (i = 0, pip = pinfo; i < num_procs; i++, pip++) {
			if ((pip->pi_flags & PI_IS_THREAD) == 0 && pip->pi_name[0] != '.')
				continue;
			/***********************************************/
			/*   Search  for  parent  and  accumulate the  */
			/*   cpu.				       */
			/***********************************************/
			for (j = 0, pip1 = pinfo; j < num_procs; j++, pip1++) {
				if (pip->pi_psinfo.pr_ppid == pip1->pi_psinfo.pr_pid) {
					pip1->pi_psinfo.pr_utime += pip->pi_psinfo.pr_utime;
					pip1->pi_psinfo.pr_stime += pip->pi_psinfo.pr_stime;
					break;
					}
				}
			}
		}

	if (num_procs == 0) {
		set_attribute(RED, YELLOW, 0);
		print("Sorry - for some reason we lost the proc info.\n");
		set_attribute(GREEN, BLACK, 0);
		return;
		}

	for (i = 0, pip = pinfo; i < num_procs && csr_y < screen_length; i++, pip++) {
		int	keep = grep_filter ? FALSE : TRUE;

		/***********************************************/
		/*   USERNAME				       */
		/***********************************************/
		if (is_visible("USERNAME")) {
			if (pip->pi_psinfo.pr_isnew)
				set_attribute(BLACK, CYAN, 0);
			else
				set_attribute(GREEN, BLACK, 0);
			if (pip->pi_psinfo.pr_uid == last_uid) {
				strcpy(buf, "  \"  \"   ");
				}
			else {
				last_uid = pip->pi_psinfo.pr_uid;
				cp = username(pip->pi_psinfo.pr_uid);
				snprintf(buf, sizeof buf, "%-8s ", cp);
				}
			print(buf);
			if (!keep && grep_compare(buf))
				keep = TRUE;
			}

		/***********************************************/
		/*   Stop  here  if we dont have a valid proc  */
		/*   info struct.			       */
		/***********************************************/
		if ((pip->pi_flags & PI_PSINFO_VALID) == 0) {
			print("%dK %-5d ", 
				(pip->pi_psinfo.pr_size * pagesize) / 1024,
				pip->pi_pid);
			set_attribute(YELLOW, BLACK, 0);
			if (!keep && grep_compare(pip->pi_cmdline))
				keep = TRUE;
			if (keep) {
				print(pip->pi_cmdline);
				print("\n");
				}
			else {
				print("\r");
				}
			continue;
			}
		/***********************************************/
		/*   EVENT.				       */
		/***********************************************/
		if (is_visible("EVENT")) {
			set_attribute(WHITE, BLACK, 0);

/*			if (pip->pi_psinfo.pr_last_stime)
				time_system += pip->pi_psinfo.pr_stime - pip->pi_psinfo.pr_last_stime;
			if (pip->pi_psinfo.pr_last_utime)
				time_user += pip->pi_psinfo.pr_utime - pip->pi_psinfo.pr_last_utime;
*/
			switch (pip->pi_psinfo.pr_state) {
			  case 'S':
				snprintf(buf, sizeof buf, "%-8.8s ", 
					event_name(pip->pi_pid, (unsigned long) pip->pi_psinfo.pr_wchan));
				break;
			  case 'R':
				set_attribute(GREEN, BLACK, 0);
			  	strcpy(buf, "Running  ");
				proc_running++;
				break;
			  case 'Z':
				strcpy(buf, " zombie  ");
				break;
			  case 'T':
				strcpy(buf, "Stopped  ");
				break;
			  case 'I':
				strcpy(buf, "Idle     ");
				break;
			  case 'D':
				set_attribute(CYAN, BLACK, 0);
				strcpy(buf, "Disk     ");
				break;
			  default:
			  	snprintf(buf, sizeof buf, "State=%c  ", pip->pi_psinfo.pr_state);
				break;
			  }
			print(buf);
			if (!keep && grep_compare(buf))
				keep = TRUE;
			}

		/***********************************************/
		/*   SYSCALL				       */
		/***********************************************/
		if (is_visible("SYSCALL")) {
			print("%-9.9s", syscall_name(pip->pi_psinfo.pr_syscall));
			}

		/***********************************************/
		/*   %CPU				       */
		/***********************************************/
		if (is_visible("%CPU")) {
			int d = (int) pip->pi_psinfo.pr_pctcpu;
if (0)
print(" d=%d ", d); else
			if (d >= 1000)
				print(" 100 ");
			else
				print("%4.1f ", d > 1000 ? 100. : d / 10.);
			}

		/***********************************************/
		/*   MEM				       */
		/***********************************************/
		if (is_visible("%MEM")) {
			print("%4.1f ",
				(((float) pip->pi_psinfo.pr_rssize * pagesize / 1)
				/ main_mem_size) * 100.0);
			}

		/***********************************************/
		/*   SIZE/RSS				       */
		/***********************************************/
		if (is_visible("SIZE/RSS")) {
			char	buf[32];
			cp = pip->pi_psinfo.pr_vsize == pip->pi_psinfo.pr_last_vsize ? NULL : "+";
/*print("%d/%d->", pip->pi_psinfo.pr_vsize, pip->pi_psinfo.pr_last_vsize);*/
/*			t = (pip->pi_psinfo.pr_size * pagesize) / 1024;*/
			t = pip->pi_psinfo.pr_vsize / 1024;
			if (t > 999999) {
				set_attribute(cp ? BLACK : MAGENTA, cp ? MAGENTA : BLACK, 0);
				print("%.2fG", (float) t / (1024 * 1024));
				set_attribute(WHITE, BLACK, 0);
				}
			else if (t > 99999) {
				set_attribute(cp ? BLACK : YELLOW, cp ? YELLOW : BLACK, 0);
				print("%4luM", t / 1024);
				set_attribute(WHITE, BLACK, 0);
				}
			else {
				set_attribute(cp ? BLACK : WHITE, cp ? WHITE : BLACK, 0);
				print("%5ld", t);
				}
			cp = pip->pi_psinfo.pr_rssize == pip->pi_psinfo.pr_last_rssize ? NULL : "+";
			t = pip->pi_psinfo.pr_rssize * (pagesize / 1024);
			if (t > 999999) {
				set_attribute(cp ? BLACK : MAGENTA , cp ? MAGENTA : BLACK, 0);
				print("/");
				snprintf(buf, sizeof buf, "%.2fG", (float) t / (1024 * 1024));
				print("%-5s ", buf);
				}
			else if (t > 99999) {
				set_attribute(cp ? BLACK : YELLOW, cp ? YELLOW : BLACK, 0);
				print("/");
				snprintf(buf, sizeof buf, "%luM", t / 1024);
				print("%-5s ", buf);
				}
			else {
				print("/%-5ld ", t);
				}
			if ((pip->pi_flags & PI_IS_THREAD) == 0 && pip->pi_name[0] != '.')
				tot_rss += (pip->pi_psinfo.pr_rssize * pagesize) / 1024;
			}

		/***********************************************/
		/*   TIME				       */
		/***********************************************/
		if (is_visible("TIME")) {
			set_attribute(WHITE, BLACK, 0);
			t = pip->pi_psinfo.pr_utime + pip->pi_psinfo.pr_stime;
			if (t < 9 * Hertz)
				print("%5.2f ", (float) t / Hertz);
			else
				print("%2d:%02d ", 
					t / Hertz / 60,
					t / Hertz % 60);
			}

		/***********************************************/
		/*   PID				       */
		/***********************************************/
		if (is_visible("PID")) {
			char	buf[64];
			if ((pip->pi_flags & PI_IS_THREAD) == 0 && pip->pi_name[0] != '.')
				set_attribute(GREEN, BLACK, 0);
			print("%-5d ", pip->pi_pid);
			set_attribute(CYAN, BLACK, 0);
			}

		/***********************************************/
		/*   THR				       */
		/***********************************************/
		if (is_visible("THR")) {
			print("%2d ", pip->pi_psinfo.pr_num_threads);
			}

		/***********************************************/
		/*   Strip off leading part of cmd name.       */
		/***********************************************/
		if (is_visible("COMMAND")) {
			char	*cp1;
			cmd = pip->pi_cmdline;
			if (!keep && grep_compare(cmd))
				keep = TRUE;
			max_len = screen_width - dst_x;
			len = (int) strlen(cmd);

			if (len > max_len && strstr(cmd, "perl") && 
			    (cp = strchr(cmd, ' ')) != NULL) {
				cmd = ++cp;
				while (*cp && *cp == '-') {
					cp++;
					while (*cp && !isspace(*cp))
						cp++;
					while (*cp && isspace(*cp))
						cp++;
					}
				if (*cp) {
					cmd = cp;
					}
				len = strlen(cmd);
				}
			else if (len > max_len && (cp = strchr(cmd, ' ')) != NULL) {
				if ((int) strlen(cp) > max_len) {
					while (cp > cmd && *cp != '/')
						cp--;
					}
				else {
					while (cp > cmd && (int) strlen(cp) < max_len)
						cp--;
					}
				if (*cp == '/')
					cp++;
				cmd = cp;
				len = strlen(cmd);
				}
			if (len > max_len)
				len = max_len;
			for (j = 0; keep && j < len; j++) {
				int	ch = cmd[j];
				if (dst_x >= screen_width)
					break;
				if (ch == '\0')
					break;
				if (ch < ' ')
					print(".");
				else
					print("%c", ch);
				}
			}
		if (!keep) {
			print("\r");
			}
		else
			print("\n");
		}
	clear_to_end_of_screen();
}
/**********************************************************************/
/*   Decode command line switches.				      */
/**********************************************************************/
static int
do_switches(int argc, char **argv)
{	int	i;

	for (i = 1; i < argc; i++) {
		char *cp = argv[i];
		if (*cp++ != '-')
			break;
		if (strcmp(cp, "batch") == 0) {
			batch_flag = TRUE;
			arg_rows = 100;
			arg_cols = 100;
			continue;
			}
		if (strcmp(cp, "cols") == 0) {
			if (++i >= argc)
				usage();
			arg_cols = atoi(argv[i]);
			continue;
			}
		if (strcmp(cp, "debugmon") == 0) {
			debugmon = TRUE;
			continue;
			}
		if (strcmp(cp, "fast_poll") == 0) {
			fast_poll = TRUE;
			continue;
			}

		if (strcmp(cp, "killmon") == 0) {
			killmon = TRUE;
			continue;
			}
		if (strcmp(cp, "rows") == 0) {
			if (++i >= argc)
				usage();
			arg_rows = atoi(argv[i]);
			continue;
			}

		while (*cp) {
			switch (*cp++) {
			  case 'c':
				if (++i >= argc)
					usage();
			 	c_flag = atoi(argv[i]);
				continue;
			  case 'W':
			  	W_flag = TRUE;
				break;
			  default:
			  	usage();
				break;
			  }
			}
		}
	return i;
}
/**********************************************************************/
/*   Try  and  convert  a  wait  channel  to the name of a nice data  */
/*   structure.                                                       */
/**********************************************************************/
hash_t *ehash;
char *
event_name(int pid, unsigned long wchan)
{	static char buf[24];
	char	buf2[128];
	int	fd;
	int	i;
	char	*cp;

	if (wchan == 0)
		return "-";

	if (ehash == NULL)
		ehash = hash_create(32, 32);
	if ((cp = hash_int_lookup(ehash, wchan)) != NULL)
		return cp;

	snprintf(buf2, sizeof buf2, "/proc/%d/wchan", pid);
	if ((fd = open(buf2, O_RDONLY)) >= 0) {
		int n = read(fd, buf, sizeof buf - 1);
		close(fd);
		buf[n] = '\0';
		hash_int_insert(ehash, wchan, chk_strdup(buf));
		return buf;
		}

	for (i = 0; i < sym_count; i++) {
		if (sym_table[i].s_value == wchan)
			return sym_table[i].s_name;
		if (i && sym_table[i].s_value > wchan)
			return sym_table[i-1].s_name;
		if (sym_table[i].s_value > wchan)
			break;
		}
	snprintf(buf, sizeof buf, "%08lx", wchan);
	return buf;
}
static void
get_cpu_info()
{	FILE	*fp;
	char	buf[BUFSIZ];
static int first_time = TRUE;

	if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
		return;

	if (num_cpus && cpuinfo == NULL)
		cpuinfo = (struct cpuinfo_t *) chk_zalloc(num_cpus * sizeof *cpuinfo);
	num_cpus = 0;
	while (fgets(buf, sizeof buf, fp)) {
		buf[strlen(buf)-1] = '\0';
		if (strncmp(buf, "processor", 9) == 0)
			num_cpus++;
		if (strncmp(buf, "flags", 5) == 0 &&
		    strstr(buf, " ht "))
		    	hyperthreaded++;
		if (strncmp(buf, "model name", 10) == 0) {
			char *cp = strchr(buf, 'U');
			if (cp == NULL)
				cp = strchr(buf, ')');
			cp++;
			strcpy(cpu_id, cp);
			while (cp = strstr(cpu_id, "  "))
				memcpy(cp, cp + 1, strlen(cp + 1) + 1);
			strcpy(cpu_info, strchr(buf, ':') + 2);
			continue;
			}

		if (cpuinfo == NULL)
			continue;

		if (strncmp(buf, "cpu MHz", 7) == 0) {
			char *cp = strchr(buf, ':') + 2;
			strncpy(cpuinfo[num_cpus-1].cpu_mhz, cp, 64);
			continue;
			}
		if (strncmp(buf, "bogomips", 7) == 0) {
			char *cp = strchr(buf, ':') + 2;
			strncpy(cpuinfo[num_cpus-1].bogomips, cp, 64);
			continue;
			}
		if (strncmp(buf, "cache size", 10) == 0) {
			char *cp = strchr(buf, ':') + 2;
			strncpy(cpuinfo[num_cpus-1].cache_size, cp, 64);
			continue;
			}
		}
	fclose(fp);
}
unsigned long
get_value(char *cp)
{	unsigned long ul = 0;

	while (isspace(*cp))
		cp++;
	while (!isspace(*cp))
		cp++;
	while (isspace(*cp))
		cp++;
	sscanf(cp, "%lu", &ul);
	return ul;
}
void
int_handler()
{
	graph_finish();
	end_screen();
	exit(0);
}
int
is_visible(char *name)
{	int	i;

	for (i = 0; columns[i].name; i++) {
		char	*cp = columns[i].name;
		while (*cp == ' ')
			cp++;
		if (strncmp(cp, name, strlen(name)) == 0)
			return !(columns[i].flags & COL_HIDDEN);
		}
	return TRUE;
}
void
main_loop(void)
{
	int	i, j, t;
	int	first_time = TRUE;
	int	pass = 0;
	int	stale_displayed = FALSE;
	unsigned long long lt0 = 0, lt = 0;

	settings_load();
	monitor_start();

	while (!quit_flag) {

		if (need_resize) {
			need_resize = FALSE;
			init_display(0, 0);
			screen_length = rows;
			}

		/***********************************************/
		/*   Avoid  refreshing if nothing changed but  */
		/*   avoid  infinite  loop  where  user loses  */
		/*   control.				       */
		/***********************************************/
		mon_lock();
		lt0 = lt;
		lt = mon_get_time();
		if (lt0 == lt) {
			struct timeval tval = {0, 50 * 1000};
			fd_set rbits;
			int	ret;

			FD_ZERO(&rbits);
			FD_SET(1, &rbits);

			mon_unlock();
			ret = select(32, &rbits, NULL, NULL, &tval);
			if (lt == (unsigned long long) -1) {
				set_attribute(RED, YELLOW, 0);
				mvprint(5, 0, "Status pmap is missing - will refresh when procmon restarts.");
				set_attribute(GREEN, BLACK, 0);
				clear_to_eol();
				refresh();
				monitor_uninit();
				monitor_start();
				stale_displayed = 2;
				}
			else if (stale_displayed == 2) {
				mvprint(5, 0, "");
				clear_to_eol();
				refresh();
				stale_displayed = FALSE;
				}
			else if (ret)
				wait_for_command();
			continue;
			}

		last_tv = tv_now;
		gettimeofday(&tv_now, 0);

		read_mem();
		pinfo = proc_get_proclist(&num_procs);
		sort_procs(pinfo, num_procs);
		get_cpu_info();

		mvprint(6, 0, "");

		switch (display_mode) {
		  case DISPLAY_CMD:
		  	shell_command((char *) NULL);
			break;
		  case DISPLAY_CPU:
		  	display_cpu();
			break;
		  case DISPLAY_DISK:
		  	display_disk();
			break;
		  case DISPLAY_FILES:
		  	display_files();
			break;
		  case DISPLAY_GRAPHS:
		  	display_graphs();
			break;
		  case DISPLAY_ICMP:
		  	display_ICMP();
			break;
		  case DISPLAY_IFCONFIG:
		  	display_ifconfig();
			break;
		  case DISPLAY_IP:
		  	display_IP();
			break;
		  case DISPLAY_INTERRUPTS:
		  	display_interrupts();
			break;
		  case DISPLAY_KSTAT:
		  	display_kstat();
			break;
		  case DISPLAY_MEMINFO:
		  	display_meminfo();
			break;
		  case DISPLAY_NETSTAT:
		  	display_netstat();
			break;
		  case DISPLAY_PROC:
		  	display_proc();
			break;
		  case DISPLAY_PS:
		  	display_ps();
			break;
		  case DISPLAY_SOFTIRQS:
		  	display_softirqs();
			break;
		  case DISPLAY_TCP:
		  	display_TCP();
			break;
		  case DISPLAY_TEMPERATURE:
		  	display_temperature();
			break;
		  case DISPLAY_UDP:
		  	display_UDP();
			break;
		  case DISPLAY_VMSTAT:
		  	display_vmstat();
			break;
		  }

		update_headers();
		mon_unlock();

		if (pass++ == 0) {
			set_attribute(CYAN, BLACK, 0);
			mvprint(5, 0, "Type 'help' for help.");
			}
if (0){extern int mon_pos;
mvprint(5, 0, "pos=%d %d %llu\n", mon_pos, mon_tell(), (unsigned long long) mon_get_time());
}
		mvprint(5, 60, "Mode: %s\n",
			mode == MODE_ABS ? "abs" :
			mode == MODE_COUNT ? "count" :
			mode == MODE_DELTA ? "delta" : "ranged");

		out_flush();

		t = mon_is_stale();
		if (t > 10) {
			mvprint(5, 0, "Warning: procmon process data is stale.\n");
			monitor_uninit();
			monitor_start();
			stale_displayed = TRUE;
		} else if (stale_displayed) {
			mvprint(5, 0, "procmon data now up to date.\n");
			stale_displayed = FALSE;
		}

		/***********************************************/
		/*   Save  copy  of screen in case user wants  */
		/*   to snapshot.			       */
		/***********************************************/
		snapshot();

		/***********************************************/
		/*   Actively   log  the  window  if  we  are  */
		/*   writing to a log.			       */
		/***********************************************/
		refresh_fp(log_fp);
		if (batch_flag)
			refresh_fp(stdout);

		if (c_flag && --c_flag <= 0)
			break;

		wait_for_command();
		}
}
void
print_vnode(long addr)
{
# if 0
	char	*cp;
	struct vnode	vnode;
	struct vnode vnode1;
	struct fifonode fifonode;

	get_struct(addr, &vnode, sizeof vnode);
	switch (vnode.v_type) {
	  case VNON: print("VNON "); break;
	  case VREG: 
	  	print("VREG ");
		print_filename(addr, &vnode);
		return;
	  case VDIR: 
		print("VDIR ");
		print_filename(addr, &vnode);
		return;
	  case VBLK:
		print("VBLK [%s]", dev_name(vnode.v_rdev));
		return;
	  case VCHR: 
	  	cp = dev_name(vnode.v_rdev);
		if (*cp != '/')
			print("VCHR ");
		print("%s", cp);
		break;
	  case VLNK: print("VLNK "); break;
	  case VBAD: print("VBAD "); break;
	  case VFIFO: 
		get_struct((long) vnode.v_data, &fifonode, sizeof fifonode);
		print("VFIFO "); 
		print("cnt=%x/%x",
			fifonode.fn_rcnt,
			fifonode.fn_wcnt
			);
		break;
	  default: print("vtype=%x ", vnode.v_type); break;
	  }
	if (vnode.v_stream) {
		print(" ");
		streams_display(vnode.v_stream, FALSE);
		}
# endif
}
# define	CACHE_SIZE	8
int
print_addr(long l)
{	static struct hostent *hp;
	long	l1;
	int	i;
	char	buf[256];
	static long last_addr;
static struct cache {
	long	c_ipaddr;
	char	*c_name;
	} cache_tbl[CACHE_SIZE];
	struct cache *cap;
	char	*cp;
static	int	next_cache_entry;

	if (l == 0) {
		print("*");
		return 1;
		}

	l1 = htonl(l);
	for (i = 0; i < CACHE_SIZE; i++) {
		if (cache_tbl[i].c_ipaddr == l1 &&
		    cache_tbl[i].c_name) {
		    	cp = cache_tbl[i].c_name;
		    	print("%.19s", cache_tbl[i].c_name);
			return strlen(cp) > 19 ? 19 : strlen(cp);
			}
		}

	cap = &cache_tbl[next_cache_entry];
	chk_free_ptr((void **) &cap->c_name);
	next_cache_entry = (next_cache_entry + 1) % CACHE_SIZE;

	hp = gethostbyaddr(&l1, sizeof l1, AF_INET);

	if (hp == NULL) {
		snprintf(buf, sizeof buf, "%lu.%lu.%lu.%lu", 
			(l >> 24) & 0xff,
			(l >> 16) & 0xff,
			(l >> 8) & 0xff,
			l & 0xff);
		}
	else {
		strcpy(buf, hp->h_name);
		}
	cap->c_ipaddr = l1;
	cap->c_name = chk_strdup(buf);
	print("%.19s", cap->c_name);
	return strlen(cap->c_name) > 19 ? 19 : strlen(cap->c_name);
}

void
print_ranged(unsigned long long n)
{
	if (n < 100 * 1024)
		print_number_string("%6llu", n);
	else if (n < 1000*1000)
		print_number_string("%5lluK", n / 1024);
	else
		print_number_string("%5lluM", n / (1024 * 1024));
}
void
print_ranged2(unsigned long long n, unsigned long long n0)
{	char	buf[BUFSIZ];

	if (n < 100 * 1024)
		snprintf(buf, sizeof buf, "%6llu", n);
	else if (n < 100 * 1000*1000)
		snprintf(buf, sizeof buf, "%5lluK", n / 1024);
	else
		snprintf(buf, sizeof buf, "%5lluM", n / (1024 * 1024));
	print_number3(buf, n, n0);
}
void
print_number3(char *buf, unsigned long long n, unsigned long long n0)
{
/*	set_attribute(GREEN, BLACK, 0);
	while (*buf == ' ') {
		print(" ");
		buf++;
		}
*/
	if (n == n0) {
		set_attribute(YELLOW, BLACK, 0);
		}
	else if (n > n0) {
		set_attribute(BLACK, YELLOW, 0);
		}
	else {
		set_attribute(BLACK, RED, 0);
		}
	print(buf);
	set_attribute(GREEN, BLACK, 0);
}
/**********************************************************************/
/*   Function to return an array of processes running on the system.  */
/**********************************************************************/
procinfo_t *
proc_get_proclist(int *nump)
{
	*nump = mon_read_procs();
	return proc_array;
}

/**********************************************************************/
/*   Read contents of a file into a memory block.		      */
/**********************************************************************/
char *
read_file(char *path, char *name)
{	char	*cp = read_file2(path, name);

	if (cp)
		return cp;
	return chk_strdup("  <non-existant>");
}
char *
read_file2(char *path, char *name)
{
static	char	*buf;
static int	bsize = 1024;
	int	fd, n, n1, ret;
	int	size = 0;

	if (buf == NULL) {
		buf = chk_alloc(bsize);
		}

	if (name)
		snprintf(buf, bsize - 1, "%s/%s", path, name);
	else
		strcpy(buf, path);

	if ((fd = open(buf, O_RDONLY)) < 0)
		return NULL;

	while (1) {
		char *bp = buf + size;
		n = bsize - size;
		if (n <= 10) {
			/***********************************************/
			/*   Sanity check - dont eat all memory.       */
			/***********************************************/
			if (bsize > 100 * 1024)
				break;
			bsize += 2048;
			buf = chk_realloc(buf, bsize);
			n = bsize - size;
			bp = buf + size;
			}

		ret = read(fd, bp, n - 1);
		if (ret <= 0)
			break;
		for (n1 = 0; n1 < ret; n1++) {
			if (bp[n1] == '\0')
				bp[n1] = ' ';
			}
		size += ret;
		if (ret < n - 1)
			break;
		}
	buf[size++] = '\0';
	close(fd);

	if (size) {
		return chk_strdup(buf);
		}
	return chk_strdup(" <read-error>");
}
/**********************************************************************/
/*   Function to read /proc/meminfo.				      */
/**********************************************************************/
static void
read_mem()
{
	main_mem_size = mon_get("meminfo.MemTotal") * 1024;
	mem_free = mon_get("meminfo.MemFree");
	mem_cached = mon_get("meminfo.Cached");
	mem_dirty = mon_get("meminfo.Dirty");
	swap_total = mon_get("meminfo.SwapTotal");
	swap_free = mon_get("meminfo.SwapFree");
	swap_used = swap_total - swap_free;

}
/**********************************************************************/
/*   Load user profile.						      */
/**********************************************************************/
void
settings_load()
{	FILE	*fp;
	char	fname[PATH_MAX];
	char	buf[BUFSIZ];
	char	*home = getenv("HOME");

	if (home == NULL)
		return;

	snprintf(fname, sizeof fname, "%s/.proc", home);
	if ((fp = fopen(fname, "r")) == NULL)
		return;

	while (fgets(buf, sizeof buf, fp)) {
		while (*buf && buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		if (strncmp(buf, "display ", 8) == 0) {
			display_mode = atoi(buf + 8);
			continue;
			}
		process_command(buf);
		}
	fclose(fp);

}
/**********************************************************************/
/*   Save the user settings.					      */
/**********************************************************************/
void
settings_save()
{	FILE	*fp;
	char	fname[PATH_MAX];
	char	buf[BUFSIZ];
	char	*home = getenv("HOME");
	int	i;

	if (home == NULL)
		return;

	if (W_flag)
		return;

	snprintf(fname, sizeof fname, "%s/.proc", home);
	if ((fp = fopen(fname, "w")) == NULL)
		return;
	for (i = 0; columns[i].name; i++) {
		char	*cp = columns[i].name;
		while (*cp == ' ')
			cp++;
		if ((columns[i].flags & COL_HIDDEN) == 0)
			continue;
		fprintf(fp, "hide %s\n", cp);
	}
	fprintf(fp, "display %d\n", display_mode);
	fclose(fp);
}

int
sym_sort(symtab_t *p1, symtab_t *p2)
{
	return p1->s_value - p2->s_value;
}
static void 
read_system_map(void)
{	FILE	*fp;
	char	buf[BUFSIZ];
	int	addr;
	char	name[BUFSIZ];
	char	dummy[BUFSIZ];
	struct stat sbuf;

	snprintf(buf, sizeof buf, "/proc/%d/wchan", getpid());
	if (stat(buf, &sbuf) == 0) {
		if (getenv("PROC_READ_KALLSYMS") == NULL)
			return;
		}

	fp = fopen("/proc/kallsyms", "r");
	if (fp == (FILE *) NULL)
		fp = fopen("/proc/ksyms", "r");
	if (fp == (FILE *) NULL)
		return;
	while (!feof(fp)) {
		if (fgets(buf, sizeof buf, fp) == (char *) NULL)
			break;
		if (buf[10] == ' ') {
			if (sscanf(buf, "%x %s %s", &addr, dummy, name) != 3)
				continue;
			if (dummy[0] == 'U')
				continue;
			}
		else {
			/***********************************************/
			/*   /proc/ksyms.			       */
			/***********************************************/
			if (sscanf(buf, "%x %s", &addr, name) != 2)
				continue;
			}

		if (sym_count >= sym_size) {
			sym_size += 256;
			if (sym_count == 0)
				sym_table = (symtab_t *) chk_alloc(
					sym_size * sizeof(symtab_t));
			else
				sym_table = (symtab_t *) chk_realloc(sym_table, 
					sym_size * sizeof(symtab_t));
			}
		sym_table[sym_count].s_value = addr;
		sym_table[sym_count].s_name = chk_strdup(name);
		sym_count++;
		}
	fclose(fp);
	qsort(sym_table, sym_count, sizeof sym_table[0], sym_sort);
}
/**********************************************************************/
/*   Sort processes into interesting order.			      */
/**********************************************************************/
int
sort_pid(procinfo_t *p1, procinfo_t *p2)
{	int	t;

	if ((t = p2->pi_psinfo.pr_pid - p1->pi_psinfo.pr_pid) != 0)
		return sort_order * t;
	return sort_normal(p1, p2);
}
int
sort_normal(procinfo_t *p1, procinfo_t *p2)
{	int	t;
	int	i;

	if ((t = p1->pi_psinfo.pr_pctcpu - p2->pi_psinfo.pr_pctcpu) != 0)
		return sort_order * t;

	if ((t = p1->pi_psinfo.pr_isnew - p2->pi_psinfo.pr_isnew) != 0)
		return sort_order * t;

	for (i = 0; i < NUM_PCTCPU; i++) {
		if ((t = p1->pi_psinfo.pr_last_pctcpu[i] - p2->pi_psinfo.pr_last_pctcpu[i]) != 0)
			return sort_order * t;
		}
	t = p1->pi_psinfo.pr_vsize - p1->pi_psinfo.pr_last_vsize;
	t -= p2->pi_psinfo.pr_vsize - p2->pi_psinfo.pr_last_vsize;
	if (t)
		return sort_order * t;
	if ((t = p1->pi_psinfo.pr_rssize - p2->pi_psinfo.pr_rssize) != 0)
		return sort_order * t;

	return sort_order * ((int) p1->pi_psinfo.pr_pid - (int) p2->pi_psinfo.pr_pid);
}
int
sort_size(procinfo_t *p1, procinfo_t *p2)
{	int	t;

	if ((t = p2->pi_psinfo.pr_size - p1->pi_psinfo.pr_size) != 0)
		return sort_order * t;
	return sort_normal(p1, p2);
}
int
sort_rss(procinfo_t *p1, procinfo_t *p2)
{	int	t;

	if ((t = p2->pi_psinfo.pr_rssize - p1->pi_psinfo.pr_rssize) != 0)
		return sort_order * t;
	return sort_size(p1, p2);
}
int
sort_user(procinfo_t *p1, procinfo_t *p2)
{	int	t;

	if ((t = p2->pi_psinfo.pr_uid - p1->pi_psinfo.pr_uid) != 0)
		return sort_order * t;
	return sort_normal(p1, p2);
}
int
sort_clock(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p2->pi_psinfo.pr_start.tv_sec - p1->pi_psinfo.pr_start.tv_sec) != 0)
		return sort_order * t;
	return sort_pid(p1, p2);
}
int
sort_time(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p1->pi_psinfo.pr_time.tv_sec - p2->pi_psinfo.pr_time.tv_sec) != 0)
		return sort_order * t;
	return sort_pid(p1, p2);
}

void
sort_procs(procinfo_t *pinfo, int num_procs)
{	int	(*sort_func)();

	switch (sort_type) {
	  case SORT_NORMAL: sort_func = sort_normal; break;
	  case SORT_PID: sort_func = sort_pid; break;
	  case SORT_USER: sort_func = sort_user; break;
	  case SORT_SIZE: sort_func = sort_size; break;
	  case SORT_TIME: sort_func = sort_time; break;
	  case SORT_RSS: sort_func = sort_rss; break;
	  case SORT_CLOCK: sort_func = sort_clock; break;
	  default: sort_func = sort_normal; break;
	  }
	qsort(pinfo, num_procs, sizeof pinfo[0], sort_func);
}
void
error_message(char *fmt, ...)
{	va_list	ap;
	char	buf[BUFSIZ];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	printf("%s\n", buf);
}
/**********************************************************************/
/*   Handle ^Z.                                                       */
/**********************************************************************/
void
stop_handler()
{
	end_screen();
	out_flush();
	signal(SIGTSTP, SIG_DFL);
	sigsetmask(0);

	kill(getpid(), SIGTSTP);

	signal(SIGTSTP, stop_handler);
	clear();
	reinit_screen();
}
/**********************************************************************/
/*   Display the header area.					      */
/**********************************************************************/
void
update_headers()
{	char	*cp, *cp1;
	unsigned long long t;
	time_t t2;
	struct tm *tm = NULL;
	unsigned long long num_ints, num_ints2;
	unsigned long long tot = 0;
	char *comma = "";
	unsigned long long btime;
	unsigned long long times[MAX_CPU_TIMES];
	int	ret, i;

	t = mon_get_time() - mon_get_rel("time", -1);
	t = t ? t : 1;

	goto_rc(1, 1);
	set_attribute(GREEN, BLACK, 0);

	/***********************************************/
	/*   /proc/stat handling.		       */
	/***********************************************/
	num_ints = mon_get("stat.intr");
	num_ints2 = mon_get_rel("stat.intr", -1);

	btime = mon_get("stat.btime");
# define	HERTZ	100
	t2 = btime;
	tm = localtime(&t2);

	print_number_string("last pid: %llu in: %3llu load avg: %.2f %.2f %.2f",
		mon_get("loadavg.06"), 
		((num_ints - num_ints2) * 100) / t ,
		(float) mon_get("loadavg") / 100, 
		(float) mon_get("loadavg.02") / 100, 
		(float) mon_get("loadavg.03") / 100);

	if (tm) {
		static char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

		print(" Boot: %d %s %d:%02d",
			tm->tm_mday,
			months[tm->tm_mon], 
			tm->tm_hour, tm->tm_min);
		}
	print("\n");

	if (mon_history()) {
		set_attribute(BLACK, CYAN, 0);
		t2 = mon_get_time() / 100;
		cp = ctime(&t2);
		mvprint(1, screen_width - 19, "%-8.8s", &cp[11]);
		set_attribute(GREEN, BLACK, 0);
		}

	t2 = time(NULL);
	cp = ctime(&t2);
	mvprint(1, screen_width - 9, "%-8.8s", &cp[11]);

	print("\n");

	/***********************************************/
	/*   CPU info.				       */
	/***********************************************/
	print_number_string("CPU: %d%s %s, proc:%d, thr:%d", 
		num_cpus,
		hyperthreaded ? "(HT)" : "",
		cpu_id,
		mon_num_procs, mon_num_threads);
	if (proc_zombie)
		print_number_string(", %d zombies", proc_zombie);
	if (proc_stopped)
		print_number_string(", %d stopped", proc_stopped);
	if (proc_running)
		print_number_string(", %d running", proc_running);
	print(proc_view ? " [proc]" : " [thread]");
	clear_to_eol();

	/***********************************************/
	/*   Compute CPU times.			       */
	/***********************************************/
	for (i = 0; i < MAX_CPU_TIMES; i++) {
		char	buf[BUFSIZ];
		strcpy(buf, "stat.cpu");
		if (i)
			sprintf(buf + strlen(buf), ".%02d", i + 1);
		if (mon_exists(buf) == FALSE)
			break;

		tot += mon_get(buf) - mon_get_rel(buf, -1);
		}

	mvprint(3, 0, "");
	set_attribute(CYAN, BLACK, 0);
	print("%s: ", utsbuf.nodename);
	tot = tot ? tot : 1;
	for (i = 0; i < MAX_CPU_TIMES; i++) {
		char	buf[BUFSIZ];
		strcpy(buf, "stat.cpu");
		if (i)
			sprintf(buf + strlen(buf), ".%02d", i + 1);
		if (mon_exists(buf) == FALSE)
			break;

		t = mon_get(buf) - mon_get_rel(buf, -1);
		if ((float) t / tot == 0)
			continue;
		if (i < MAX_CPU_TIMES)
			print(comma);
		comma = ",";
		print_number_string(" %.1f%% %s",
			(float) (t * 100) / tot,
			cpu_names[i]);
		}
	clear_to_eol();

	/***********************************************/
	/*   Handle the per-cpu stats.		       */
	/***********************************************/
	print("\n");

# define M_THRESH 10000
	print_number_string("RAM:%LuM RSS:%lu%s Free:%lu%s Cached:%lu%s Dirty: %lu%s Swap:%lu%s Free:%lu%s", 
		(main_mem_size / (1024 * 1024)), 
		tot_rss > M_THRESH ? tot_rss / 1024 : tot_rss, tot_rss > M_THRESH ? "M" : "K",
		mem_free > M_THRESH ? mem_free / 1024 : mem_free, mem_free > M_THRESH ? "M" : "K",
		mem_cached > M_THRESH ? mem_cached / 1024 : mem_cached, mem_cached > M_THRESH ? "M" : "K",
		mem_dirty > M_THRESH ? mem_dirty / 1024 : mem_dirty, mem_dirty > M_THRESH ? "M" : "K",
		swap_used > M_THRESH ? swap_used / 1024 : swap_used, swap_used > M_THRESH ? "M" : "K",
		swap_free > M_THRESH ? swap_free / 1024 : swap_free, swap_free> M_THRESH ? "M" : "K"
		);
	if (log_fp) {
		print(" ** LOGGING");
		}
	print("\n");
}
static void
usage()
{
	printf("proc -- show running processes\n");
	printf("Usage: proc [-batch] [-c n] [-rows r] [-cols c] [N]\n");
	printf("\n");
	printf("    -batch     Dont use color or escape sequences\n");
	printf("    -c N       Run at most N times\n");
	printf("    -debugmon  Debug the procmon child.\n");
	printf("    -fast_poll Fast screen refresh (used for debugging).\n");
	printf("    -killmon   Kill any existing child monitoring process.\n");
	printf("    -cols NN   Set screen size to NN columns, rather than autodetect.\n");
	printf("    -rows NN   Set screen size to NN rows, rather than autodetect.\n");
	printf("    -W         Dont save state in $HOME/.proc\n");
	printf("    N          Limit ourselves to N procs\n");
	printf("\n");
	printf("TMPDIR (default /var/tmp)\n");
	printf("    Directory where the proc monitoring files are kept.\n");
	exit(1);
}

/**********************************************************************/
/*   Return username corresponding to UID.			      */
/**********************************************************************/
char *
username(int uid)
{	static char	buf[32];
# define	MAX_PCACHE	64
static struct pcache {
	int	uid;
	int	hits;
	char	*name;
	} cache_tbl[MAX_PCACHE];
	struct pcache tmp;
	struct passwd *pwd;
	int	i;
	int	e = 0;

	if (uid == 0)
		return "root";

	for (i = 0; i < MAX_PCACHE; i++) {
		if (cache_tbl[i].hits == 0)
			e = i;
		if (cache_tbl[i].uid == uid) {
			cache_tbl[i].hits++;
			if (i && cache_tbl[i].hits > cache_tbl[i-1].hits) {
				tmp = cache_tbl[i-1];
				cache_tbl[i-1] = cache_tbl[i];
				cache_tbl[i] = tmp;
				i--;
				}
			return cache_tbl[i].name;
			}
		}

	chk_free_ptr((void *) &cache_tbl[e].name);

	cache_tbl[e].uid = uid;
	cache_tbl[e].hits = 1;
	if ((pwd = getpwuid(uid)) != (struct passwd *) NULL) {
		cache_tbl[e].name = chk_strdup(pwd->pw_name);
		}
	else {
		snprintf(buf, sizeof buf, "#%d", uid);
		cache_tbl[e].name = chk_strdup(buf);
		}
/*{FILE *fp;
fp = fopen("/tmp/proc.log", "a");
fprintf(fp, "adding cache=%d uid=%d buf='%s'\n", e, uid, cache_tbl[e].name);
fclose(fp);
}*/
	return cache_tbl[e].name;
}

void
wait_for_command()
{	fd_set	rbits;
	int	n;
	struct timeval tval;
	char	buf[BUFSIZ];
	char	*cp;
	char	ch;
static int first_time = TRUE;

	goto_rc(5, 0);
	refresh();

	cp = buf;
	while (1) {
		FD_ZERO(&rbits);
		FD_SET(1, &rbits);

		tval.tv_sec = first_time ? 1 : delay_time;
		first_time = FALSE;
		tval.tv_usec = 0;
		if (cp == buf) {
			/***********************************************/
			/*   For debugging.			       */
			/***********************************************/
			if (fast_poll) {
				tval.tv_sec = 0;
				tval.tv_usec = 10000;
				}
			n = select(32, &rbits, (fd_set *) NULL, (fd_set *) NULL, &tval);
			if (n <= 0)
				return;
			}

		if (read(1, &ch, 1) != 1)
			continue;
		if (cp == buf) {
			clear_to_eol();
			out_flush();
			}
		switch (ch) {
		  case 'b' & 0x1f:
		  	mon_move(-1);
		  	return;
		  case 'f' & 0x1f:
		  	mon_move(1);
		  	return;
		  case 'p' & 0x1f:
		  	mon_move(-60);
		  	return;
		  case 'n' & 0x1f:
		  	mon_move(60);
		  	return;
		  case 'r' & 0x1f:
		  	mon_move(0);
		  	return;

		  case '\b':
		  	if (cp == buf)
				break;
			cp--;
			if (write(1, "\b \b", 3) != 3)
				break;
		  	break;
		  case 'u' & 0x1f:
		  	while (cp != buf) {
				cp--;
				if (write(1, "\b \b", 3) != 3)
					break;
				}
		  	break;
		  case '\r':
		  case '\n':
		  	if (cp == buf)
			  	return;
			*cp = '\0';
			graph_finish();
			process_command(buf);
			settings_save();
			return;
		  case 'l' & 0x01f:
		  	last_time = 0;
			clear();
			return;
		  	
		  case ' ':
		  	if (cp == buf)
				return;
			/* Fallthru... */
		  default:
		  	if (ch < ' ' || ch >= 0x7f)
				continue;
		  	if (cp < buf + sizeof buf - 1) {
				*cp++ = ch;
				if (write(1, &ch, 1) != 1)
					break;
				}
			break;
		  }
		}
}
void
win_handler()
{
	need_resize = TRUE;
	signal(SIGWINCH, win_handler);
}


