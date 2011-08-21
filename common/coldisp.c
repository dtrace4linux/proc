# include	"coldisp.h"
# include	"psys.h"
# include	<termio.h>

# define	ARGS	a, b, c, d, e, f, g, h, i, j, k, l, m, n
int	auto_hilite;
int	once_only;
int	rows;
int	cols;
int	csr_x, csr_y;

int	dsp_crmode = TRUE;
int	dsp_autowrap = FALSE;

# define	MAX_HIST	10
static char	*history;
static int	h;

/**********************************************************************/
/*   Output buffer.						      */
/**********************************************************************/
static int	osize = 1024;
static int	oused;
static char	obuf[1024];

static char	*dpy;
static char	*attr;
static	int	display_initialised = FALSE;

int	dst_x, dst_y, dst_attr;
static int	cur_attr = -1;
static int batch_mode;

static void	goto_pos PROTO((int, int, int, int));
void	refresh();
void	out_str(char *);
void	out_char(int);
void	out_flush(void);
void print_number_string(char *str, ...);

void
init_display(int r, int c)
{
# if  defined(TIOCGWINSZ)
	struct winsize	winsize;
	int	new_rows;
	int	new_cols;

	if (display_initialised) {
		/***********************************************/
		/*   Handle window changing size.	       */
		/***********************************************/
		if (ioctl(0, TIOCGWINSZ, (char *) &winsize) < 0)
			return;
		new_rows = r ? r : winsize.ws_row;
		new_cols = c ? c : winsize.ws_col;
		if (new_rows == rows && new_rows == cols)
			return;
		free(dpy);
		free(attr);
		rows = new_rows;
		cols = new_cols;
		}
# endif
	display_initialised = TRUE;
	dpy = (char *) malloc(rows * cols);
	attr = (char *) malloc(rows * cols);
	if (!batch_mode)
		history = (char *) calloc(rows * cols * MAX_HIST, 1);
	if (dpy == NULL || attr == NULL) {
		fprintf(stderr, "Cannot get memory for display.\n");
		exit(1);
		}
	if (!once_only)
		clear();
	set_attribute(WHITE, BLACK, 0);
}
void
disp_set_batch(int r, int c)
{
	if (r && c == 0)
		c = 80;
	if (r && c) {
		rows = r;
		cols = c;
		if (dpy)
			free(dpy);
		if (attr)
			free(attr);
		if (history) {
			free(history);
			history = NULL;
			}
		dpy = (char *) malloc(rows * cols + 1);
		attr = (char *) malloc(rows * cols + 1);
		}
	batch_mode = 1;
}
void
clear_screen()
{	int	y;

	csr_x = csr_y = 0;
	dst_x = dst_y = 0;
	
	memset(dpy, ' ', rows * cols);
	memset(attr, MAKE_COLOR(WHITE, BLACK), rows * cols);

	if (batch_mode) {
		out_str("\n");
		}
	else {	
		out_str("\033[H\033[J");
		}
}
void
home_cursor()
{
	dst_x = dst_y = 0;
}		   
void
goto_rc(int r, int c)
{
	dst_y = r - 1;
	dst_x = c - 1;
}
void
print(char *fmt, ...)
{	va_list pvar;
	char	buf[BUFSIZ];

	va_start(pvar, fmt);
	vsprintf(buf, fmt, pvar);
	va_end(pvar);
	print_string(buf);
}
void
print_string(char *str)
{	int	orig_attr = dst_attr;

	if (!display_initialised) {
		fputs(str, stdout);
		return;
		}
	while (1) {
		switch (*str) {
		  case '\0':
			dst_attr = orig_attr;
		  	return;
		  case '\n':
			str++;
		  	if (dst_y < rows - 1) {
				if (dsp_crmode) {
					clear_to_eol();
					dst_x = 0;
					}
				dst_y++;
				}
			else {
				clear_to_eol();
				dst_x = cols - 1;
				}
			continue;
		  case '\r':
		  	dst_x = 0;
			str++;
			continue;
		  default:
		  	if (dst_y * cols + dst_x >= rows * cols)
				return;
		  	if (dpy[dst_y * cols + dst_x] != *str ||
			    attr[dst_y * cols + dst_x] != dst_attr) {
			    	if (auto_hilite)
					dst_attr = MAKE_COLOR(YELLOW, GET_BG(dst_attr));
					
			    	dpy[dst_y * cols + dst_x] = *str;
			    	attr[dst_y * cols + dst_x] = dst_attr;
				if (dst_x != csr_x || dst_y != csr_y) {
					goto_pos(dst_x, dst_y, csr_x, csr_y);
					}
				display_attribute();
				
				if (dst_x <= cols - 1) {
					dst_x++;
					out_char(*str);
					}
				else {
				  	if (dsp_autowrap && dst_y < rows - 1) {
						dst_y++;
						dst_x = 0;
						out_char(*str);
						}
					}
				str++;
				csr_x = dst_x;
				csr_y = dst_y;
				break;
			    	}
			if (dst_x < cols)
				dst_x++;
			else {
			  	if (dsp_autowrap && dst_y < rows - 1) {
					dst_y++;
					dst_x = 0;
					}
				}
			str++;
			break;
		  }
		}
}

void
print_char(int ch)
{	char	buf[2];
	buf[0] = ch;
	buf[1] = '\0';
	print_string(buf);
}
static void
goto_pos(int cx, int cy, int x, int y)
{	char	buf[64];

	if (csr_y == cy) {
		if (cx == x)
			return;

		if (cx > x)
			sprintf(buf, "\033[%dC", cx - x);
		else
			sprintf(buf, "\033[%dD", x - cx);
		out_str(buf);
		dst_y = csr_y = cy;
		dst_x = csr_x = cx;
		return;
		}
	dst_x = csr_x = cx;
	dst_y = csr_y = cy;
	if (cx == 0)
		sprintf(buf, "\033[%dH", cy + 1);
	else
		sprintf(buf, "\033[%d;%dH", cy + 1, cx + 1);
	out_str(buf);
}
void
set_attribute(int fg, int bg, int attr)
{
	dst_attr = MAKE_COLOR(fg, bg);
}
void
display_attribute()
{	int	need_semi = FALSE;
	char	buf[BUFSIZ];
	char	*bp;

	if (batch_mode || dst_attr == cur_attr)
		return;
		
	bp = buf;
	*bp++ = '\033';
	*bp++ = '[';
	if ((dst_attr ^ cur_attr) & ATTR_BOLD) {
		if ((dst_attr & ATTR_BOLD) == 0)
			*bp++ = '0';
		else
			*bp++ = '1';
		need_semi = TRUE;
		}
	if (GET_FG(dst_attr) != GET_FG(cur_attr)) {
		if (need_semi)
			*bp++ = ';';
		sprintf(bp, "3%d", GET_FG(dst_attr));
		bp += strlen(bp);
		need_semi = TRUE;
		}
	if (GET_BG(dst_attr) != GET_BG(cur_attr)) {
		if (need_semi)
			*bp++ = ';';
		sprintf(bp, "4%d", GET_BG(dst_attr));
		bp += strlen(bp);
		need_semi = TRUE;
		}
	*bp++ = 'm';
	*bp = '\0';
	out_str(buf);
	cur_attr = dst_attr;
}
/* VARARGS4 */
void
mvprint(int row, int col, char *str, ...)
{	va_list	pvar;
	char	buf[BUFSIZ];

	va_start(pvar, str);
	if (display_initialised) {
		goto_rc(row, col+1);
		vsprintf(buf, str, pvar);
		print_string(buf);
		}
	else {
		vprintf(str, pvar);
		}
	va_end(pvar);
}
void
clear_to_end_of_screen()
{	int	y;

	if (dst_x != csr_x || dst_y != csr_y) {
		goto_pos(dst_x, dst_y, csr_x, csr_y);
		}

	for (y = csr_y + 1; y < rows; y++) {
		memset(&dpy[y * cols], ' ', cols);
		memset(&attr[y * cols], MAKE_COLOR(WHITE, BLACK), cols);
		}
	display_attribute();
	out_str("\033[J");
	refresh();
}
void
clear_to_eol()
{	int	x;
	char	*cp;

	if (dst_x != csr_x || dst_y != csr_y) {
		goto_pos(dst_x, dst_y, csr_x, csr_y);
		}
	if (csr_x >= cols)
		return;
	/***********************************************/
	/*   See  if  we  can  avoid  clear to end of  */
	/*   line.				       */
	/***********************************************/
	cp = &dpy[csr_y * cols + csr_x];
	for (x = csr_x; x < cols; x++, cp++) {
		if (*cp != ' ')
			break;
		}
	if (x < cols) {
		memset(&dpy[csr_y * cols + csr_x], ' ', cols - csr_x);
		display_attribute();
		if (!batch_mode)
			out_str("\033[K");
		}
}
void
refresh()
{
	goto_pos(dst_x, dst_y, csr_x, csr_y);
	out_flush();
	fflush(stdout);
}
void
refresh_fp(FILE *fp)
{	register int y, x;
	time_t	t;
	struct tm *tm;

	if (fp == NULL)
		return;

	t = time(NULL);
	tm = localtime(&t);
	fprintf(fp, "=== %04d-%02d-%02d %d:%02d:%02d\n", 
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	for (y = 0; y < rows; y++) {
		char *cp = &dpy[y * cols];
		for (x = 0; x < cols; x++)
			fputc(*cp++, fp);
		fputc('\n', fp);
		}
	fflush(fp);
}
static unsigned long long *oldvals;
static int oldsize;

void
print__number(int idx, int width, int is_signed, int percent,
	unsigned long long v, unsigned long long v0, unsigned long divisor)
{	char	buf[BUFSIZ];

	if (idx >= oldsize) {
		if (oldsize == 0) {
			oldsize = 64;
			oldvals = (unsigned long long *) chk_alloc(oldsize * sizeof(unsigned long long));
			}
		else {
			oldsize = idx + 4;
			oldvals = (unsigned long long *) chk_realloc(oldvals, oldsize * sizeof(unsigned long long));
			}
		}

	if (mode_clear == 2 && idx == 0)
		mode_clear = 0;
	if (mode_clear == 1 || mode_clear == 2) {
		oldvals[idx] = v;
		mode_clear = 2;
		}
	if (percent) {
		print_number_string("%5.1f%%", (100. * (float) (v - v0)) / divisor);
		return;
		}

	if (mode == MODE_COUNT) {
		v -= oldvals[idx];
		v0 = 0;
		}

	if (mode == MODE_ABS) {
		if (width <= 7 && v >= 1000000) {
			print("%*.*s", width - 5, width - 5, " ");
//		print_number_string("%*lluM", width, v / (1024 * 1024));
			print_ranged2(v, v0);
			}
		else {
			snprintf(buf, sizeof buf, is_signed ? "%*lld" : "%*llu ", width, v);
			print_number3(buf, v, v0);
			}
		}
	else if (mode == MODE_RANGED) {
		print_ranged(v);
		print(" ");
		}
	else {
		v = v - v0;
		print_number_string(is_signed ? "%*lld" : "%*llu ", width, v);
		}
}

void
print_number(int idx, int width, int is_signed, unsigned long long v, unsigned long long v0)
{
	print__number(idx, width, is_signed, FALSE, v, v0, 0);
}

void
print_number_string(char *str, ...)
{	char	buf[BUFSIZ];
	char	*cp;
	va_list args;

	va_start(args, str);
	vsprintf(buf, str, args);
	va_end(args);
	cp = buf;
	while (*cp) {
		if (isdigit(*cp) || *cp == ',') {
			set_attribute(YELLOW, BLACK, 0);
			while (isdigit(*cp) || *cp == ',')
				print_char(*cp++);
			}
		else {
			set_attribute(GREEN, BLACK, 0);
			while (*cp && !isdigit(*cp))
				print_char(*cp++);
			}
		}
}
void
out_str(char *str)
{	int	len;

	if (batch_mode)
		return;

	len = strlen(str);
	if (oused + len < osize) {
		memcpy(&obuf[oused], str, len);
		oused += len;
		return;
		}
	while (len-- > 0)
		out_char(*str++);
}
void
out_char(int ch)
{
	if (batch_mode)
		return;

	if (oused + 1 >= osize)
		out_flush();
	obuf[oused++] = ch;
}
void
out_flush()
{
	fflush(stdout);
	write(1, obuf, oused);
	oused = 0;
}
void
snapshot()
{	register int y, x;
	char	*dp;

	if (history == NULL)
		return;

	dp = &history[h * rows * cols];
	for (y = 0; y < rows; y++) {
		memcpy(dp, &dpy[y * cols], cols);
		dp += cols;
		}
	h = (h + 1) % MAX_HIST;
}
/**********************************************************************/
/*   Snap the history records to a file.			      */
/**********************************************************************/
void
snap_to_file(FILE *fp)
{	int	h1 = (h + 1) % MAX_HIST;
	int	i, j;
	time_t	t;
	struct tm *tm;
	
	t = time(NULL);
	tm = localtime(&t);
	fprintf(fp, "=== Snapshot at %04d-%02d-%02d %d:%02d:%02d\n", 
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	for (i = 0; i < MAX_HIST; i++) {
		char *cp = &history[rows * cols * h1];
		h1 = (h1 + 1) % MAX_HIST;
		if (*cp == '\0')
			continue;
		for (j = 0; j < rows; j++) {
			fwrite(cp, cols, 1, fp);
			cp += cols;
			fputc('!', fp);
			fputc('\n', fp);
			}
		fputs("===\n", fp);
		}

	fflush(fp);
}

