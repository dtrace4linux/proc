/*# include	"machine.h"*/
# include	<stdio.h>
# include	<ctype.h>
# include	<limits.h>
# include	<string.h>
# include	<unistd.h>
# include 	<signal.h>
# include	<errno.h>
# include	<malloc.h>
# include	<fcntl.h>
# include	<stdlib.h>
# include	<time.h>
# include	<sys/types.h>
# include 	<sys/stat.h>
# include 	<sys/wait.h>
# include	<sys/syscall.h>
# include	<include.h>
# include	"graph.h"

# undef putchar
# define	putchar(x) do {char ch = x; fflush(stdout); write(1, &ch, 1);} while (0)

/**********************************************************************/
/*   Following  macro  used  to  allow  compilation  with ANSI C and  */
/*   non-ANSI C compilers automatically.			      */
/**********************************************************************/
# if !defined(PROTO)
#	if	defined(__STDC__)
#		define	PROTO(x)	x
#	else
#		define	PROTO(x)	()
#	endif
# endif

# if !defined(TRUE)
#	define	TRUE	1
#	define	FALSE	0
# endif

# define UNUSED_PARAMETER(x)

/**********************************************************************/
/*   CPU states.						      */
/**********************************************************************/
# define	MAX_CPU_TIMES 10

/**********************************************************************/
/*   Sorting orders for user display.				      */
/**********************************************************************/
# define	SORT_NORMAL	0
# define	SORT_PID	1
# define	SORT_USER	2
# define	SORT_SIZE	3
# define	SORT_TIME	4
# define	SORT_RSS	5
# define	SORT_CLOCK	6

/**********************************************************************/
/*   Display modes.						      */
/**********************************************************************/
enum {
	DISPLAY_CMD,
	DISPLAY_CPU,
	DISPLAY_DISK,
	DISPLAY_FILES,
	DISPLAY_GRAPHS,
	DISPLAY_ICMP,
	DISPLAY_IFCONFIG,
	DISPLAY_INTERRUPTS,
	DISPLAY_IP,
	DISPLAY_KSTAT,
	DISPLAY_MEMINFO,
	DISPLAY_NETSTAT,
	DISPLAY_PROC,
	DISPLAY_PS,
	DISPLAY_SOFTIRQS,
	DISPLAY_TEMPERATURE,
	DISPLAY_TCP,
	DISPLAY_UDP,
	DISPLAY_VMSTAT,
	};

/**********************************************************************/
/*   Structure to describe a process on the system.		      */
/**********************************************************************/
# define	PI_PSINFO_VALID		0x01
# define	PI_IS_THREAD		0x02
typedef struct procinfo_t {
	int		pi_flags;
	pid_t		pi_pid;
	int		pi_fd;
	char		*pi_cmdline;
	prpsinfo_t	pi_psinfo;
	char		pi_name[10];
	} procinfo_t;

/**********************************************************************/
/*   General structure for mapping strings to int's or vice versa.    */
/**********************************************************************/
struct map {
	char	*name;
	long	number;
};
/**********************************************************************/
/*   Info about columns.					      */
/**********************************************************************/
# define	COL_HIDDEN	0x01
struct columns {
	char	*name;
	char	*fmt;
	int	width;
	int	flags;
	};
extern struct columns columns[];

# define MODE_ABS	0
# define MODE_COUNT	1
# define MODE_DELTA	2
# define MODE_RANGED	3
extern int mode;
extern int mode_clear;

struct cpuinfo_t {
	char	cpu_mhz[64];
	char	cache_size[64];
	char	bogomips[64];
	} *cpuinfo;

/**********************************************************************/
/*   netstat/socket into.					      */
/**********************************************************************/
typedef struct socket_t {
	time_t		s_time;
	unsigned long	l_ip;
	unsigned int	l_port;
	unsigned long	r_ip;
	unsigned int	r_port;
	unsigned int	state;
	unsigned int	rcvwin;
	unsigned int	sndwin;
	int		uid;
	unsigned long	inode;
	} socket_t;

int mon_read_netstat(int rel, socket_t **tblp);

/**********************************************************************/
/*   Externs.							      */
/**********************************************************************/
extern int 	agg_mode;
extern char switches[26];
extern int 	proc_view;
extern int	display_mode;
extern int	kstat_level;
extern char	*kstat_mask;
extern int	quit_flag;
extern int	sort_order;
extern int	sort_type;
extern FILE 	*log_fp;
extern char	*grep_filter;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
int grep_compare(char *str);
void	*chk_zalloc(size_t);
procinfo_t	*proc_get_proclist(int *);
void	error_message(char *, ...);
void	message(char *, ...);
void	wait_for_key(void);
int	load_nlist();
long	get_ksym(char *);
int	get_struct(long, void *, int);
char	*comma(unsigned long);
int	mon_find(char *, int *);
char	*mon_name(int);
int	mon_exists(char *name);
int	mon_history(void);
int	mon_tell(void);
unsigned long long mon_get_item(int, int);
unsigned long long mon_get(char *);
unsigned long long mon_get_rel(char *name, int rel);
unsigned long long mon_get_time(void);
void	mon_lock(void);
void	mon_unlock(void);
void	mon_move(int arg);
int	mon_read_procs(void);
void	monitor_start(void);
void print_number(int idx, int width, int is_signed, unsigned long long v, unsigned long long v0);
void print__number(int idx, int width, int is_signed, int percent,
	unsigned long long v, unsigned long long v0, unsigned long divisor);
void print_number_string(char *str, ...);
void	shell_command(char *cmd);
char	*chk_realloc(void *, int);
char	*chk_strdup(char *);
char	*chk_alloc(int);
procinfo_t * raw_proc_get_proclist(int *nump);
char	*syscall_name(int);
int diff_time(struct timeval *tnow, struct timeval *tlast);
void draw_graph(graph_t *g, int flags, char *item, int x, int y, int width, int height, double scale, int bg_color);
graph_t * graphs_new(void);
void	graph_finish(void);
void	graph_refresh(graph_t *g);
char *mon_dir(void);
time_t mon_is_stale(void);
void print_number3(char *buf, unsigned long long n, unsigned long long n0);
void monitor_uninit(void);
int read_syscall(int pid, int uid);
void	display_TCP(void);
void	display_UDP(void);
void	display_ICMP(void);
void	display_IP(void);
void	display_cpu(void);
void	display_disk(void);
void	display_graphs(void);
void	display_ifconfig(void);
void	display_interrupts(void);
void	display_meminfo(void);
void	display_netstat(void);
void	display_softirqs(void);
void	display_temperature(void);
void	display_vmstat(void);
char * read_file(char *path, char *name);
char * read_file2(char *path, char *name);
void error(char *str);
void	chk_free(void *);
void	chk_free_ptr(void **);
char	*username(int);

