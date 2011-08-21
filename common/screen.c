/*
 *  Top - a top users display for Berkeley Unix
 *
 *  This file contains the routines that interface to termcap and stty/gtty.
 *
 *  Paul Vixie, February 1987: converted to use ioctl() instead of stty/gtty.
 */

# include	"psys.h"
#include <termio.h>
#include "screen.h"
#include "coldisp.h"

# define	TRUE	1
# define	FALSE	0

int putstdout();
int  scrolls;
int  hardcopy;
int  screen_length;
int  screen_width;
extern int	once_only;
extern	char	*do_move_col;

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

#define	STDIN	0
#define	STDOUT	1
#define	STDERR	2

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
	    fprintf(stderr, "No termcap entry for a `%s' terminal\n", term_name);

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

    /* screen_width is a little different */
    if ((screen_width = tgetnum("co")) == -1)
	screen_width = 79;
    else
	screen_width--;
	{	extern int rows, cols;
		rows = r ? r : screen_length;
		cols = c ? c : screen_width;
	}

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

init_screen(ticks)
int	ticks;
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
    else
    {
	/* not a terminal at all---consider it dumb */
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

void
reset_terminal()
{
	if (is_a_terminal)
		ioctl(0, TCSETAF, &old_settings);
}
reinit_screen()

{
    /* install our settings if it is a terminal */
    if (is_a_terminal)
	ioctl(0, TCSETAF, &new_settings);

    /* send init string */
    if (smart_terminal)
	putcap(terminal_init);
}

standout(fmt, a1, a2, a3)

char *fmt;
int a1, a2, a3;

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
Tgetstr(str, buf)
char	*str;
char	**buf;
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
/* This has to be defined as a subroutine for tputs (instead of a macro) */

putstdout(ch)

char ch;

{
	out_char(ch);
}

