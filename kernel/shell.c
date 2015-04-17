#include <kernel.h>


// determines location to print primes
static WINDOW shell_window_def = {0, 10, 80, 14, 0, 0, '_'};
WINDOW* shell_window = &shell_window_def;

#define MAX_COMMANDS 32

// commands use a similar interface to c programs
// here argc is the length of the buffer argv
// argv contains arguments separated by null chars and null terminated
// a command shall be unused if it's function pointer is NULL
typedef struct _command
{
	char *name;
	void (*func) (int argc, char *argv);
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

	// skip initial white space
	while ((in_buf->buffer[j] == ' ' || 
			in_buf->buffer[j] == '\t' || 
			in_buf->buffer[j] == '\n' || 
			in_buf->buffer[j] == '\r') &&
			j < INPUT_BUFFER_LENGTH)
	{
		j++;
	}

	while (j < INPUT_BUFFER_LENGTH && in_buf->buffer[j] != '\0')
	{
		if ((in_buf->buffer[j] == ' ' || 
				in_buf->buffer[j] == '\t' || 
				in_buf->buffer[j] == '\n' || 
				in_buf->buffer[j] == '\r') &&
				j < INPUT_BUFFER_LENGTH)
		{
			// skip central white space
			in_buf->buffer[i++] = '\0';
			
			while ((in_buf->buffer[j] == ' ' || 
					in_buf->buffer[j] == '\t' || 
					in_buf->buffer[j] == '\n' || 
					in_buf->buffer[j] == '\r') &&
					j < INPUT_BUFFER_LENGTH)
			{
				j++;
			}
		}
		else
		{
			in_buf->buffer[i++] = in_buf->buffer[j++];
		}
	}
	in_buf->buffer[i] = '\0';
	return i;
}

void print_commands(WINDOW *wnd)
{
	int i;
	for (i = 0; cmd[i].func != NULL; i++)
	{
		wprintf(wnd, "cmd: %s\t%d\n", cmd[i].name, (LONG)cmd[i].func);
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

		// wprintf(shell_window, "\n%d\n", i);
		// wprintf(shell_window, "\n%c\n", in_buf->buffer[0]);
	}
	// wprintf(shell_window, "\n%d\n", i);
	in_buf->buffer[0] = char0;
	in_buf->buffer[i] = '\0';
	return i;
}

command* find_command(char * in)
{
	int i;

	for (i = 0; cmd[i].func != NULL; i++)
	{
		// wprintf(shell_window, "'%s', '%s'\n", c->name, command_buffer);
		if (k_strcmp(cmd[i].name, in))
		{
			return cmd + i;
		}
	}
	return cmd + i;
}

void shell_process(PROCESS proc, PARAM param)
{
	int i;
	command *c;
	input_buffer in_buf;

	char test[5] = "heys";

	wprintf(shell_window, "%s\n", test);
	wprintf(shell_window, "%s\n", test);

	while(1)
	{
		clear_in_buf(&in_buf);

		get_input(&in_buf);

		wprintf(shell_window, "%s", in_buf.buffer);

		i = clean_in_buf(&in_buf);

		wprintf(shell_window, "%s %s\n", in_buf.buffer, in_buf.buffer + 5);

		wprintf(shell_window, "input size %d\n", i);

		c = find_command(in_buf.buffer);

		wprintf(shell_window, "%s\n", c->name);

		// print_commands(shell_window);

		if (c->func == NULL)
		{
			wprintf(shell_window, "Unknown Command %s\n", in_buf.buffer);
		}
		else
		{
			// wprintf(shell_window, "%s\n", c->name);

			(c->func)(i, in_buf.buffer);
		}
		k_memset(in_buf.buffer, 0, INPUT_BUFFER_LENGTH+1);
	}
}

BOOL init_command(char *name, void (*func) (int argc, char *argv), int i) 
{
	if (i >= MAX_COMMANDS) 
	{
		wprintf(shell_window, "Too many processes: not adding %s\n", name);
		return FALSE;
	}
	cmd[i].name = name;
	cmd[i].func = func;
	return TRUE;
}

void print_commands_func(int argc, char *argv)
{
	print_commands(shell_window);
}

void print_process_func(int argc, char *argv)
{
	print_all_processes(shell_window);
}

void sleep_func(int argc, char *argv)
{
	if (argc > 7)
	{
		wprintf(shell_window, "Sleeping: ");
		sleep(atoi(argv+6));
		wprintf(shell_window, "woke after %d\n", atoi(argv+6));
	}
	else
	{
		wprintf(shell_window, "Missing argument to sleep\n");
	}
}

void clear_func(int argc, char *argv)
{
	clear_window(shell_window);
}

void echo_func(int argc, char *argv)
{
	if (argc > 6)
		wprintf(shell_window, "%s\n", argv+5);
}

void pacman_func(int argc, char *argv)
{
	if (argc > 8)
		init_pacman(shell_window, atoi(argv+7));
	else
		wprintf(shell_window, "Missing argument to pacman\n");
}

void init_shell()
{
	int i;

	// init all commands to unused
	for (i = 0; i < MAX_COMMANDS; i++)
	{
		init_command("unused", NULL, i);
	}
	cmd[MAX_COMMANDS].name = "NULL";
	cmd[MAX_COMMANDS].func = NULL;

	i = 0;
	// init used commands
	init_command("help", print_commands_func, i++);
	init_command("print_process", print_process_func, i++);
	init_command("sleep", sleep_func, i++);
	init_command("clear", clear_func, i++);
	init_command("echo", echo_func, i++);
	init_command("pacman", pacman_func, i++);

	// print_commands(shell_window);

	create_process (shell_process, 3, 0, "Shell process");
}
