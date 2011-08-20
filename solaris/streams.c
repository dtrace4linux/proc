# include	"psys.h"
# define	strsignal	renamed_strsignal
# include	<sys/stream.h>
# include	<sys/tiuser.h>
# include	<sys/systm.h>
# include	<sys/stropts.h>
# include	<sys/sockmod.h>
# include	<sys/strsubr.h>
# include	<net/route.h>
# include	<netinet/in.h>
# include	<netinet/in_pcb.h>
# include	<netinet/tcp.h>

void
streams_display(addr, full_flag)
int	addr;
int	full_flag;
{
	struct stdata st;
	struct stdata *stp;
	struct qinit qinit;
	struct module_info minfo;
	extern long	strhead_addr;
	char	name[BUFSIZ];
	queue_t	q;
	queue_t	*qp;	
	int	loop_count = 24;
	int	need_arrow = FALSE;
	int	i;
	int	tcp_flag = FALSE;

	stp = (struct stdata *) addr;
	get_struct((long) stp, &st, sizeof st);
	/***********************************************/
	/*   Try and determine if this is TCP/UDP and  */
	/*   avoid dumping stream info.                */
	/***********************************************/
	qp = st.sd_wrq;
	if (!full_flag) {
		for (qp = st.sd_wrq; qp && loop_count-- > 0; ) {
			get_struct((long) qp, &q, sizeof q);
			get_struct((long) q.q_qinfo, &qinit, sizeof qinit);
			get_struct((long) qinit.qi_minfo, &minfo, sizeof minfo);
			get_struct((long) minfo.mi_idname, name, sizeof name);
			if (strncmp(name, "tcp", 3) == 0 ||
			    strncmp(name, "udp", 3) == 0) {
				tcp_flag = TRUE;
				break;
				}
			qp = q.q_next;
			}
		if (qp == (queue_t *) NULL)
			qp = st.sd_wrq;
		loop_count = 24;
		}
	if (full_flag)
		mvprint(6, 0, "====== stdata: 0x%lx ========\n", addr, stp);
	while (qp) {
		get_struct((long) qp, &q, sizeof q);
		get_struct((long) q.q_qinfo, &qinit, sizeof qinit);
		get_struct((long) qinit.qi_minfo, &minfo, sizeof minfo);
		get_struct((long) minfo.mi_idname, name, sizeof name);
		if (full_flag)
			print("0x%08lx: %-8s count=%d qptr=0x%08lx ", 
				qp, name, q.q_count, q.q_ptr);
		else {
			if (need_arrow)
				print("->");
			need_arrow = TRUE;
			for (i = 0; i < 12 && name[i]; i++)
				print("%c", name[i] >= ' ' && i <= 0x7e ? name[i] : '?');
			if (q.q_count)
				print("(%d)", q.q_count);
			}
		if (strncmp(name, "tcp", 3) == 0 ||
		    strncmp(name, "udp", 3) == 0) {
		    	struct inpcb inpcb;
			if (tcp_flag) {
				if (strncmp(name, "tcp", 3) == 0)
					print(" TCP <");
				else
					print(" UDP <");
				}
			else
				print("\n    <");
			get_struct((long) q.q_ptr, &inpcb, sizeof inpcb);
			print_addr(htonl(inpcb.inp_laddr.s_addr));
			print(":%u ", inpcb.inp_lport);
			print_addr(htonl(inpcb.inp_faddr.s_addr));
			print(":%u>", inpcb.inp_fport);
		    	}
		if (full_flag)
			print("\n");
		qp = q.q_next;
		if (--loop_count < 0)
			break;
		}
}

