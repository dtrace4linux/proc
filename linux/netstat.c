# include	"psys.h"
# include	"screen.h"
# include	"coldisp.h"

/**********************************************************************/
/*   Network stats.						      */
/**********************************************************************/
typedef struct netstat_t {
	char	n_if[16];
	unsigned long long	n_rxbytes;
	unsigned long long	n_rxpkts;
	unsigned long long	n_rxerrs;
	unsigned long long	n_rxdrop;
	unsigned long long	n_rxfifo;
	unsigned long long	n_rxframe;
	unsigned long long	n_rxcomp;
	unsigned long long	n_rxmcast;
	unsigned long long	n_txbytes;
	unsigned long long	n_txpkts;
	unsigned long long	n_txerrs;
	unsigned long long	n_txdrop;
	unsigned long long	n_txfifo;
	unsigned long long	n_txcolls;
	unsigned long long	n_txcarrier;
	unsigned long long	n_txcomp;
	} netstat_t;
netstat_t	*netstat;
netstat_t	*old_netstat;
int		netstat_size;

static void read_net(void);

void
display_netstat()
{	netstat_t *np;
	int	n = 0;

	if (old_netstat) {
		chk_free(old_netstat);
		old_netstat = NULL;
		}
	if (netstat_size) {
		old_netstat = chk_alloc(netstat_size * sizeof *netstat);
		memcpy(old_netstat, netstat, netstat_size * sizeof *netstat);
		}

	read_net();
	if (old_netstat == NULL)
		old_netstat = chk_zalloc(netstat_size * sizeof *netstat);
	print("         rxbyte  rxpkt rxerr rxdrop  txbyte txpkt txerr txdrop\n");
	for (np = netstat; np < &netstat[netstat_size]; np++) {
		netstat_t *np1 = &old_netstat[n++];
		print("%-9s ", np->n_if);
		print_ranged(np->n_rxbytes - np1->n_rxbytes);
		print_ranged(np->n_rxpkts - np1->n_rxpkts);
		print_ranged(np->n_rxerrs - np1->n_rxerrs);
		print_ranged(np->n_rxdrop - np1->n_rxdrop);
		print(" | ");
		print_ranged(np->n_txbytes - np1->n_txbytes);
		print_ranged(np->n_txpkts - np1->n_txpkts);
		print_ranged(np->n_txerrs - np1->n_txerrs);
		print_ranged(np->n_txdrop - np1->n_txdrop);
		print("\n");
	}
	clear_to_end_of_screen();
}

static void
read_net()
{	FILE	*fp;
	char	buf[BUFSIZ];
	int	n = 0;
	netstat_t	*np;

	if ((fp = fopen("/proc/net/dev", "r")) == NULL)
		return;

	fgets(buf, sizeof buf, fp);
	fgets(buf, sizeof buf, fp);
	while (fgets(buf, sizeof buf, fp)) {
		if (n >= netstat_size) {
			if (n == 0)
				netstat = chk_zalloc(sizeof *netstat);
			else {
				netstat = chk_realloc(netstat, (n + 1) * sizeof *netstat);
				memset(&netstat[n], 0, sizeof *netstat);
				}
			netstat_size++;
			}
		np = &netstat[n++];
		sscanf(buf, " %s %llu %llu %llu %llu %llu %llu %llu %llu %llu", 
			&np->n_if, 
			&np->n_rxbytes,
			&np->n_rxpkts,
			&np->n_rxerrs,
			&np->n_rxdrop,
			&np->n_rxfifo,
			&np->n_rxframe,
			&np->n_rxcomp,
			&np->n_rxmcast,
			&np->n_txbytes,
			&np->n_txpkts,
			&np->n_txerrs,
			&np->n_txdrop,
			&np->n_txfifo,
			&np->n_txcolls,
			&np->n_txcarrier,
			&np->n_txcomp);
	}
	fclose(fp);
}

