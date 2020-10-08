/**********************************************************************/
/*   Curses/emulation routines.					      */
/**********************************************************************/

# include	"psys.h"
#include <termio.h>
#include "screen.h"
#include "coldisp.h"

# define	TRUE	1
# define	FALSE	0

# define	TBUFSIZ		(3 * 1024)

/**********************************************************************/
/*   For use in %P[a-z] and %g[a-z].				      */
/**********************************************************************/
static short	ti_vars[26];
static	char	*ubuf;		/* User's termcap buffer.		*/

static struct tc {
	int	name;
	char	*entry;
	}	*tc;
static int	ent_count;
static int	tc_size;

int  scrolls;
int  hardcopy;
int  screen_length;
int  screen_width;
extern int	once_only;
extern	char	*do_move_col;
extern int rows, cols;

char ch_erase;
char ch_kill;
char smart_terminal;
char *Tgetstr(), *tgetstr();
char *tgoto();
char string_buffer[1024];
char home[15];
char lower_left[15];
char *clear_line;
char *esc_clear_screen;
char	*clr_to_eos;
char *cursor_motion;
char *start_standout;
char *end_standout;
char *terminal_init;
char *terminal_end, *terminal_end1;

static struct termio old_settings;
static struct termio new_settings;
static char is_a_terminal = FALSE;

char	*termcap_filename;
char *termcap_dir;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
static int	tgetent1 PROTO((char *, char *, int));
void putstdout(char ch);

void
init_termcap(int r, int c)

{	char *bufptr;
	char *term_name;
	char *getenv();
	int status;
	char termcap_buf[1024];

	/* assume we have a smart terminal until proven otherwise */
	smart_terminal = TRUE;

	/* now get terminal name and termcap entry */
	term_name = getenv("TERM");
    if ((status = tgetent(termcap_buf, term_name)) != 1) {
	if (status == -1)
	    fprintf(stderr, "Can't open termcap file\n");
	else
	    fprintf(stderr, "No termcap entry for a '%s' terminal\n", term_name);

	/* pretend it's dumb and proceed */
	smart_terminal = FALSE;
	return;
    }

    /* these immediately indicate a very stupid terminal */
    if (tgetflag("hc") || tgetflag("os")) {
	smart_terminal = FALSE;
	return;
    }

    /* set up common terminal capabilities */
    if ((screen_length = tgetnum("li")) <= 0) {
	screen_length = smart_terminal = 0;
	return;
    }
	if (getenv("LINES"))
		screen_length = atoi(getenv("LINES"));

	if ((screen_width = tgetnum("co")) == -1)
		screen_width = 79;
	else
		screen_width--;
	rows = r ? r : screen_length;
	cols = c ? c : screen_width;

    /* initialize the pointer into the termcap string buffer */
    bufptr = string_buffer;

    /* get necessary capabilities */
    if ((clear_line    = Tgetstr("ce", &bufptr)) == NULL ||
	(esc_clear_screen  = Tgetstr("cl", &bufptr)) == NULL ||
	(clr_to_eos  = Tgetstr("cd", &bufptr)) == NULL ||
	(cursor_motion = Tgetstr("cm", &bufptr)) == NULL) {
	smart_terminal = FALSE;
	return;
    }

	/* get some more sophisticated stuff -- these are optional */
	terminal_init  = Tgetstr("ti", &bufptr);
	terminal_end   = Tgetstr("te", &bufptr);
	terminal_end1   = Tgetstr("rs", &bufptr);
	start_standout = Tgetstr("so", &bufptr);
	end_standout   = Tgetstr("se", &bufptr);


	/* set convenience strings */
	strcpy(home, tgoto(cursor_motion, 0, 0));
	strcpy(lower_left, tgoto(cursor_motion, 0, screen_length - 1));

	/* if stdout is not a terminal, pretend we are a dumb terminal */
	if (ioctl(0, TCGETA, &old_settings) < 0)
		smart_terminal = FALSE;
}

void
init_screen(int ticks)
{
    /* get the old settings for safe keeping */
    if (ioctl(0, TCGETA, &old_settings) == 0) {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	new_settings.c_cc[VQUIT] = 0xff;
	new_settings.c_cc[VMIN] = 1;      /* one character read is OK     */
	new_settings.c_cc[VTIME] = 0/*ticks*/;
	new_settings.c_iflag |= IGNBRK;
	new_settings.c_iflag &= ~( /*ICRNL | INLCR | */ISTRIP | IXON | IXOFF );
	new_settings.c_oflag &= ~(OCRNL | ONLRET);
	new_settings.c_oflag |= ONLCR;
	new_settings.c_cflag |= CS8;      /* allow 8th bit on input       */
	new_settings.c_cflag &= ~PARENB;  /* Don't check parity           */
	new_settings.c_lflag &= ~( ECHO | ICANON/* | ISIG*/ );

	ioctl(0, TCSETAF, &new_settings);

	/* remember the erase and kill characters */
	ch_erase = old_settings.c_cc[VERASE];
	ch_kill  = old_settings.c_cc[VKILL];

	/* remember that it really is a terminal */
	is_a_terminal = TRUE;

	/* send the termcap initialization string */
	if (terminal_init && !once_only)
		putcap(terminal_init);
    }
    else {
	smart_terminal = FALSE;
    }
}

void
end_screen()
{
	set_attribute(WHITE, BLACK, 0);
	display_attribute();
	refresh();

	/* move to the lower left, clear the line and send "te" */
	if (smart_terminal && !once_only) {
		printf("%s%s", lower_left, clear_line);
		fflush(stdout);
		}

	/* if we have settings to reset, then do so */
	if (is_a_terminal)
		ioctl(0, TCSETAF, &old_settings);
}

static  int
compile_termcap(char *cp)
{
	while (*cp) {
		while (*cp != ':' && *cp)
			cp++;
		if (*cp == '\0' || cp[1] == '\0')
			break;
		if (ent_count + 1 >= tc_size) {
			tc_size += 128;
			tc = (struct tc *) chk_realloc((void *) tc,
				tc_size * sizeof(struct tc));
			}
		tc[ent_count].entry = ++cp;
		tc[ent_count++].name = (*cp << 8) | cp[1];
		}

	return 1;
}

/**********************************************************************/
/*   New  termcap  interface  routine  which  dynamically  allocates  */
/*   buffer  for  termcap  info  rather  than hardcoding a too large  */
/*   static buffer into the code.				      */
/**********************************************************************/
char *
tgetent_str(char *name, int *retp)
{	char	buf[TBUFSIZ];
	int	ret;
	char	*cp;

	if ((ret = tgetent(buf, name)) <= 0) {
		*retp = ret;
		return (char *) NULL;
		}
	/***********************************************/
	/*   Grab   a   chunk   of  memory  hopefully  */
	/*   smaller  than  the  TBUFSIZ allocated on  */
	/*   the   stack.   Need   to  recompile  the  */
	/*   pointers.				       */
	/***********************************************/
	ent_count = 0;
	cp = chk_strdup(buf);
	compile_termcap(cp);
	return cp;
}
/**********************************************************************/
/*   Termcap routine to read an entry from the termcap file.	      */
/**********************************************************************/
int
tgetent(char *bp, char *name)
{	char	buf[64];
	char	*p;
	char	*cp;
	int	ret;

	ubuf = bp;
	tc_size = 256;
	tc = (struct tc *) chk_alloc(tc_size * sizeof (struct tc));
	if ((ret = tgetent1(bp, name, TRUE)) <= 0)
		return ret;

	compile_termcap(ubuf);
	p = buf;
	if ((cp = tgetstr("tc", &p)) == (char *) NULL)
		return 1;

	p = ubuf + strlen(ubuf);
	tgetent1(p, cp, FALSE);
	compile_termcap(p);

	return 1;
}
static int
tgetent1(char *bp, char *name, int first_time)
{	int	fd;
	char	buf[TBUFSIZ];
	register char *cp;
	register char	*bp1 = &buf[TBUFSIZ];
	int	cnt = -1;
	char	*term = getenv("TERM");
	struct stat sbuf;
	
	/***********************************************/
	/*   Under  Mac  OS  X,  TERMCAP may not be a  */
	/*   file and be useless crap.		       */
	/***********************************************/
	termcap_filename = getenv("TERMCAP");
	if (termcap_filename && strlen(termcap_filename) == 3 &&
	    termcap_filename[0] & 0x80)
	    	termcap_filename = NULL;
	if (termcap_filename == NULL) {
		strcpy(bp, "xterm:|");
		return 1;
		}

	if ((fd = open(termcap_filename, O_RDONLY)) < 0) {
		/***********************************************/
		/*   Try   /etc/termcap  if  the  environment  */
		/*   variables lied to us.		       */
		/***********************************************/
		if (strcmp(termcap_filename, "/etc/termcap") == 0)
			return -1;
		if ((fd = open("/etc/termcap", O_RDONLY)) < 0)
			return -1;
		}
	while (1) {
		cp = bp;
		while (1) {
			if (--cnt <= 0) {
				if ((cnt = read(fd, buf, TBUFSIZ)) <= 0) {
					close(fd);
					fd = -1;
					break;
					}
				bp1 = buf;
				}
			if ((*cp++ = *bp1++) == '\n') {
				cp--;
				if (cp == bp)
					break;
				if (cp > bp && cp[-1] != '\\')
					break;
				cp--;
				continue;
				}
			}
		*cp = '\0';
		if (tnamchk(bp, name)) {
			if (fd > 0)
				close(fd);
			return 1;
			}
		if (fd < 0)
			return 0;
		}

	/* NOTREACHED */	
}
int
tnamchk(char *bp, char *name)
{	register char	*cp;

	while (1) {
		for (cp = name; *bp == *cp; )
			bp++, cp++;
		if (*cp == '\0' && (*bp == '|' || *bp == ':'))
			break;
		while (*bp && *bp != '|' && *bp != ':')
			bp++;
		if (*bp != '|')
			return 0;
		bp++;
		}
	return 1;
}

void
reset_terminal()
{
	if (is_a_terminal)
		ioctl(0, TCSETAF, &old_settings);
}

void
reinit_screen()
{
    /* install our settings if it is a terminal */
    if (is_a_terminal)
	ioctl(0, TCSETAF, &new_settings);

    /* send init string */
    if (smart_terminal)
	putcap(terminal_init);
}

void
standout(char *fmt, int a1, int a2, int a3)
{
	print(fmt, a1, a2, a3);
}

void
clear()
{
	if (!once_only && smart_terminal) {
		putcap(esc_clear_screen);
		}
	clear_screen();
}

char *
Tgetstr(char *str, char **buf)
{	char	*ret, *cp;

	ret = tgetstr(str, buf);
	if (ret == (char *) NULL || strchr(ret, '$') == (char *) NULL)
		return ret;
	for (cp = ret; *cp && *cp != '$'; cp++)
		;
	if (cp[1] == '<')
		*cp = '\0';
	return ret;
}

/**********************************************************************/
/*   Routine  to  decypher  termcap style strings. Needed by code in  */
/*   set_term_escapes.						      */
/**********************************************************************/
char *
tcopy_string(char *dp, char *bp, int delim)
{	int n;

	while (*bp != (char) delim && *bp) {
		if (*bp == '^') {
			*dp++ = (char) (*++bp & 0x1f);
			bp++;
			continue;
			}
		if (*bp != '\\') {
			*dp++ = *bp++;
			continue;
			}
		switch (*++bp) {
		  case 'E':	*dp++ = 0x1b; break;
		  case 'r':	*dp++ = '\r'; break;
		  case 'n':	*dp++ = '\n'; break;
		  case 't':	*dp++ = '\t'; break;
		  case 'b':	*dp++ = '\b'; break;
		  case 'f':	*dp++ = '\f'; break;
		  case '0': case '1': case '2': case '3':
			n = 0;
			while (*bp >= '0' && *bp <= '7')
				n = 8*n + *bp++ - '0';
			*dp++ = (char) n;
			continue;
		  case 'x':
			bp++;
			n = get_two_hex_digits(&bp);
			*dp++ = (char) n;
			continue;
		  default:
			*dp++ = *bp; break;
		  }
		bp++;
		}
	*dp++ = '\0';
	return dp;
}


void
putstdout(char ch)
{
	out_char(ch);
}
int
tgetnum(char *id)
{	register char	*bp = ubuf;

	if (strncmp(id, "li", 2) == 0)
		return rows;
	if (strncmp(id, "co", 2) == 0)
		return cols;

	while (*bp) {
		while (*bp != ':' && *bp)
			bp++;
		if (*bp && bp[1] == id[0] && bp[2] == id[1] && bp[3] == '#')
			return atoi(bp+4);
		bp++;
		}
	return 0;
}
int
tgetflag(char *id)
{	register char	*bp = ubuf;

	while (*bp) {
		while (*bp != ':' && *bp)
			bp++;
		if (*bp && bp[1] == id[0] && bp[2] == id[1])
			return 1;
		bp++;
		}
	return 0;
}
char *
tgetstr(char *id, char **area)
{	register int	ltc = (*id << 8) | (id[1] & 0xff);
	register int	i = ent_count;
	register char	*dp = *area;
	char	*init_area = *area;
	struct tc *tcp = tc;
	char	*cp;

	for (; i-- > 0; tcp++) {
		if (tcp->name != ltc)
			continue;
		if (tcp->entry[2] == '=') {
			/*------------------------------
			 *   Remove delays at beginning of string.
			 *------------------------------*/
			for (cp = tcp->entry + 3; isdigit(*(unsigned char *) cp); )
				cp++;
			*area = tcopy_string(dp, cp, ':');
			return init_area;
			}
		}
	return (char *) NULL;
}
char *
tgoto(register char *str, int col, int row)
{	static char buf[32];
	register char	*cp = buf;
	int	arg[2];
	register int	*argp = arg;
	int	t;
# define	MAX_STACK	12
	int	stack[MAX_STACK];
	int	sp = 0;
	int	terminfo = FALSE;
	int	if_level = 0;

	arg[0] = row;
	arg[1] = col;

	stack[sp++] = col;
	stack[sp++] = row;

	while (*str) {
		if (*str != '%') {
			*cp++ = *str++;
			continue;
			}
		str++;
		switch (*str++) {
		  case '?':
		  	/***********************************************/
		  	/*   Terminfo: %? .. %t .. %;			       */
		  	/***********************************************/
			if_level++;
			break;
		  case 't':
		  	/***********************************************/
		  	/*   Terminfo: Then clause		       */
		  	/***********************************************/
			if (sp <= 0)
				break;
			if (stack[sp-1] == 0) {
				/***********************************************/
				/*   Skip to else of endif.		       */
				/***********************************************/
				while (*str && str[1] &&
				       str[0] != '%' && str[1] != ';')
				       str++;
				if (*str == '%' && str[1] == ';')
					str += 2;
				}
			break;
		  case ';':
		  	/***********************************************/
		  	/*   Terminfo: endif clause.		       */
		  	/***********************************************/
			if_level--;
			break;

		  case '+':
			if (terminfo && sp > 2) {
				/* Terminfo */
				stack[sp-2] = stack[sp-1] + stack[sp-2];
				sp--;
				break;
				}
			*argp += *str++;
			/* Fall into ... */
		  case '.':	/* Code change thanks to john@hcrvax.uucp */
			*cp++ = (char) *argp++;
			break;
		  case '\'':
			stack[sp++] = *str++;
			if (*str)
				str++;
			break;
		  case '>':
			if (*argp > *str)
				*argp += str[1];
			str += 2;
			break;

		  case '2':
			sprintf(cp, "%02d", *argp++);
			cp += 2;
			break;
		  case '3':	
			sprintf(cp, "%03d", *argp++);
			cp += 3;
			break;

		  case 'B':
			*argp = (16 * (*argp / 10)) + (*argp % 10);
			break;
		  case 'c':
			/* Terminfo */
			if (sp > 0)
				*cp++ = stack[--sp];
			break;
		  case 'D':
			*argp = (*argp - 2 * (*argp % 16));
			break;
		  case 'd':
		  	if (terminfo) {
			  	if (sp > 0) {
					sprintf(cp, "%d", stack[--sp]);
					}
				}
			else {
				sprintf(cp, "%d", *argp++);
				}
			cp += strlen(cp);
			break;
		  case 'g':
		  	if (islower(*(unsigned char *) str) && sp < MAX_STACK) {
				stack[sp++] = ti_vars[*str++ - 'a'];
				}
			break;
		  case 'i':
			arg[0]++, arg[1]++;
			break;
		  case 'n':
			arg[0] ^= 0140;
			arg[1] ^= 0140;
			break;
		  case 'P':
		  	if (sp > 0 && islower(*(unsigned char *) str)) {
				ti_vars[*str++ - 'a'] = stack[--sp];
				}
			break;
		  case 'p':
			/* %pX -- Push arg X */
			/* Needed for QNX. Sufficient for now */
			if (sp >= MAX_STACK)
				break;
			if (*str++ == '1')
				stack[sp++] = arg[0];
			else
				stack[sp++] = arg[1];
			break;
		  case 'r':
			t = arg[0];
			arg[0] = arg[1];
			arg[1] = t;
			break;
		  case '%':
			*cp++ = '%';
			break;
		  default:
			*cp++ = *str;
			break;
		  }
		}
	*cp = '\0';
	return buf;
}
int
tputs(char *cp, int affcnt, int (*outc)())
{
	while (*cp)
		(*outc)(*cp++);
	return 0;
}

