#include <kernel.h>


// determines location to print primes
static WINDOW shell_window_def = {0, 10, 80 - MAZE_WIDTH, 14, 0, 0, '_'};
WINDOW* shell_window = &shell_window_def;

#define MAX_COMMANDS 32

// commands use a similar interface to c programs
// here argc is the length of the buffer argv
// argv contains arguments separated by null chars and null terminated
// a command shall be unused if it's function pointer is NULL
typedef struct _command
{
	char *name;
	void (*func) (int argc, char **argv);
	char *description;
} command;
command cmd[MAX_COMMANDS + 1];

#define INPUT_BUFFER_LENGTH 160

typedef struct _input_buffer
{
	char buffer[INPUT_BUFFER_LENGTH + 1];
} input_buffer;

void clear_in_buf(input_buffer *in_buf)
{
	k_memset(in_buf->buffer, 0, INPUT_BUFFER_LENGTH + 1);
}

int clean_in_buf(input_buffer *in_buf)
{
	int i = 0;
	int j = 0;
	int in_arg = FALSE;

	while (j < INPUT_BUFFER_LENGTH && in_buf->buffer[j] != '\0')
	{
		if (in_buf->buffer[j] == ' ' || 
				in_buf->buffer[j] == '\t' || 
				in_buf->buffer[j] == '\n' || 
				in_buf->buffer[j] == '\r')
		{
			// skip white space
			if (in_arg == TRUE)
			{
				in_buf->buffer[i++] = '\0';
			}
			in_arg = FALSE;
			j++;
		}
		else
		{
			in_buf->buffer[i++] = in_buf->buffer[j++];
			in_arg = TRUE;
		}
	}
	in_buf->buffer[i] = '\0';
	return i;
}

void print_commands(WINDOW *wnd)
{
	int i;

	wprintf(wnd, "Available commands\n");

	for (i = 0; cmd[i].func != NULL; i++)
	{
		wprintf(wnd, " - %s:\t%s\n", cmd[i].name, cmd[i].description);
	}
}

void win_printc(WINDOW *wnd, char c)
{
	if (c == '\b')
	{
		remove_char(wnd);
	}
	else
	{
		wprintf(wnd, "%c", c);
	}
}

// returns the length of the command buffer used
// formats the command buffer to contain non-white space characters separated by the null char
// prints the entered characters and returns when the command buffer is filled or \n or \r received
int get_input(input_buffer *in_buf)
{
	int i = 0;
	char c = '\0';
	char char0 = '\0';
	Keyb_Message msg;
	while(i < INPUT_BUFFER_LENGTH && c != '\n' && c != '\r')
	{
		send(keyb_port, (void *)&msg);

		c = *(msg.key_buffer);

		if (i == 0)
			char0 = c;

		if (c == '\b' && i > 0)
		{
			in_buf->buffer[--i] = '\0';
			win_printc(shell_window, c);
		}
		else if (c != '\b')
		{
			in_buf->buffer[i++] = c;
			win_printc(shell_window, c);
		}
	}
	in_buf->buffer[0] = char0;
	in_buf->buffer[i] = '\0';
	return i;
}

command* find_command(char * in)
{
	command *ci;
	for (ci = cmd; ci->func != NULL; ci++)
	{
		if (k_strcmp(ci->name, in) == 0)
		{
			break;
		}
	}
	return ci;
}

int setup_args(int len, const char *buf, char **argv)
{
	int i, j;
	int in_arg = FALSE;
	for (i = j = 0; i < len; i++)
	{
		if (buf[i] != '\0')
		{
			// reading through part of an argument, add to argv if this is the first char
			if (in_arg == FALSE)
			{
				argv[j++] = (char *)&buf[i];
				in_arg = TRUE;
			}
		}
		else
		{
			// found a null char, no longer in an argument
			in_arg = FALSE;
		}
	}
	return j;
}

void shell_process(PROCESS proc, PARAM param)
{
	int i;
	command *c;
	input_buffer in_buf;
	char *argv[INPUT_BUFFER_LENGTH];

	while(1)
	{
		clear_in_buf(&in_buf);

		get_input(&in_buf);

		// wprintf(shell_window, "%s", in_buf.buffer);

		i = clean_in_buf(&in_buf);

		// wprintf(shell_window, "%s %s\n", in_buf.buffer, in_buf.buffer + 5);

		// wprintf(shell_window, "input size %d\n", i);

		c = find_command(in_buf.buffer);

		// wprintf(shell_window, "%s\n", c->name);

		// print_commands(shell_window);

		if (c->func == NULL)
		{
			wprintf(shell_window, "Unknown Command %s\n", in_buf.buffer);
		}
		else
		{
			// wprintf(shell_window, "%s\n", c->name);

			i = setup_args(i, in_buf.buffer, argv);

			(c->func)(i, argv);
		}
		k_memset(in_buf.buffer, 0, INPUT_BUFFER_LENGTH+1);
	}
}

void print_commands_func(int argc, char **argv)
{
	print_commands(shell_window);
}

void print_process_func(int argc, char **argv)
{
	print_all_processes(shell_window);
}

void sleep_func(int argc, char **argv)
{
	if (argc > 1)
	{
		wprintf(shell_window, "Sleeping: ");
		sleep(atoi(argv[1]));
		wprintf(shell_window, "woke after %d\n", atoi(argv[1]));
	}
	else
	{
		wprintf(shell_window, "Missing argument to sleep\n");
	}
}

void clear_func(int argc, char **argv)
{
	clear_window(shell_window);
}

void echo_func(int argc, char **argv)
{
	int i = 1;
	while (i < argc)
		wprintf(shell_window, "%s ", argv[i++]);
	if (argc > 1)
		wprintf(shell_window, "\n");
}

void pacman_func(int argc, char **argv)
{
	WINDOW pacman_window;
	pacman_window.x = WINDOW_TOTAL_WIDTH - MAZE_WIDTH;
	pacman_window.y = WINDOW_TOTAL_HEIGHT - MAZE_HEIGHT - 1;
	move_cursor(&pacman_window, 0, 0);

	if (argc > 1)
	{
		wprintf(shell_window, "Starting pacman with %d ghosts\n", atoi(argv[1]));
		init_pacman(&pacman_window, atoi(argv[1]));
	}
	else
		wprintf(shell_window, "Missing argument to pacman\n");
}

BOOL init_command(char *name, void (*func) (int argc, char **argv), char *description, int i) 
{
	if (i >= MAX_COMMANDS) 
	{
		wprintf(shell_window, "Too many processes: not adding %s\n", name);
		return FALSE;
	}
	cmd[i].name = name;
	cmd[i].func = func;
	cmd[i].description = description;
	return TRUE;
}

void init_shell()
{
	int i = 0;

	// init used commands
	init_command("help", print_commands_func, "Prints all commands", i++);
	init_command("prproc", print_process_func, "Prints all processes", i++);
	init_command("sleep", sleep_func, "Sleeps for the given duration", i++);
	init_command("clear", clear_func, "Clears the screen", i++);
	init_command("echo", echo_func, "Prints its arguments", i++);
	init_command("pacman", pacman_func, "Starts pacman with given number of ghosts", i++);

	// init unused commands
	while (i < MAX_COMMANDS)
	{
		init_command("unused", NULL, "-", i++);
	}
	cmd[MAX_COMMANDS].name = "NULL";
	cmd[MAX_COMMANDS].func = NULL;
	cmd[MAX_COMMANDS].description = "NULL";

	// print_commands(shell_window);

	create_process (shell_process, 3, 0, "Shell process");
}
