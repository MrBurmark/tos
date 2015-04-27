
#include <kernel.h>

// determines location to print primes
// static WINDOW null_window_def = {0, 0, 80, 1, 0, 0, ' '};
// WINDOW* null_window = &null_window_def;

volatile unsigned int null_prime;
volatile BOOL prime_reset;
volatile unsigned int new_start;

volatile PROCESS process_to_kill;

void null_process(PROCESS proc, PARAM param)
{
	// WORD *pos = (WORD*)WINDOW_BASE_ADDR + WINDOW_OFFSET(null_window, null_window->cursor_x, null_window->cursor_y) + 11;
	unsigned int i = 3;
	unsigned int j;
	prime_reset = TRUE;
	new_start = param;
	null_prime = 2;

	while(1)
	{
		if (process_to_kill != NULL)
		{
			kill_process(process_to_kill, TRUE);
			process_to_kill = NULL;
		}

		if (prime_reset == TRUE)
		{
			i = (new_start > 2) ? new_start + 1 - new_start%2 : 3;
			prime_reset = FALSE;
		}

		// compute primes
		for(j = 3; j*j <= i; j += 2)
		{
			if(i % j == 0)
			{
				i += 2;
				j = 3;
			}
		}
		null_prime = i;

		// // wprintf(null_window, "\n%d", i); // causes the timer notifier to break
		// // for(j=0;j<0xffffff;j++);


		// // print primes safely
		// for (j=i; pos >= (WORD*)WINDOW_BASE_ADDR + WINDOW_OFFSET(null_window, null_window->cursor_x, null_window->cursor_y); pos--, j/=10)
		// {
		// 	poke_w((MEM_ADDR)pos, ('0' + (j % 10)) | 0x0F00);
		// }
		// pos += 12;

		i += 2;
	}
}

void init_null_process()
{
	process_to_kill = NULL;

	create_process (null_process, 0, 456198994, "Null process");
}
