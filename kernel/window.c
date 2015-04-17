
#include <kernel.h>

typedef struct _buffer_window
{
	unsigned char buffer[2 * WINDOW_TOTAL_WIDTH * WINDOW_TOTAL_HEIGHT];
} buffer_window;

buffer_window kernel_buffer_window;

void write_char(int offset, unsigned char c)
{
	char d = c;
	if (c == '\t' || c == '\r' || c == '\n')
		d = ' ';
	poke_w((MEM_ADDR)((WORD*)WINDOW_BASE_ADDR + offset), d | 0x0F00);
	poke_w((MEM_ADDR)((WORD*)kernel_buffer_window.buffer + offset), c | 0x0F00);
}

unsigned char read_char(int offset)
{
	return peek_b((MEM_ADDR)((WORD*)kernel_buffer_window.buffer + offset));
}

/* sets the currect cursor location to black ' ' */
void move_cursor(WINDOW* wnd, int x, int y)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);
	
	assert(x >= 0 && y >= 0 && x < wnd->width && y < wnd->height);
	wnd->cursor_x = x;
	wnd->cursor_y = y;

	ENABLE_INTR(saved_if); 
}


void remove_cursor(WINDOW* wnd)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	write_char(WINDOW_OFFSET(wnd, wnd->cursor_x, wnd->cursor_y), ' ');

	ENABLE_INTR(saved_if); 
}


void show_cursor(WINDOW* wnd)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	write_char(WINDOW_OFFSET(wnd, wnd->cursor_x, wnd->cursor_y), wnd->cursor_char);

	ENABLE_INTR(saved_if); 
}


void clear_window(WINDOW* wnd)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	int offset_to_next_line = WINDOW_TOTAL_WIDTH - wnd->width;
	int h_end, w_end;
	int offset = WINDOW_OFFSET(wnd, 0, 0);
	for (h_end = offset + wnd->height * WINDOW_TOTAL_WIDTH; offset < h_end; offset += offset_to_next_line)
	{
		for (w_end = offset + wnd->width; offset < w_end; offset++)
		{
			write_char(offset, ' ');
		}
	}
	move_cursor(wnd, 0, 0);
	show_cursor(wnd);

	ENABLE_INTR(saved_if);
}

/* scrolls the window up lines number of lines, also moving the cursor */
/* returns TRUE if scrolling would move the cursor off screen, FALSE otherwise */
/* if cursor would move offscreen it is reset to position 0,0 */
BOOL scroll_window(WINDOW* wnd, int lines)
{
	int i;
	int dst = WINDOW_OFFSET(wnd, 0, 0);
	int src = dst + lines * WINDOW_TOTAL_WIDTH;
	int end = dst + wnd->height * WINDOW_TOTAL_WIDTH;
	for( ; src < end; src += WINDOW_TOTAL_WIDTH, dst += WINDOW_TOTAL_WIDTH)
	{
		for(i = 0; i < wnd->width; i++)
			write_char(dst + i, read_char(src + i));
	}
	for( ; dst < end; dst += WINDOW_TOTAL_WIDTH)
	{
		for(i = 0; i < wnd->width; i++)
			write_char(dst + i, ' ');
	}

	if (wnd->cursor_y - lines >= 0)
	{
		move_cursor(wnd, wnd->cursor_x, wnd->cursor_y - lines);
		return FALSE;
	}
	else
	{
		move_cursor(wnd, 0, 0);
		show_cursor(wnd);
		return TRUE;
	}
}

void remove_char(WINDOW* wnd)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	unsigned char c;

	if (wnd->cursor_x != 0)
	{
		// removing within a single line
		c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
		// remove single character
		remove_cursor(wnd);
		move_cursor(wnd, wnd->cursor_x - 1, wnd->cursor_y);

		if (c == '\t')
		{
			c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			// removing tabs
			while (c == '\t' && wnd->cursor_x % TAB_SIZE != 0)
			{
				remove_cursor(wnd);
				move_cursor(wnd, wnd->cursor_x - 1, wnd->cursor_y);
				c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			}
		}
	}
	else if (wnd->cursor_y > 0)
	{
		// removing on previous line if it exists
		c = read_char(WINDOW_OFFSET(wnd, wnd->width - 1, wnd->cursor_y - 1));
		// move cursor to end of new line, removing single character
		remove_cursor(wnd);
		move_cursor(wnd, wnd->width - 1, wnd->cursor_y - 1);

		if (c == '\n' || c == '\r')
		{
			c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			// removing newlines
			while ((c == '\n' || c == '\r') && wnd->cursor_x > 0)
			{
				remove_cursor(wnd);
				move_cursor(wnd, wnd->cursor_x - 1, wnd->cursor_y);
				c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			}
		}
		else if (c == '\t')
		{
			c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			// removing tabs
			while (c == '\t' && wnd->cursor_x % TAB_SIZE != 0)
			{
				remove_cursor(wnd);
				move_cursor(wnd, wnd->cursor_x - 1, wnd->cursor_y);
				c = read_char(WINDOW_OFFSET(wnd, wnd->cursor_x - 1, wnd->cursor_y));
			}
		}
	}

	show_cursor(wnd);

	ENABLE_INTR(saved_if); 
}

void output_char(WINDOW* wnd, unsigned char c)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	int i;
	int write = TRUE;
	int offset;
	int cursor_old_x = wnd->cursor_x;
	// int cursor_old_y = wnd->cursor_y;
	int cursor_new_x = wnd->cursor_x;
	int cursor_new_y = wnd->cursor_y;

	switch (c)
	{
		case '\n':
		case '\r':
			cursor_new_x = 0;
			cursor_new_y++;
			break;
		case '\t':
			cursor_new_x += TAB_SIZE - cursor_new_x % TAB_SIZE;
			if (cursor_new_x >= wnd->width)
			{
				cursor_new_x = 0;
				cursor_new_y++;
			}
			break;
		default:
			cursor_new_x++;
			if (cursor_new_x >= wnd->width)
			{
				cursor_new_x = 0;
				cursor_new_y++;
			}
			break;
	}

	remove_cursor(wnd);

	if (cursor_new_y >= wnd->height)
	{
		write = !scroll_window(wnd, cursor_new_y - wnd->height + 1);
		cursor_new_y -= cursor_new_y - wnd->height + 1;
	}

	if (write)
	{
		/* save current cursor address before move_cursor */
		offset = WINDOW_OFFSET(wnd, wnd->cursor_x, wnd->cursor_y);

		move_cursor(wnd, cursor_new_x, cursor_new_y);

		if (cursor_new_x == 0)
		{
			// writing across a line, fill rest of line with said char
			for(i = 0; i + cursor_old_x < wnd->width; i++)
				write_char(offset + i, c);
		}
		else
		{
			// writing within a line, fill till get to new position
			for(i = 0; i + cursor_old_x < cursor_new_x; i++)
				write_char(offset + i, c);
		}
	}

	show_cursor(wnd);

	ENABLE_INTR(saved_if); 
}



void output_string(WINDOW* wnd, const char *str)
{
	// volatile int saved_if;
	// DISABLE_INTR(saved_if);

	while(*str != '\0')
	{
		output_char(wnd, *str++);
	}
	
	// ENABLE_INTR(saved_if); 
}


/*
 * There is no need to make any changes to the code below,
 * however, you are encouraged to at least look at it!
 */
#define MAXBUF (sizeof(long int) * 8)		 /* enough for binary */

char *printnum(char *b, unsigned int u, int base,
	       BOOL negflag, int length, BOOL ladjust,
	       char padc, BOOL upcase)
{
    char	buf[MAXBUF];	/* build number here */
    char	*p = &buf[MAXBUF-1];
    int		size;
    char	*digs;
    static char up_digs[] = "0123456789ABCDEF";
    static char low_digs[] = "0123456789abcdef";
    
    digs = upcase ? up_digs : low_digs;
    do {
	*p-- = digs[ u % base ];
	u /= base;
    } while( u != 0 );
    
    if (negflag)
	*b++ = '-';
    
    size = &buf [MAXBUF - 1] - p;
    
    if (size < length && !ladjust) {
	while (length > size) {
	    *b++ = padc;
	    length--;
	}
    }
    
    while (++p != &buf [MAXBUF])
	*b++ = *p;
    
    if (size < length) {
	/* must be ladjust */
	while (length > size) {
	    *b++ = padc;
	    length--;
	}
    }
    return b;
}


/*
 *  This version implements therefore following printf features:
 *
 *	%d	decimal conversion
 *	%u	unsigned conversion
 *	%x	hexadecimal conversion
 *	%X	hexadecimal conversion with capital letters
 *	%o	octal conversion
 *	%c	character
 *	%s	string
 *	%m.n	field width, precision
 *	%-m.n	left adjustment
 *	%0m.n	zero-padding
 *	%*.*	width and precision taken from arguments
 *
 *  This version does not implement %f, %e, or %g.  It accepts, but
 *  ignores, an `l' as in %ld, %lo, %lx, and %lu, and therefore will not
 *  work correctly on machines for which sizeof(long) != sizeof(int).
 *  It does not even parse %D, %O, or %U; you should be using %ld, %o and
 *  %lu if you mean long conversion.
 *
 *  This version implements the following nonstandard features:
 *
 *	%b	binary conversion
 *
 */


#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define ctod(c) ((c) - '0')


void vsprintf(char *buf, const char *fmt, va_list argp)
{
    char		*p;
    char		*p2;
    int			length;
    int			prec;
    int			ladjust;
    char		padc;
    int			n;
    unsigned int        u;
    int			negflag;
    char		c;
    
    while (*fmt != '\0') {
	if (*fmt != '%') {
	    *buf++ = *fmt++;
	    continue;
	}
	fmt++;
	if (*fmt == 'l')
	    fmt++;	     /* need to use it if sizeof(int) < sizeof(long) */
	
	length = 0;
	prec = -1;
	ladjust = FALSE;
	padc = ' ';
	
	if (*fmt == '-') {
	    ladjust = TRUE;
	    fmt++;
	}
	
	if (*fmt == '0') {
	    padc = '0';
	    fmt++;
	}
	
	if (isdigit (*fmt)) {
	    while (isdigit (*fmt))
		length = 10 * length + ctod (*fmt++);
	}
	else if (*fmt == '*') {
	    length = va_arg (argp, int);
	    fmt++;
	    if (length < 0) {
		ladjust = !ladjust;
		length = -length;
	    }
	}
	
	if (*fmt == '.') {
	    fmt++;
	    if (isdigit (*fmt)) {
		prec = 0;
		while (isdigit (*fmt))
		    prec = 10 * prec + ctod (*fmt++);
	    } else if (*fmt == '*') {
		prec = va_arg (argp, int);
		fmt++;
	    }
	}
	
	negflag = FALSE;
	
	switch(*fmt) {
	case 'b':
	case 'B':
	    u = va_arg (argp, unsigned int);
	    buf = printnum (buf, u, 2, FALSE, length, ladjust, padc, 0);
	    break;
	    
	case 'c':
	    c = va_arg (argp, int);
	    *buf++ = c;
	    break;
	    
	case 'd':
	case 'D':
	    n = va_arg (argp, int);
	    if (n >= 0)
		u = n;
	    else {
		u = -n;
		negflag = TRUE;
	    }
	    buf = printnum (buf, u, 10, negflag, length, ladjust, padc, 0);
	    break;
	    
	case 'o':
	case 'O':
	    u = va_arg (argp, unsigned int);
	    buf = printnum (buf, u, 8, FALSE, length, ladjust, padc, 0);
	    break;
	    
	case 's':
	    p = va_arg (argp, char *);
	    if (p == (char *)0)
		p = "(NULL)";
	    if (length > 0 && !ladjust) {
		n = 0;
		p2 = p;
		for (; *p != '\0' && (prec == -1 || n < prec); p++)
		    n++;
		p = p2;
		while (n < length) {
		    *buf++ = ' ';
		    n++;
		}
	    }
	    n = 0;
	    while (*p != '\0') {
		if (++n > prec && prec != -1)
		    break;
		*buf++ = *p++;
	    }
	    if (n < length && ladjust) {
		while (n < length) {
		    *buf++ = ' ';
		    n++;
		}
	    }
	    break;
	    
	case 'u':
	case 'U':
	    u = va_arg (argp, unsigned int);
	    buf = printnum (buf, u, 10, FALSE, length, ladjust, padc, 0);
	    break;
	    
	case 'x':
	    u = va_arg (argp, unsigned int);
	    buf = printnum (buf, u, 16, FALSE, length, ladjust, padc, 0);
	    break;
	    
	case 'X':
	    u = va_arg (argp, unsigned int);
	    buf = printnum (buf, u, 16, FALSE, length, ladjust, padc, 1);
	    break;
	    
	case '\0':
	    fmt--;
	    break;
	    
	default:
	    *buf++ = *fmt;
	}
	fmt++;
    }
    *buf = '\0';
}


void wprintf(WINDOW* wnd, const char *fmt, ...)
{
	// volatile int saved_if;
	// DISABLE_INTR(saved_if);
	
    va_list	argp;
    char	buf[160];

    va_start(argp, fmt);
    vsprintf(buf, fmt, argp);
    output_string(wnd, buf);
    va_end(argp);

    // ENABLE_INTR(saved_if); 
}


static WINDOW kernel_window_def = {0, 0, 80, 25, 0, 0, ' '};
WINDOW* kernel_window = &kernel_window_def;


void kprintf(const char *fmt, ...)
{
	// volatile int saved_if;
	// DISABLE_INTR(saved_if);

    va_list	  argp;
    char	  buf[160];

    va_start(argp, fmt);
    vsprintf(buf, fmt, argp);
    output_string(kernel_window, buf);
    va_end(argp);

    // ENABLE_INTR(saved_if);
}


