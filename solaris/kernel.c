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
# include	<kvm.h>

/**********************************************************************/
/*   Following  structure  is  used  to  keep  track  of  the memory  */
/*   addresses of certain variables in kernel memory.		      */
/**********************************************************************/
# define	VF_DIRTY	0x01	/* Need to fetch from memory. */
struct var_info {
	char	*name;
	int	*addr;
	int	flags;

	char	*bufs[2];	/* Used to cache contents from /dev/mem */
	int	cur_len;
	char	*mmap_addr;
	};

# define	ENTRY(name)	{name}
# define	ARRAY(name)	{name}

struct var_info var_tbl[] = {
	ENTRY("*allstream"),
	ENTRY("anoninfo"),
	ENTRY("async_bufhead"),
	ENTRY("availrmem"),
	ENTRY("availsmem"),
	ENTRY("avenrun"),
	ENTRY("cdevsw"),
	ENTRY("*cdevcnt"),
	ENTRY("cnt"),
	ENTRY("cp_time"),
	ENTRY("dinfo"),
	ENTRY("dk_bps"),
	ENTRY("*dk_ndrive"),
	ENTRY("dk_read"),
	ENTRY("dk_seek"),
	ENTRY("dk_time"),
	ENTRY("dk_wds"),
	ENTRY("dk_xfer"),
	ENTRY("fifo_alloc"),
	ENTRY("fifoinfo"),
	ENTRY("file"),
	ENTRY("freemem"),
	ENTRY("*hz"),
	ENTRY("icmpstat"),
	ENTRY("ifnet"),
	ENTRY("ifstats"),
	ENTRY("inode"),
	ENTRY("ipstat"),
	ENTRY("maxmem"),
	ENTRY("mbutl"),
	ENTRY("minfo"),
	ENTRY("mpid"),
	ENTRY("msginfo"),
	ENTRY("*msgmap"),
	ENTRY("*ncache"),
	ENTRY("*ncsize"),
	ENTRY("ncstats"),
	ENTRY("*nmblock"),
	ENTRY("*nproc"),
	ENTRY("*nstrbufsz"),
	ENTRY("numcpu"),
	ENTRY("pgstat"),
	ENTRY("pollwait"),
	ENTRY("practive"),
	ENTRY("*pregpp"),
	ENTRY("proc"),
	ENTRY("rate"),
	ARRAY("rbsize"),
	ENTRY("region"),
	ENTRY("rtstat"),
	ENTRY("runin"),
	ENTRY("runout"),
	ENTRY("selwait"),
	ARRAY("strbufsizes"),
	ENTRY("Strinfo"),
	ENTRY("*strbufstat"),
	ENTRY("streams"),
	ENTRY("strst"),
	ENTRY("strthresh"),
	ENTRY("Strcount"),
	ENTRY("Streamstat"),
	ENTRY("sum"),
	ENTRY("Sysbase"),
	ENTRY("Syslimit"),
	ENTRY("sysinfo"),
	ENTRY("tcpcb"),
	ENTRY("tcpstat"),
	ENTRY("total"),
	ENTRY("udpstat"),
	ENTRY("u"),
	ENTRY("v"),
	};

static struct nlist *nlst;
static kvm_t	*kvm;
/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
static void	get_namelist(void);

int
load_nlist()
{
	kvm = kvm_open(
		(char *) NULL,
		(char *) NULL,
		(char *) NULL,
		O_RDONLY,
		"proc");
	if (kvm == (kvm_t *) NULL) {
		printf("kvm_open failed\n");
		return -1;
		}
	get_namelist();
	return 0;
}
static void
get_namelist()
{	int i;
	char *cp;

# define	NUM_ENTRIES (sizeof var_tbl / sizeof var_tbl[0])

	nlst = (struct nlist *) chk_zalloc((NUM_ENTRIES + 1) * sizeof(struct nlist));
	
	for (i = 0; i < NUM_ENTRIES; i++) {
		cp = var_tbl[i].name;
		if (*cp == '*' || *cp == '&')
			cp++;
		nlst[i].n_name = cp;
		}

	if (kvm_nlist(kvm, nlst) < 0) {
		fprintf(stderr, "can't get name list entries.\n");
		exit(2);
		}
}
long
get_ksym(name)
char	*name;
{	int	i;
	char	*cp;
	char	prefix = '\0';
	long	val;

	if (nlst == (struct nlist *) NULL)
		return 0;

	if (*name == '*')
		prefix = *name++;

	for (i = 0; i < NUM_ENTRIES; i++) {
		cp = var_tbl[i].name;
		if (*cp == '*' || *cp == '&')
			cp++;
		if (strcmp(cp, name) == 0) {
			if (prefix == '\0') {
				return nlst[i].n_value;
				}
			kvm_read(kvm, nlst[i].n_value, &val, sizeof val);
			return val;
			}
		}
	return 0;
}
int
get_struct(addr, buf, size)
long	addr;
void	*buf;
int	size;
{
	return kvm_read(kvm, addr, buf, size);
}
