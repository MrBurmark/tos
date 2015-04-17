#include <kernel.h>


// determines location to print primes
static WINDOW shell_window_def = {0, 10, 80 - MAZE_WIDTH, 14, 0, 0, '_'};
WINDOW* shell_window = &shell_window_def;

#define MAX_COMMANDS 32

// commands use the same interface as c programs
// a command shall be unused if it's function pointer is NULL
typedef struct _command
{
	char *name;
	int (*func) (int argc, char **argv);
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

int setup_args(int len, const input_buffer *in_buf, char **argv)
{
	int i, j;
	int in_arg = FALSE;
	for (i = j = 0; i < len; i++)
	{
		if (in_buf->buffer[i] != '\0')
		{
			// reading through part of an argument, add to argv if this is the first char
			if (in_arg == FALSE)
			{
				argv[j++] = (char *)&in_buf->buffer[i];
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

void clear_args(char **argv)
{
	k_memset(argv, 0, INPUT_BUFFER_LENGTH*sizeof(char *));
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
	Keyb_Message msg;
	msg.key_buffer = &c;

	while(i < INPUT_BUFFER_LENGTH && c != '\n' && c != '\r')
	{
		// get new character
		send(keyb_port, (void *)&msg);

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
	in_buf->buffer[i] = '\0';
	return i;
}

command* find_command(input_buffer *in_buf)
{
	command *ci;
	for (ci = cmd; ci->func != NULL; ci++)
	{
		if (k_strcmp(ci->name, in_buf->buffer) == 0)
		{
			break;
		}
	}
	return ci;
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

		i = clean_in_buf(&in_buf);

		c = find_command(&in_buf);

		if (c->func == NULL)
		{
			wprintf(shell_window, "Unknown Command %s\n", in_buf.buffer);
		}
		else
		{

			clear_args(argv);

			i = setup_args(i, &in_buf, argv);

			// run command
			i = (c->func)(i, argv);

			// check for error code returned
			if (i != 0)
				wprintf(shell_window, "%s exited with error code %d\n", c->name, i);
		}
		k_memset(in_buf.buffer, 0, INPUT_BUFFER_LENGTH+1);
	}
}

int print_commands_func(int argc, char **argv)
{
	print_commands(shell_window);
	return 0;
}

int print_process_func(int argc, char **argv)
{
	print_all_processes(shell_window);
	return 0;
}

int sleep_func(int argc, char **argv)
{
	if (argc > 1)
	{
		wprintf(shell_window, "Sleeping: ");
		sleep(atoi(argv[1]));
		wprintf(shell_window, "woke after %d\n", atoi(argv[1]));
		return 0;
	}
	else
	{
		wprintf(shell_window, "Missing argument to sleep\n");
		return 1;
	}
}

int clear_func(int argc, char **argv)
{
	clear_window(shell_window);
	return 0;
}

int echo_func(int argc, char **argv)
{
	int i = 1;
	while (i < argc)
		wprintf(shell_window, "%s ", argv[i++]);
	if (argc > 1)
		wprintf(shell_window, "\n");
	return 0;
}

int pacman_func(int argc, char **argv)
{
	WINDOW pacman_window;
	pacman_window.x = WINDOW_TOTAL_WIDTH - MAZE_WIDTH;
	pacman_window.y = WINDOW_TOTAL_HEIGHT - MAZE_HEIGHT - 1;
	move_cursor(&pacman_window, 0, 0);

	if (argc > 1)
	{
		wprintf(shell_window, "Starting pacman with %d ghosts\n", atoi(argv[1]));
		init_pacman(&pacman_window, atoi(argv[1]));
		return 0;
	}
	else
	{
		wprintf(shell_window, "Missing argument to pacman\n");
		return 1;
	}
}

BOOL init_command(char *name, int (*func) (int argc, char **argv), char *description, int i) 
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
	init_command("prtprc", print_process_func, "Prints all processes", i++);
	init_command("sleep", sleep_func, "Sleeps for the given duration", i++);
	init_command("clear", clear_func, "Clears the screen", i++);
	init_command("echo", echo_func, "Prints its arguments", i++);
	init_command("pacman", pacman_func, "Starts pacman with given number of ghosts", i++);

	// init unused commands
	while (i < MAX_COMMANDS)
	{
		init_command("----", NULL, "unused", i++);
	}
	cmd[MAX_COMMANDS].name = "NULL";
	cmd[MAX_COMMANDS].func = NULL;
	cmd[MAX_COMMANDS].description = "NULL";

	// print_commands(shell_window);

	create_process (shell_process, 3, 0, "Shell process");
}
