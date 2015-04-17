
#include <kernel.h>

// determines location to print primes
static WINDOW null_window_def = {0, 0, 80, 1, 0, 0, ' '};
WINDOW* null_window = &null_window_def;


void null_process(PROCESS proc, PARAM param)
{
	// WORD *pos = (WORD*)WINDOW_BASE_ADDR + WINDOW_OFFSET(null_window, null_window->cursor_x, null_window->cursor_y) + 11;
	// unsigned int i = param + 1 - param%2;
	// unsigned int j;
	while(1)
	{
		// // compute primes
		// for(j = 3; j*j <= i; j += 2)
		// {
		// 	if(i % j == 0)
		// 	{
		// 		i += 2;
		// 		j = 3;
		// 	}
		// }

		// // wprintf(null_window, "\n%d", i); // causes the timer notifier to break
		// // for(j=0;j<0xffffff;j++);


		// // print primes safely
		// for (j=i; pos >= (WORD*)WINDOW_BASE_ADDR + WINDOW_OFFSET(null_window, null_window->cursor_x, null_window->cursor_y); pos--, j/=10)
		// {
		// 	poke_w((MEM_ADDR)pos, ('0' + (j % 10)) | 0x0F00);
		// }
		// i += 2;
		// pos += 12;
		
	}
}

void init_null_process()
{
	create_process (null_process, 0, 456198994, "Null process");
}
