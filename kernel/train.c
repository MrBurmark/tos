
#include <kernel.h>

static WINDOW train_window_def = {0, 0, 80 - MAZE_WIDTH, 10, 0, 0, '_'};
WINDOW* train_wnd = &train_window_def;


command train_cmd[MAX_COMMANDS + 1];

PORT train_port;

int train_cmd_pause = 15;

#define UNKNOWN 0
#define GREEN 'G'
#define RED 'R'
#define RED_TRAIN 20
#define BLACK_TRAIN 10
#define CARGO_CAR 5
#define FORWARD 1
#define BACKWARD -1

typedef struct _track_status
{
	unsigned char s88_status;

} track_status;
track_status main_track_status;

struct _train_switch;
typedef struct _train_switch
{
	unsigned char id;
	unsigned char direction;
} train_switch;

typedef struct _train_train
{
	unsigned char id;
	unsigned char speed 	: 6;
	unsigned char direction : 2;
	unsigned char position;
	unsigned char destination;
} train_train;

// clears s88 memory in train controller, required before checking a segment
void clear_s88(track_status *trk)
{
	COM_Message msg;
	char out_buf[] = "R\r";
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (trk->s88_status == FALSE)
	{
		send(com_port, &msg);
		trk->s88_status = TRUE;
	}

	sleep(train_cmd_pause);
}

// returns true if segment occupied, false otherwise
// clears s88
BOOL check_segment(track_status *trk, int segment)
{
	COM_Message msg;
	char out_buf[16];
	char in_buf[3];
	msg.output_buffer = out_buf;
	msg.input_buffer = in_buf;
	msg.len_input_buffer = 3;
	
	k_sprintf(msg.output_buffer, "C%d\r", segment);

	clear_s88(trk);

	send(com_port, &msg);

	trk->s88_status = FALSE;

	sleep(train_cmd_pause);

	return msg.input_buffer[1] == '1' ? TRUE : FALSE;
}

// returns true if command could be executed, false otherwise
BOOL set_speed(train_train *trn, int speed)
{
	COM_Message msg;
	char out_buf[16];
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (speed < 0 || speed > 5)
		return FALSE;

	k_sprintf(msg.output_buffer, "L%dS%d\r", trn->id, speed);

	send(com_port, &msg);

	trn->speed = (unsigned char)speed;

	sleep(train_cmd_pause);

	return TRUE;
}

// returns true if command could be executed, false otherwise
BOOL change_direction(train_train *trn)
{
	COM_Message msg;
	char out_buf[16];
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (trn->speed != 0)
		return FALSE;

	k_sprintf(msg.output_buffer, "L%dD\r", trn->id);

	send(com_port, &msg);

	trn->direction = -trn->direction;

	sleep(train_cmd_pause);

	return TRUE;
}

// returns true if command could be executed, false otherwise
// only accepts direction 'G' or 'R'
// always tries to set direction even if switch should already be set to direction
BOOL set_switch(train_switch *swt, unsigned char direction)
{
	COM_Message msg;
	char out_buf[16];
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (direction != GREEN && direction != RED)
		return FALSE;

	k_sprintf(msg.output_buffer, "M%d%c\r", swt->id, direction);

	send(com_port, &msg);

	swt->direction = direction;

	sleep(train_cmd_pause);

	return TRUE;
}


//**************************
//run the train application
//**************************

void train_process(PROCESS self, PARAM param)
{
	int i;
	Train_Message *msg;
	PROCESS sender;
	command *cmd;

	while(1)
	{
		wprintf(train_wnd, "train> ");

		msg = (Train_Message *)receive(&sender);

		cmd = find_command(train_cmd, msg->argv[0]);

		if (cmd->func == NULL)
		{
			wprintf(train_wnd, "Unknown Command %s\n", msg->argv[0]);
		}
		else
		{
			// run command
			i = (cmd->func)(msg->argc, msg->argv);

			// check for error code returned
			if (i != 0)
				wprintf(train_wnd, "%s exited with code %d\n", cmd->name, i);
		}

		reply(sender);
	}
}

// prints the available train "shell" commands
int print_train_commands_func(int argc, char **argv)
{
	print_commands(train_wnd, train_cmd);
	return 0;
}

// clears the shell window
int clear_train_func(int argc, char **argv)
{
	clear_window(train_wnd);
	return 0;
}

// runs the train command in argv[1] as is, unchecked
int run_command_func(int argc, char **argv)
{
	int len;
	COM_Message msg;
	char out_buf[10];
	char in_buf[3];
	msg.output_buffer = out_buf;
	msg.input_buffer = in_buf;
	msg.len_input_buffer = 0;

	len = k_strlen(argv[1]);

	if (len < 9)
	{
		wprintf(train_wnd, "cmd: %s ", argv[1]);

		if (argv[1][0] == 'C')
		{
			msg.len_input_buffer = 3;
		}

		k_memcpy(out_buf, argv[1], len);
		out_buf[len] = '\r';
		out_buf[len+1] = '\0';

		send(com_port, &msg);

		if (argv[1][0] == 'C' )
		{
			wprintf(train_wnd, "%c", msg.input_buffer[1]);
		}
		wprintf(train_wnd, "\n");

		return 0;
	}
	else
	{
		return 1;
	}
}

// initializes train related data structures, and train shell commands
// creates the train process
void init_train(WINDOW* wnd)
{
	int i = 0;

	train_wnd = wnd;

	// init used commands
	init_command("help", print_train_commands_func, "Prints all train commands", &train_cmd[i++]);
	init_command("clear", clear_train_func, "Clears the screen", &train_cmd[i++]);
	init_command("cmd", run_command_func, "Runs a train command", &train_cmd[i++]);

	// init unused commands
	while (i < MAX_COMMANDS)
	{
		init_command("----", NULL, "unused train command", &train_cmd[i++]);
	}
	train_cmd[MAX_COMMANDS].name = "NULL";
	train_cmd[MAX_COMMANDS].func = NULL;
	train_cmd[MAX_COMMANDS].description = "NULL";

	train_port = create_process (train_process, 3, 0, "Train process");
}
