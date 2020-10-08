# if !defined(COLDISP_H_INCLUDE)
# define	COLDISP_H_INCLUDE

# include	<stdarg.h>
# include	<stdio.h>

/**********************************************************************/
/*   Following  macro  used  to  allow  compilation  with ANSI C and  */
/*   non-ANSI C compilers automatically.			      */
/**********************************************************************/
# if !defined(PROTO)
# 	if	defined(__STDC__) || defined(MSDOS)
#		define	PROTO(x)	x
#	else
#		define	PROTO(x)	()
#	endif
# endif

# if !defined(__STDC__) && !CR_NT && !CR_IRIS
#	define	const
# endif

# if !defined(TRUE)
#	define	TRUE	1
#	define	FALSE	0
# endif

# define	FG_MASK		0x0f
# define	BG_MASK		0x70
# define	ATTR_HIGHLIGHT	0x80
# define	ATTR_BOLD	0x100
# define	GET_FG(x)	((x) & FG_MASK)
# define	GET_BG(x)	(((x) & BG_MASK) >> 4)
# define	GET_ATTR(x)	((x) & ATTR_HIGHLIGHT)

# define	MAKE_COLOR(fg, bg)	((fg) | (bg) << 4)

# define	WHITE	7
# define	CYAN	6
# define	MAGENTA	5
# define	BLUE	4
# define	YELLOW	3
# define	GREEN	2
# define	RED	1
# define	BLACK	0

void	init_display(int, int);
void	clear_screen PROTO((void));
void	clear_to_end_of_screen(void);
void	clear_to_eol PROTO((void));
void	home_screen PROTO((void));
void	goto_rc PROTO((int, int));
void	print(char *, ...);
void	set_attribute PROTO((int, int, int));
void	display_attribute PROTO((void));
void	print_char PROTO((int));
void	print_string PROTO((char *));
void	mvprint(int, int, char *, ...);
void	refresh(void);
void	snap_to_file(FILE *);
void	clear(void);
void	snapshot(void);
void	refresh_fp(FILE *fp);
void	out_flush(void);
void	reinit_screen(void);
void	end_screen(void);


extern int dsp_crmode;
extern int csr_x, csr_y;
extern int dst_x, dst_y;
# endif
