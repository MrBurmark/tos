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

#define COMMAND_BUFFER_LENGTH 160

char command_buffer[COMMAND_BUFFER_LENGTH + 1];

void print_commands(WINDOW *window)
{
	int i;
	for (i = 0; cmd[i].func != NULL; i++)
	{
		wprintf(window, "cmd: %s\t%d\n", cmd[i].name, (LONG)cmd[i].func);
	}
}

// returns the length of the command buffer used
// formats the command buffer to contain non-white space characters separated by the null char
// prints the entered characters and returns when the command buffer is filled or \n or \r received
int get_input()
{
	int i = 0;
	char c;
	Keyb_Message msg;
	while(i < COMMAND_BUFFER_LENGTH)
	{
		send(keyb_port, (void *)&msg);

		c = *(msg.key_buffer);

		switch (c)
		{
			case '\n':
			case '\r':
				command_buffer[i++] = '\0';
				break;
			case '\t':
			case ' ':
				if (command_buffer[i] != '\0' && i > 0)
				{
					command_buffer[i++] = '\0';
					command_buffer[i] = '\0';
				}
				break;
			default:
				command_buffer[i++] = c;
				command_buffer[i] = '_';
				break;
		}

		wprintf(shell_window, "%c", c);

		if (c == '\n' || c == '\r')
		{
			return i;
		}
	}
	command_buffer[i-1] = '\0';
	return i;
}

command* find_command()
{
	int i;

	for (i = 0; cmd[i].func != NULL; i++)
	{
		// wprintf(shell_window, "'%s', '%s'\n", c->name, command_buffer);
		if (k_strcmp(cmd[i].name, command_buffer))
		{
			return cmd + i;
		}
	}
	return cmd + MAX_COMMANDS;
}

void shell_process(PROCESS proc, PARAM param)
{
	int i;
	command *c;

	while(1)
	{
		i = get_input();

		// wprintf(shell_window, "input size %d\n", i);

		c = find_command();

		wprintf(shell_window, "%s\n", c - cmd);

		// print_commands(shell_window);

		if (c->func == NULL)
		{
			wprintf(shell_window, "Unknown Command %s\n", command_buffer);
		}
		else
		{
			// wprintf(shell_window, "%s\n", c->name);

			(c->func)(i, command_buffer);
		}
		k_memset(command_buffer, 0, COMMAND_BUFFER_LENGTH+1);
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
	init_command("print_commands", print_commands_func, i++);
	init_command("print_process", print_process_func, i++);
	init_command("sleep", sleep_func, i++);
	init_command("clear", clear_func, i++);
	init_command("echo", echo_func, i++);
	init_command("pacman", pacman_func, i++);

	// print_commands(shell_window);

	create_process (shell_process, 3, 0, "Shell process");
}
