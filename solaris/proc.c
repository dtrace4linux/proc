/* $Header: Last edited: 21-Nov-2002 1.1 $ 			      */
# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<sys/sysconfig.h>
# include	<sys/stat.h>
# include	<sys/sysinfo.h>
# include	<sys/utsname.h>
# include	<kstat.h>
# include	<dirent.h>
# include	<stdarg.h>
# include	<pwd.h>
# include	<sys/proc.h>
# include	<sys/swap.h>
# include	<sys/file.h>
# include	<sys/fs/fifonode.h>
# include	<sys/socket.h>
# include	<netinet/in.h>
# include	<netdb.h>
# include	<kvm.h>


struct columns {
	char	*name;
	char	*fmt;
	int	width;
	int	flags;
	} columns[] = {
	{"USERNAME", "%-8s", 8},
	{"EVENT   ", "%-8s", 8},
	{" %CPU", "%-8s", 8},
	{" %MEM", "%-8s", 8},
	{"SIZE/", "%-8s", 8},
	{"RSS  ", "%-8s", 8},
	{" TIME", "%-8s", 8},
	{"PID  ", "%-8s", 8},
	{"COMMAND", "%-8s", 8},
	{0}
	};

struct utsname	utsbuf;

int	kstat_level = 0;
char	*kstat_mask;

int	pagesize;
int	sort_order = -1;
int	sort_type = SORT_NORMAL;
kstat_ctl_t	*kc;
int	proc_id;	/* Process ID to display in detail. */
int	quit_flag;
int	need_resize;
int	display_mode = DISPLAY_PS;

static int	proc_zombie;
static int	proc_running;
static int	proc_stopped;

time_t	time_now;
time_t	last_time;
procinfo_t	*pinfo;
int	num_procs;

extern int rows;
/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
long	get_counter(char *, char *, long *);
long	get_kstat(char *, char *);
void	stop_handler();
void	win_handler();
int	process_command(char *);
void	wait_for_command(void);
char	*username(int);
char	*event_name(unsigned long);
int	sort_normal();
void	sort_procs(procinfo_t *pinfo, int num_procs);
int	do_switches(int, char **);
void	main_loop(void);
void	update_headers(void);
void	usage(void);
void	print_vnode(long);
int	print_addr(long);

int
main(int argc, char **argv)
{

	pagesize = _sysconfig(_CONFIG_PAGESIZE);
	uname(&utsbuf);

	load_nlist();

	chdir("/proc");

	kc = kstat_open();
	if (kc == (kstat_ctl_t *) NULL) {
		perror("kstat_open");
		exit(1);
		}

	init_termcap();
	init_display();
	init_screen();

	/***********************************************/
	/*   Clear to end of line when we see a \n.    */
	/***********************************************/
	dsp_crmode = TRUE;

	signal(SIGWINCH, win_handler);
	signal(SIGTSTP, stop_handler);

	main_loop();

	end_screen();

	return 0;
}
/**********************************************************************/
/*   Show kstat types.						      */
/**********************************************************************/
static int
sort_kstat(p1, p2)
char	**p1;
char	**p2;
{
	return strcmp(*p1, *p2);
}
void
display_kstat()
{	kstat_t	*ksp;
	char	buf[BUFSIZ];
	int	i;
	int	chain_len = 0;
	char	**names;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next)
		chain_len++;

	names = (char **) chk_alloc(sizeof(char *) * chain_len);
	for (i = 0, ksp = kc->kc_chain; ksp; ksp = ksp->ks_next, i++) {
		sprintf(buf, "%.*s", KSTAT_STRLEN, ksp->ks_module);
		if (kstat_level >= 1)
			sprintf(buf + strlen(buf), ".%.*s", KSTAT_STRLEN, ksp->ks_name);
		if (kstat_level >= 2)
			sprintf(buf + strlen(buf), ".%.*s", KSTAT_STRLEN, ksp->ks_class);
		names[i] = chk_strdup(buf);
		}

	/***********************************************/
	/*   Sort items into order.		       */
	/***********************************************/
	qsort(names, chain_len, sizeof names[0], sort_kstat);
	for (i = 0; i < chain_len; i++) {
		if (i != 0 && strcmp(names[i], names[i-1]) == 0)
			continue;
		if (kstat_mask && 
		    strncmp(names[i], kstat_mask, strlen(kstat_mask)) != 0)
			continue;
		print("%s\n", names[i]);
		}
	chk_free(names);
	clear_to_end_of_screen();
}
void
display_proc()
{	struct proc *pp;
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
}
/**********************************************************************/
/*   Process display mode.					      */
/**********************************************************************/
void
display_ps()
{	int	i, j, t;
	int	len, max_len;
	int	last_uid;
	int	tot_rss;
	char	*cmd;
	char	*cp;
	procinfo_t	*pip;
	int	nswap;
	char	buf[BUFSIZ];
	int	pgsize = pagesize;
	float	f;
static	swaptbl_t *swaptbl;

# define	MAX_SWAPS	8

# if _STRUCTURED_PROC
	pgsize = 1024;
# endif
	if (swaptbl == (swaptbl_t *) NULL) {
		swaptbl = malloc(sizeof(int) + sizeof(struct swapent) * MAX_SWAPS);
		for (i = 0; i < MAX_SWAPS; i++) {
			swaptbl->swt_ent[i].ste_path = malloc(64);
			}
		}

	proc_running = proc_zombie = proc_stopped = 0;

	set_attribute(YELLOW, BLACK, 0);
	for (j = 0; columns[j].name; j++) {
		print("%s ", columns[j].name);
		}
	print("\n");

	last_uid = -1;
	tot_rss = 0;

	for (i = 0, pip = pinfo; i < num_procs && csr_y < screen_length; i++, pip++) {
		set_attribute(GREEN, BLACK, 0);

		if (pip->pi_psinfo.pr_uid == last_uid)
			print("  \"  \"   ");
		else {
			last_uid = pip->pi_psinfo.pr_uid;
			cp = username(pip->pi_psinfo.pr_uid);
			print("%-8s ", cp);
			}
		if (pip->pi_flags & PI_PSINFO_VALID) {
			set_attribute(WHITE, BLACK, 0);

# if _STRUCTURED_PROC
/*print("wchan=%x/", pip->pi_psinfo.pr_lwp.pr_wchan);
print("fl=%02x:%02x:", pip->pi_psinfo.pr_flag, pip->pi_psinfo.pr_lwp.pr_flag);
*/
			if (pip->pi_psinfo.pr_flag & PR_STOPPED) {
				if (pip->pi_psinfo.pr_flag & PR_ISSYS)
					print("System   ");
				else {
					set_attribute(CYAN, BLACK, 0);
					print("Stopped  ");
					proc_stopped++;
					}
				}
			else if (pip->pi_psinfo.pr_lwp.pr_wchan ||
			       (pip->pi_psinfo.pr_lwp.pr_flag & PR_ISTOP)) {
				if (pip->pi_psinfo.pr_lwp.pr_wchan == 0)
					print("Sleeping ");
				else
					print("%s ", event_name((unsigned long) pip->pi_psinfo.pr_lwp.pr_wchan));
				}
			else {
				set_attribute(GREEN, BLACK, 0);
				print("CPU#%d    ", 
					pip->pi_psinfo.pr_lwp.pr_onpro);
				set_attribute(WHITE, BLACK, 0);
				proc_running++;
				}
# else
			switch (pip->pi_psinfo.pr_state) {
			  case SSLEEP:
				print("%s ", event_name((unsigned long) pip->pi_psinfo.pr_wchan));
				break;
			  case SRUN:
				set_attribute(GREEN, BLACK, 0);
			  	print("Running  ");
				proc_running++;
				break;
			  case SZOMB:
				print(" zombie  ");
				proc_zombie++;
				break;
			  case SSTOP:
				print("Stopped  ");
				proc_stopped++;
				break;
			  case SIDL:
				print("Idle     ");
				break;
			  case SONPROC:
				set_attribute(GREEN, BLACK, 0);
				print("<onproc> ");
				proc_running++;
				break;
			  }
# endif

			f = ((float) pip->pi_psinfo.pr_pctcpu / 0x8000) * 100.0;
			if (f > 0.05)
				set_attribute(CYAN, BLACK, 0);
			print("%5.2f ", f);
			set_attribute(WHITE, BLACK, 0);

			print("%5.2f ",
				((float) pip->pi_psinfo.pr_pctmem / 0x8000) * 100.0);

			t = (pip->pi_psinfo.pr_size * pgsize) / 1024;
			if (t > 99999) {
				set_attribute(YELLOW, BLACK, 0);
				print("%4ldM", t / 1024);
				set_attribute(WHITE, BLACK, 0);
				}
			else
				print("%5ld", t);
			t = (pip->pi_psinfo.pr_rssize * pgsize) / 1024;
			if (t > 99999) {
				char buf[BUFSIZ];
				sprintf(buf, "%ldM", t / 1024);
				set_attribute(YELLOW, BLACK, 0);
				print("/%-5s ", buf);
				set_attribute(WHITE, BLACK, 0);
				}
			else
				print("/%-5ld ", t);
			tot_rss += (pip->pi_psinfo.pr_rssize * pgsize) / 1024;

			print("%2d:%02d ", 
				pip->pi_psinfo.pr_time.tv_sec / 60,
				pip->pi_psinfo.pr_time.tv_sec % 60);
			print("%-5d ", pip->pi_pid);
			set_attribute(CYAN, BLACK, 0);

			cmd = pip->pi_psinfo.pr_psargs;
			max_len = screen_width - dst_x;
			len = (int) strlen(cmd);
				
			if (len > max_len) {
				char *cp = cmd; 
				while (*cp && *cp != ' ')
					cp++;
				if (*cp == ' ') {
					while (cp > cmd && *cp != '/')
						cp--;
					if (*cp == '/')
						cp++;
					cmd = cp;
					}
				len = strlen(cmd);
				}
			if (len > max_len)
				len = max_len;
			for (j = 0; j < len; j++) {
				int	ch = cmd[j];
				if (dst_x >= screen_width - 1)
					break;
				if (ch == '\0')
					break;
				if (ch < ' ')
					print(".");
				else
					print("%c", ch);
				}
			}
		else {
			print("%dK %-5d ", 
				(pip->pi_psinfo.pr_size * pagesize) / 1024,
				pip->pi_pid);
			set_attribute(YELLOW, BLACK, 0);
			print("<EPERM>");
			}
		print("\n");
		}

	goto_rc(4, 0);
	print_number_string("Memory: RSS: %dK", tot_rss);
	/***********************************************/
	/*   Do swap info.			       */
	/***********************************************/
	swaptbl->swt_n = MAX_SWAPS;
	if ((nswap = swapctl(SC_LIST, swaptbl)) > 0) {
		int	sw_used = 0;
		int	sw_free = 0;
		for (i = 0; i < nswap; i++) {
			sw_used += swaptbl->swt_ent[i].ste_pages;
			sw_free += swaptbl->swt_ent[i].ste_free;
			}
		sw_used *= pagesize / 1024;
		sw_free *= pagesize / 1024;
		if ((sw_free * 100) / sw_used < 20)
			set_attribute(RED, YELLOW, 0);
		print_number_string(" Swap(%d): %dK used, %dK free (%d%%)\n", 
			nswap,
			sw_used - sw_free,
			sw_free,
			(sw_free * 100) / sw_used
			);
		set_attribute(GREEN, BLACK, 0);
		}
	else
		print(" swapctl=%d (errno=%d)", nswap, errno);
	mvprint(5, 0, "");
}
/**********************************************************************/
/*   Try  and  convert  a  wait  channel  to the name of a nice data  */
/*   structure.                                                       */
/**********************************************************************/
char *
event_name(unsigned long wchan)
{	static char buf[16];
	kstat_t	*ksp;

	if (wchan == 0)
		return "Running ";
	/***********************************************/
	/*   Walk  the  kstats  chain  looking  for a  */
	/*   named block.			       */
	/***********************************************/
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (wchan >= (unsigned long) ksp->ks_private &&
		    wchan < (unsigned long) ksp->ks_private + ksp->ks_data_size) {
			sprintf(buf, "%-.8s", ksp->ks_name);
			return buf;
			}
		}
	sprintf(buf, "%08lx", wchan);
	return buf;
}
/**********************************************************************/
/*   Get kernel statistics and tell us the rate/sec.		      */
/**********************************************************************/
long
get_counter(name, subname, old_addr)
char	*name;
char	*subname;
long	*old_addr;
{	time_t	t = time_now - last_time;
	long	v;
	long	vnew;
	
	if (t == 0)
		t = 1;
	v = get_kstat(name, subname);

	if (*old_addr == 0)
		*old_addr = v;

	vnew = (v - *old_addr) / t;
	*old_addr = v;
	return vnew;
}
long
get_kstat(name, subname)
char	*name;
char	*subname;
{	kstat_t	*ksp;
	kstat_named_t	*namep;
	char	*n1, *n2, *n3;
	char	buf[BUFSIZ];
	int	n;

	n1 = name;
	n2 = strchr(n1, '.') + 1;
	n3 = strchr(n2, '.') + 1;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (ksp->ks_module[0] != n1[0] ||
		    ksp->ks_name[0] != n2[0] ||
		    ksp->ks_class[0] != n3[0])
			continue;
		sprintf(buf, "%.*s.%.*s.%.*s",
			KSTAT_STRLEN, ksp->ks_module,
			KSTAT_STRLEN, ksp->ks_name,
			KSTAT_STRLEN, ksp->ks_class);
		if (strcmp(buf, name) == 0)
			break;
		}
	if (ksp == (kstat_t *) NULL)
		return -1;
	/***********************************************/
	/*   Found  the  table,  now  find the actual  */
	/*   value.				       */
	/***********************************************/
  	kstat_read(kc, ksp, (void *) NULL);
	namep = (kstat_named_t *) ksp->ks_data;
	for (n = 0; n++ < (int) ksp->ks_ndata; namep++) {
		if (strcmp(namep->name, subname) == 0)
			break;
		}
	if (n - 1 >= (int) ksp->ks_ndata)
		return -2;
	/***********************************************/
	/*   Got the element. Hope its integral.       */
	/***********************************************/
	switch (namep->data_type) {
	  case KSTAT_DATA_CHAR:
	  	return (long) namep->value.c;
	  case KSTAT_DATA_LONG:
	  	return namep->value.l;
	  case KSTAT_DATA_ULONG:
	  	return namep->value.ul;
	  case KSTAT_DATA_LONGLONG:
	  	return namep->value.l;
	  case KSTAT_DATA_ULONGLONG:
	  	return namep->value.ul;
	  case KSTAT_DATA_FLOAT:
	  	return namep->value.f;
	  case KSTAT_DATA_DOUBLE:
	  	return namep->value.d;
	  }
	return -3;
}
void
main_loop(void)
{
	int	i, j, t;

	while (!quit_flag) {
		kstat_chain_update(kc);

		if (need_resize) {
			need_resize = FALSE;
			init_display();
			screen_length = rows;
			}

		pinfo = proc_get_proclist(&num_procs);
		sort_procs(pinfo, num_procs);

		last_time = time_now;
		time_now = time((time_t *) NULL);


		mvprint(6, 0, "");

		switch (display_mode) {
		  case DISPLAY_KSTAT:
		  	display_kstat();
			break;
		  case DISPLAY_PROC:
		  	display_proc();
			break;
		  case DISPLAY_PS:
		  	display_ps();
			break;
		  case DISPLAY_CMD:
		  	shell_command((char *) NULL);
			break;
		  }

		update_headers();

		chk_free((void *) pinfo);
		out_flush();

		wait_for_command();
		}
}
void
print_vnode(addr)
long	addr;
{	char	*cp;
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
}
# define	CACHE_SIZE	8
int
print_addr(l)
long	l;
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
		sprintf(buf, "%d.%d.%d.%d", 
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
/**********************************************************************/
/*   Function to return an array of processes running on the system.  */
/**********************************************************************/
procinfo_t *
proc_get_proclist(nump)
int	*nump;
{	int	pused;
	int	psize;
	procinfo_t	*parray;
	procinfo_t	*pip;
	char	buf[BUFSIZ];
	int	i;
static	DIR	*dirp;
	struct dirent *de;
	struct stat sbuf;

	pused = 0;
	psize = 128;
	parray = (procinfo_t *) chk_alloc(sizeof(procinfo_t) * psize);

	if (dirp == (DIR *) NULL) {
		dirp = opendir("/proc");
		if (dirp == (DIR *) NULL) {
			error_message("Cannot open /proc.");
			goto end_of_function;
			}
		}
	else
		rewinddir(dirp);

	/***********************************************/
	/*   Read all the process id's in /proc.       */
	/***********************************************/
	while ((de = readdir(dirp)) != (struct dirent *) NULL) {
		if (!isdigit(de->d_name[0]))
			continue;
		pip = &parray[pused++];
		pip->pi_pid = atoi(de->d_name);
		pip->pi_flags = 0;
		memset((char *) &pip->pi_psinfo, 0, sizeof pip->pi_psinfo);
# if defined(PIOCPSINFO)
		if ((pip->pi_fd = open(de->d_name, O_RDONLY)) >= 0) {
			if (ioctl(pip->pi_fd, PIOCPSINFO, &pip->pi_psinfo) >= 0)
				pip->pi_flags = PI_PSINFO_VALID;
			close(pip->pi_fd);
			}
# endif
		if ((pip->pi_flags & PI_PSINFO_VALID) == 0) {
			snprintf(buf, sizeof buf, "/proc/%s/psinfo", de->d_name);
			if ((pip->pi_fd = open(buf, O_RDONLY)) >= 0) {
				read(pip->pi_fd, &pip->pi_psinfo, sizeof pip->pi_psinfo);
				pip->pi_flags = PI_PSINFO_VALID;
				close(pip->pi_fd);
				}
			}

		/***********************************************/
		/*   Handle graceful failure.		       */
		/***********************************************/
		if ((pip->pi_flags & PI_PSINFO_VALID) == 0) {
			if (stat(de->d_name, &sbuf) >= 0) {
				pip->pi_psinfo.pr_size = sbuf.st_size / pagesize;
				pip->pi_psinfo.pr_uid = sbuf.st_uid;
				}
			}

		/***********************************************/
		/*   Check  theres  enough  room for the next  */
		/*   entry.				       */
		/***********************************************/
		if (pused >= psize) {
			psize += 128;
			parray = (procinfo_t *) chk_realloc((void *) parray,
				sizeof(procinfo_t) * psize);
			}
		}

end_of_function:
	*nump = pused;
	return parray;
}
/**********************************************************************/
/*   Sort processes into interesting order.			      */
/**********************************************************************/
int
sort_pid(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p2->pi_psinfo.pr_pid - p1->pi_psinfo.pr_pid) != 0)
		return sort_order * t;
	return sort_normal(p1, p2);
}
int
sort_normal(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p1->pi_psinfo.pr_pctcpu - p2->pi_psinfo.pr_pctcpu) != 0)
		return sort_order * t;
	return sort_order * (p1->pi_psinfo.pr_pid - p2->pi_psinfo.pr_pid);
}
int
sort_size(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p2->pi_psinfo.pr_size - p1->pi_psinfo.pr_size) != 0)
		return sort_order * t;
	return sort_normal(p1, p2);
}
int
sort_rss(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
{	int	t;

	if ((t = p2->pi_psinfo.pr_rssize - p1->pi_psinfo.pr_rssize) != 0)
		return sort_order * t;
	return sort_size(p1, p2);
}
int
sort_user(p1, p2)
procinfo_t	*p1;
procinfo_t	*p2;
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

	kill(getpid(), SIGTSTP);

	signal(SIGTSTP, stop_handler);
	clear();
	reinit_screen();
}
/**********************************************************************/
/*   Return username corresponding to UID.			      */
/**********************************************************************/
char *
username(int uid)
{	static char	buf[32];
# define	MAX_PCACHE	24
static struct pcache {
	int	uid;
	int	hits;
	char	*name;
	} cache_tbl[MAX_PCACHE];
	struct pcache tmp;
	struct passwd *pwd;
	int	i;

	if (uid == 0)
		return "root";

	for (i = 0; i < MAX_PCACHE; i++) {
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

	if ((pwd = getpwuid(uid)) != (struct passwd *) NULL) {
		if (cache_tbl[MAX_PCACHE-1].name)
			chk_free(cache_tbl[MAX_PCACHE-1].name);
		cache_tbl[MAX_PCACHE-1].uid = uid;
		cache_tbl[MAX_PCACHE-1].hits = 1;
		cache_tbl[MAX_PCACHE-1].name = chk_strdup(pwd->pw_name);
		return cache_tbl[MAX_PCACHE-1].name;
		}
	sprintf(buf, "#%d", uid);
	return buf;
}
void
update_headers()
{	kstat_t	*ksp;
	cpu_stat_t	*cpup = (cpu_stat_t *) NULL;
static cpu_stat_t	last_cpu;
static char *cpu_state_names[] = {
	"idle", "user", "kernel", /*"wait" */ (char *) NULL
	};
static char *cpu_wait_names[] = {
	"wio", "wswap", "wpio", (char *) NULL
	};
	int	c, i, j;
	int	t;
	char	*comma;
	time_t	curr_time;
	kstat_named_t	*kn, *kn1;
	char	buf[BUFSIZ];
static long old_fault;
static long old_nproc;

	t = time_now - last_time;

	kstat_chain_update(kc);
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (ksp->ks_module[0] == 'c' &&
		    strcmp(ksp->ks_name, "cpu_stat0") == 0 &&
		    strcmp(ksp->ks_module, "cpu_stat") == 0 &&
		    ksp->ks_type == KSTAT_TYPE_RAW) {
		  	kstat_read(kc, ksp, (void *) NULL);
		    	cpup = (cpu_stat_t *) ksp->ks_data;
		    	}
		}

	goto_rc(1, 1);
	set_attribute(GREEN, BLACK, 0);
	print("last proc: ");
	set_attribute(YELLOW, BLACK, 0);
	print("%d ", get_ksym("*mpid"));

	set_attribute(GREEN, BLACK, 0);
	print("flt:");
	set_attribute(YELLOW, BLACK, 0);
	print("%d", get_counter("unix.segmap.vm", "fault", &old_fault));

	/***********************************************/
	/*   Load average.			       */
	/***********************************************/
	ksp = kstat_lookup(kc, "unix", 0, "system_misc");
	kstat_read(kc, ksp, 0);
	if ((kn = kstat_data_lookup(ksp, "avenrun_1min")) != (kstat_named_t *) NULL) {
		set_attribute(GREEN, BLACK, 0);
		print(" load avg: ");
		print_number_string("%.2f", (double) kn->value.ul/FSCALE);
		}
	if ((kn = kstat_data_lookup(ksp, "avenrun_5min")) != (kstat_named_t *) NULL) {
		print_number_string(", %.2f", (double) kn->value.ul/FSCALE);
		}
	if ((kn = kstat_data_lookup(ksp, "avenrun_15min")) != (kstat_named_t *) NULL) {
		print_number_string(", %.2f", (double) kn->value.ul/FSCALE);
		}
	print("        ");

	curr_time = time((time_t *) NULL);
	mvprint(1, screen_width - 9, "%-8.8s", &(ctime(&curr_time)[11]));

	print("\n");

	/***********************************************/
	/*   Get CPU info.			       */
	/***********************************************/
	if ((kn = kstat_data_lookup(ksp, "ncpus")) != (kstat_named_t *) NULL) {
		int	num_cpus = kn->value.l;
		print_number_string("cpus:%d (", kn->value.l);
		for (c = 0; c < num_cpus; c++) {
			sprintf(buf, "cpu_info%d", c);
			ksp = kstat_lookup(kc, "cpu_info", 0, buf);
			if (ksp == NULL)
				continue;
			kstat_read(kc, ksp, 0);
			kn = kstat_data_lookup(ksp, "cpu_type");
			kn1 = kstat_data_lookup(ksp, "clock_MHz");
			if (kn == NULL || kn1 == NULL)
				continue;
			if (c)
				print("+");
			print("%d MHz %s", kn1->value.l, kn->value.c);
			}
		print(")");
		}
	else
		print_number_string("cpus:?");
	print_number_string(", procs: %d", num_procs);
	if (proc_zombie)
		print_number_string(", zombies: %d", proc_zombie);
	if (proc_stopped)
		print_number_string(", stopped: %d", proc_stopped);
	if (proc_running)
		print_number_string(", running: %d", proc_running);

	mvprint(3, 0, "");
	set_attribute(CYAN, BLACK, 0);
	print("%s: ", utsbuf.nodename);
	set_attribute(GREEN, BLACK, 0);
	if (cpup && t) {
		comma = "";
		for (i = 0; cpu_state_names[i]; i++) {
			j = (cpup->cpu_sysinfo.cpu[i] - last_cpu.cpu_sysinfo.cpu[i]) / t;
			print("%s%d%% %s", comma, j, cpu_state_names[i]);
			comma = ", ";
			}
		for (i = 0; cpu_wait_names[i]; i++) {
			j = (cpup->cpu_sysinfo.wait[i] - last_cpu.cpu_sysinfo.wait[i]) / t;
			if (j) {
				print("%s%d%% %s", comma, j, cpu_wait_names[i]);
				comma = ", ";
				}
			}
		print("\n");
		}
	if (cpup)
		last_cpu = *cpup;
}
void
wait_for_command()
{	fd_set	rbits;
	int	n;
	struct timeval tval;
	char	buf[BUFSIZ];
	char	*cp;
	char	ch;

	goto_rc(5, 0);
	refresh();

	cp = buf;
	while (1) {
		FD_ZERO(&rbits);
		FD_SET(1, &rbits);

		tval.tv_sec = 5;
		tval.tv_usec = 0;
		n = select(32, &rbits, (fd_set *) NULL, (fd_set *) NULL, &tval);
		if (n <= 0)
			return;

		if (read(1, &ch, 1) != 1)
			continue;
		switch (ch) {
		  case '\b':
		  	if (cp == buf)
				break;
			cp--;
			write(1, "\b \b", 3);
		  	break;
		  case 'u' & 0x1f:
		  	while (cp != buf) {
				cp--;
				write(1, "\b \b", 3);
				}
		  	break;
		  case '\r':
		  case '\n':
		  	if (cp == buf)
			  	return;
			*cp = '\0';
			process_command(buf);
			return;
		  case 'l' & 0x01f:
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
				write(1, &ch, 1);
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

