# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<sys/socket.h>
# include	<netdb.h>
# include	<netinet/tcp.h>
# include	<dirent.h>

/**********************************************************************/
/*   Info to cache socket inodes to owning procs.		      */
/**********************************************************************/
typedef struct fd_list_t {
	unsigned long	f_inode;
	int		f_pid;
	} fd_list_t;
static fd_list_t *fd_list;
static int	fd_size = 256;
static int	fd_used;

static char *tcp_states[] = {
  "unknown",
  "ESTABLISHED",
  "SYN_SENT",
  "SYN_RECV",
  "FIN_WAIT1",
  "FIN_WAIT2",
  "TIME_WAIT",
  "CLOSE",
  "CLOSE_WAIT",
  "LAST_ACK",
  "LISTEN",
  "CLOSING",
  };
# define	NUM_STATES (sizeof tcp_states / sizeof(int))

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
static void read_fds(void);

void
display_netstat()
{	int	cnt;
	int	f, i, j;
	socket_t	*tbl;
	char	buf[BUFSIZ];
	char	buf2[BUFSIZ];
	char	buf3[BUFSIZ];
	struct servent *sp;
	struct sockaddr_in sin;
	char	*spc;
	int	states[sizeof tcp_states / sizeof (int)];

	memset(&sin, 0, sizeof sin);

	cnt = mon_read_netstat(0, &tbl);
	read_fds();

	set_attribute(GREEN, BLACK, 0);
	print("%d sockets: ", cnt);
	memset(states, 0, sizeof states);
	for (i = 0; i < cnt; i++) {
		states[tbl[i].state]++;
		}
	spc = "";
	for (i = 0; i < NUM_STATES; i++) {
		if (states[i] == 0)
			continue;
		print("%s%d %s", spc, states[i], tcp_states[i]);
		spc = ", ";
		}
	print("\n");

	print("OWNER    rcv/snd  State        Local address       Remote address\n");
	for (i = 0; i < cnt; i++) {
		long	ip;
		char	*name = NULL;
		char	port[128];
		struct hostent *hp = NULL;
		struct hostent *hp2 = NULL;
		sin.sin_family = AF_INET;

		/***********************************************/
		/*   Local address.			       */
		/***********************************************/
		sp = getservbyport(htons(tbl[i].l_port), "tcp");
		if (sp)
			snprintf(port, sizeof port, sp->s_name);
		else
			snprintf(port, sizeof port, "%u", tbl[i].l_port);
		if (switches['n' & 0x1f] == 0) {
			ip = htonl(tbl[i].l_ip);
			if (ip)
				name = hostname(ip);
			}
		if (name)
			snprintf(buf, sizeof buf, "%s:%s", name, port);
		else
			snprintf(buf, sizeof buf, "%ld.%ld.%ld.%ld:%s",
				(tbl[i].l_ip >> 0) & 0xff,
				(tbl[i].l_ip >> 8) & 0xff,
				(tbl[i].l_ip >> 16) & 0xff,
				(tbl[i].l_ip >> 24) & 0xff,
				port);
		/***********************************************/
		/*   Remote address.			       */
		/***********************************************/
		sp = getservbyport(htons(tbl[i].r_port), "tcp");
		if (sp)
			snprintf(port, sizeof port, sp->s_name);
		else
			snprintf(port, sizeof port, "%u", tbl[i].r_port);
		name = NULL;
		if (tbl[i].state != TCP_LISTEN && switches['n' & 0x1f] == 0) {
			ip = htonl(tbl[i].r_ip);
			name = hostname(ip);
			}
		if (name)
			snprintf(buf2, sizeof buf2, "%s:%s", name, port);
		else
			snprintf(buf2, sizeof buf2, "%ld.%ld.%ld.%ld:%s",
				(tbl[i].r_ip >> 0) & 0xff,
				(tbl[i].r_ip >> 8) & 0xff,
				(tbl[i].r_ip >> 16) & 0xff,
				(tbl[i].r_ip >> 24) & 0xff,
				port);

		snprintf(buf3, sizeof buf3, "%u/%u",
			tbl[i].rcvwin, tbl[i].sndwin);
		set_attribute(GREEN, BLACK, 0);
		print("%-8s ", username(tbl[i].uid));
		set_attribute(WHITE, BLACK, 0);
		print("%-8s %-12s %-19s %-19s\n",
			buf3,
			tcp_states[tbl[i].state],
			buf,
			tbl[i].state == TCP_LISTEN ? "" : buf2);
		/***********************************************/
		/*   Find the owning proc.		       */
		/***********************************************/
		for (f = 0; f < fd_used; f++) {
			if (fd_list[f].f_inode == tbl[i].inode) {
				int	fd;
				snprintf(buf, sizeof buf, "/proc/%d/cmdline", fd_list[f].f_pid);
				print_number_string("         pid=%d", fd_list[f].f_pid);
				if ((fd = open(buf, O_RDONLY)) >= 0) {
					int n = read(fd, buf2, sizeof buf2);
					close(fd);
					set_attribute(GREEN, BLACK, 0);
					print(" ");
					for (j = 0; j < n; j++) {
						print("%c", buf2[j] == '\0' ? ' ' : buf2[j]);
						}
					}
				print("\n");
				break;
				}
			}
		}
	print("\n");
	chk_free(tbl);
	clear_to_end_of_screen();
}

/**********************************************************************/
/*   Build  a cache of sockets to (one) process, so we know who owns  */
/*   it.							      */
/**********************************************************************/
static void
read_fds()
{	DIR	*dirp, *dirp1;
	struct dirent *de;
	int	len;
	char	buf[BUFSIZ];
	char	buf1[BUFSIZ];
	unsigned long inode;

	if (fd_list == NULL) {
		fd_list = (fd_list_t *) chk_alloc(fd_size * sizeof *fd_list);
		}
	fd_used = 0;

	if ((dirp = opendir("/proc")) == NULL)
		return;
	while ((de = readdir(dirp)) != NULL) {
		int	pid;
		if (!isdigit(de->d_name[0]))
			continue;
		pid = atoi(de->d_name);
		snprintf(buf, sizeof buf, "/proc/%d/fd", pid);

		if ((dirp1 = opendir(buf)) == NULL)
			continue;
		while ((de = readdir(dirp1)) != NULL) {
			if (de->d_name[0] == '.')
				continue;
			snprintf(buf, sizeof buf, "/proc/%d/fd/%s", pid, de->d_name);
			len = readlink(buf, buf1, sizeof buf1 - 1);
			if (len <= 0)
				continue;
			if (strncmp(buf1, "socket:[", 8) != 0)
				continue;
//			printf("%s\n", buf1);
			buf1[len] = '\0';
			inode = atol(buf1 + 8);
			if (fd_used + 1 >= fd_size) {
				fd_size += 256;
				fd_list = (fd_list_t *) chk_realloc(fd_list, fd_size * sizeof *fd_list);
				}
			fd_list[fd_used].f_inode = inode;
			fd_list[fd_used].f_pid = pid;
			fd_used++;
			}
		closedir(dirp1);
		}
	closedir(dirp);
}
