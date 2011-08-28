# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"
# include	<sys/socket.h>
# include	<netdb.h>

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

void
display_netstat()
{	int	cnt;
	int	i;
	socket_t	*tbl;
	char	buf[BUFSIZ];
	char	buf2[BUFSIZ];
	struct servent *sp;
	struct sockaddr_in sin;
	char	*spc;
	int	states[sizeof tcp_states / sizeof (int)];

	memset(&sin, 0, sizeof sin);

	cnt = mon_read_netstat(0, &tbl);

	print("netstat - %d sockets\n", cnt);
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

	print("OWNER    rcv :snd  Local address       Remote address      State\n");
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
		if (switches['n' & 0x1f] == 0) {
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
		print("%-8s %4d:%-4d %-19s %-19s %-14s \n",
			username(tbl[i].uid),
			tbl[i].rcvwin, tbl[i].sndwin,
			buf,
			buf2, 
			tcp_states[tbl[i].state]);
		}
	print("\n");
	chk_free(tbl);
	clear_to_end_of_screen();
}
