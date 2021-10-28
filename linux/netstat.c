# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<sys/socket.h>
# include	<netdb.h>
# include	<netinet/tcp.h>
# include	<dirent.h>
# include	"../include/hash.h"

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

hash_t	*hash_serv;

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
static void load_portmap(void);
static void read_fds(void);
static void save_portmap(void);

char *
hostname(long ip)
{	static char buf[128];

	snprintf(buf, sizeof buf, "%d.%d.%d.%d",
		(ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
	return buf;
}

char *
getport(unsigned int port, char *proto)
{	struct servent *sp;
	char	*cp;
static	char	buf[64];

	/***********************************************/
	/*   Avoid port storm.			       */
	/***********************************************/
	if (port >= 32768) {
		snprintf(buf, sizeof buf, "%u", port);
		return buf;
		}

	if (hash_serv == NULL) {
		hash_serv = hash_create(256, 256);
		load_portmap();
		}

	cp = hash_int_lookup(hash_serv, port);
	if (cp)
		return cp;

	sp = getservbyport(port, proto);
	if (!sp) {
		snprintf(buf, sizeof buf, "%u", port);
		cp = chk_strdup(buf);
		}
	else
		cp = chk_strdup(sp->s_name);
	hash_int_insert(hash_serv, port, cp);
	/***********************************************/
	/*   Save the portmap state.		       */
	/***********************************************/
	save_portmap();
	return cp;
}
static void
load_portmap()
{	FILE	*fp;
	char	buf[BUFSIZ];

	snprintf(buf, sizeof buf, "%s/services", mon_dir());
	if ((fp = fopen(buf, "r")) == NULL)
		return;
	while (fgets(buf, sizeof buf, fp) != NULL) {
		int	port;
		sscanf(buf, "%d %s", &port, buf);
		hash_int_insert(hash_serv, port, chk_strdup(buf));
		}
	fclose(fp);
}
static void
save_portmap()
{	FILE	*fp;
	int	i;
	char	buf[BUFSIZ];
	hash_element_t **harray;

	snprintf(buf, sizeof buf, "%s/services", mon_dir());
	if ((fp = fopen(buf, "w")) == NULL)
		return;
	chmod(buf, 0666);
	harray = hash_linear(hash_serv, HASH_UINT_COMPARE);
	for (i = 0; i < hash_size(hash_serv); i++) {
		fprintf(fp, "%u %s\n",
			(int) hash_key(harray[i]),
			hash_data(harray[i]));
		}
	chk_free(harray);
	fclose(fp);
}
int
netstat_sort(socket_t *p1, socket_t *p2)
{
	return p2->s_time - p1->s_time;
}

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
	if (cnt == 0) {
		print("Sorry - netstat info not available. Try later.\n");
		return;
		}

	read_fds();

	qsort(tbl, cnt, sizeof *tbl, netstat_sort);

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
		char	*port_name;
		char	*name;
		char	port[128];
		struct hostent *hp = NULL;
		struct hostent *hp2 = NULL;
		sin.sin_family = AF_INET;
		int	keep = FALSE;

		if (grep_compare(tcp_states[tbl[i].state]))
			keep = TRUE;
		if (grep_compare(username(tbl[i].uid)))
			keep = TRUE;

		/***********************************************/
		/*   Local address.			       */
		/***********************************************/
		snprintf(buf, sizeof buf, "%ld.%ld.%ld.%ld:%u",
			(tbl[i].l_ip >> 0) & 0xff,
			(tbl[i].l_ip >> 8) & 0xff,
			(tbl[i].l_ip >> 16) & 0xff,
			(tbl[i].l_ip >> 24) & 0xff,
			tbl[i].l_port);
		/***********************************************/
		/*   Remote address.			       */
		/***********************************************/
		snprintf(buf2, sizeof buf2, "%ld.%ld.%ld.%ld:%u",
			(tbl[i].r_ip >> 0) & 0xff,
			(tbl[i].r_ip >> 8) & 0xff,
			(tbl[i].r_ip >> 16) & 0xff,
			(tbl[i].r_ip >> 24) & 0xff,
			tbl[i].r_port);

		if (grep_compare(buf))
			keep = TRUE;
		if (grep_compare(buf2))
			keep = TRUE;
		if (!keep)
			continue;

		snprintf(buf3, sizeof buf3, "%u/%u",
			tbl[i].rcvwin, tbl[i].sndwin);

		if (tbl[i].s_time + 30 < time(NULL))
			set_attribute(GREEN, BLACK, 0);
		else
			set_attribute(BLACK, CYAN, 0);
		print("%-8s ", username(tbl[i].uid));

		set_attribute(WHITE, BLACK, 0);
		if (tbl[i].rcvwin || tbl[i].sndwin)
			set_attribute(BLACK, CYAN, 0);
		print("%-8s ", buf3);
		set_attribute(WHITE, BLACK, 0);

		if (tbl[i].state == TCP_ESTABLISHED)
			set_attribute(YELLOW, BLACK, 0);
		print("%-12s ", tcp_states[tbl[i].state]);
		set_attribute(WHITE, BLACK, 0);
		print("%-19s %-19s\n",
			buf,
			tbl[i].state == TCP_LISTEN ? "" : buf2);
		/***********************************************/
		/*   Local line.			       */
		/***********************************************/
		name = NULL;
		port_name = getport(htons(tbl[i].l_port), "tcp");
		if (port_name)
			snprintf(port, sizeof port, "%s", port_name);
		else {
			snprintf(port, sizeof port, "%u", tbl[i].l_port);
			}
		ip = htonl(tbl[i].l_ip);
		if (ip)
			name = hostname(ip);
		print("         Local:   ");
		if (name == NULL) {
			set_attribute(CYAN, BLACK, 0);
			print("INADDR_ANY:");
			set_attribute(WHITE, BLACK, 0);
		} else {
			print("%s:", name);
			}
		print("%s\n", port);
		/***********************************************/
		/*   Remote line.			       */
		/***********************************************/
		if (tbl[i].state != TCP_LISTEN) {
			name = NULL;
			port_name = getport(htons(tbl[i].r_port), "tcp");
			if (port_name)
				snprintf(port, sizeof port, "%s", port_name);
			else {
				snprintf(port, sizeof port, "%u", tbl[i].r_port);
				}
			ip = htonl(tbl[i].r_ip);
			if (ip)
				name = hostname(ip);
			if (name)
				snprintf(buf, sizeof buf, "%s:%s", name, port);
			else
				snprintf(buf, sizeof buf, "%s:%s", name, port);
			print("         Remote:  %s\n", buf);
		}
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
