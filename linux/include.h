# include	<sys/time.h>

typedef struct pinfo_t {
	int	pr_pid;
	} pinfo_t;
typedef struct prpsinfo_t {
	int	pr_isnew;
	char	pr_state;
	int	pr_pid;
	int	pr_ppid;
	int	pr_pgrp;
	int	pr_session;
	int	pr_tty;
	int	pr_tpgrp;
	int	pr_flags;
	unsigned long pr_min_flt;
	unsigned long pr_cmin_flt;
	unsigned long pr_maj_flt;
	unsigned long pr_cmaj_flt;
# define	NUM_PCTCPU	20
	unsigned pr_last_pctcpu[NUM_PCTCPU];
	unsigned pr_last_utime;
	unsigned pr_last_stime;
	unsigned pr_last_size;
	unsigned long long pr_last_vsize;
	unsigned pr_last_rssize;
	unsigned long pr_utime;
	unsigned long pr_stime;
	unsigned long pr_cutime; 
	unsigned long pr_cstime; 
	int	pr_priority; 
	int	pr_nice; 
	int	pr_num_threads;
	int	pr_it_real_value;
	int	pr_start_time;
	unsigned long long pr_vsize;
	unsigned long pr_pctcpu;
	unsigned long pr_pctcpu0;
	unsigned long pr_size;
	unsigned long pr_rssize;
	unsigned long pr_rss_rlim;
	unsigned long pr_start_code;
	unsigned long pr_end_code;
	unsigned long pr_start_stack;
	unsigned long pr_kstk_esp;
	unsigned long pr_kstk_eip;
	unsigned long long pr_wchan;
	unsigned long pr_nswap;
	unsigned long pr_cnswap;
	int	pr_lproc;
	int	pr_uid;
	struct timeval	pr_tlast;
	struct timeval	pr_tnow;
	struct timeval pr_start;
	struct timeval pr_time;

	unsigned pr_rt_priority;
	unsigned pr_policy;
	unsigned long long pr_delayacct_blkio_ticks;
	unsigned long pr_guest_time;
	unsigned long pr_cguest_time;

	int	pr_syscall;

	} prpsinfo_t;
