
#include <kernel.h>

static WINDOW train_window_def = {0, 0, 80 - MAZE_WIDTH, 10, 0, 0, '_'};
WINDOW* train_wnd = &train_window_def;


command train_cmd[MAX_COMMANDS + 1];

PORT train_port;

int train_cmd_pause = 15;

#define UNKNOWN 0
#define TRACK_SECTION 1
#define TRACK_SWITCH 2
#define GREEN 'G'
#define RED 'R'
#define RED_TRAIN 20
#define BLACK_TRAIN 10
#define CARGO_CAR 5
#define TOS_TRACK_NUMBER_SECTIONS 16
#define TOS_TRACK_NUMBER_SWITCHES 9

typedef struct _track_status
{
	unsigned char s88_status;

} track_status;
track_status main_track_status;

// track pieces have an id and type
// TRACK_SECTION: data is the length of this track section
// TRAIN_SWITCH: data is the state of the switch GREEN or RED
// TRAIN_SWITCH_END: data is which end this is, GREEN or RED
struct _track_piece;
typedef struct _track_piece
{
	unsigned short id;
	unsigned char type;
	unsigned char direction;
	union
	{
		struct _track_piece *track_out;
		unsigned int length;
	};
	union
	{
		struct _track_piece *track_green;
		struct _track_piece *track1;
	};
	union
	{
		struct _track_piece *track_red;
		struct _track_piece *track2;
	};
} track_piece;
track_piece _TOS_track[TOS_TRACK_NUMBER_SECTIONS + TOS_TRACK_NUMBER_SWITCHES];
track_piece *TOS_track_sections = _TOS_track - 1; // always reference by id
track_piece *TOS_track_switches = _TOS_track + TOS_TRACK_NUMBER_SECTIONS - 1; // always reference by id
track_piece *TOS_track = _TOS_track;

typedef struct _train_train
{
	unsigned char id;
	unsigned char speed;
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

	// trn->direction = -trn->direction;

	sleep(train_cmd_pause);

	return TRUE;
}

// returns true if command could be executed, false otherwise
// only accepts direction 'G' or 'R'
// always tries to set direction even if switch should already be set to direction
BOOL set_switch(track_piece *swt, unsigned char direction)
{
	COM_Message msg;
	char out_buf[16];
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if ((direction != GREEN && direction != RED) || swt->type != TRACK_SWITCH)
		return FALSE;

	k_sprintf(msg.output_buffer, "M%d%c\r", swt->id, direction);

	send(com_port, &msg);

	swt->direction = direction;

	sleep(train_cmd_pause);

	return TRUE;
}

void add_track_section(unsigned short id, unsigned int length, unsigned char type1, 
	unsigned int id1, unsigned char type2, unsigned int id2)
{
	track_piece *trk = &TOS_track_sections[id];

	trk->id 		= id;
	trk->type 		= TRACK_SECTION;
	trk->length 	= length;

	if (type1 == TRACK_SECTION)
		trk->track1 = &TOS_track_sections[id1];
	else if (type1 == TRACK_SWITCH)
		trk->track1 = &TOS_track_switches[id1];
	else
		trk->track1 = NULL;

	if (type2 == TRACK_SECTION)
		trk->track2 = &TOS_track_sections[id2];
	else if (type2 == TRACK_SWITCH)
		trk->track2 = &TOS_track_switches[id2];
	else
		trk->track2 = NULL;
}

void add_track_switch(unsigned short id, unsigned char type_out, unsigned int id_out, 
	unsigned char type_G, unsigned int id_G, unsigned char type_R, unsigned int id_R)
{
	track_piece *trk = &TOS_track_switches[id];

	trk->id 		= id;
	trk->type 		= TRACK_SWITCH;
	trk->direction 	= UNKNOWN;

	if (type_out == TRACK_SECTION)
		trk->track_out = &TOS_track_sections[id_out];
	else if (type_out == TRACK_SWITCH)
		trk->track_out = &TOS_track_switches[id_out];
	else
		trk->track_out = NULL;

	if (type_G == TRACK_SECTION)
		trk->track_green = &TOS_track_sections[id_G];
	else if (type_G == TRACK_SWITCH)
		trk->track_green = &TOS_track_switches[id_G];
	else
		trk->track_green = NULL;

	if (type_R == TRACK_SECTION)
		trk->track_red = &TOS_track_sections[id_R];
	else if (type_R == TRACK_SWITCH)
		trk->track_red = &TOS_track_switches[id_R];
	else
		trk->track_red = NULL;
}

// initialises the track graph with the TOS track
void init_TOS_track_graph()
{
	// make graph of track sections and switches
	add_track_section(1, 1, TRACK_SWITCH, 2, TRACK_SECTION, 2);
	add_track_section(2, 1, TRACK_SWITCH, 3, TRACK_SECTION, 1);
	add_track_section(3, 1, TRACK_SWITCH, 1, TRACK_SECTION, 4);
	add_track_section(4, 1, TRACK_SWITCH, 4, TRACK_SECTION, 3);
	add_track_section(5, 1, TRACK_SWITCH, 3, TRACK_SECTION, 4);
	add_track_section(6, 1, TRACK_SWITCH, 4, TRACK_SECTION, 7);
	add_track_section(7, 1, TRACK_SWITCH, 5, TRACK_SECTION, 6);
	add_track_section(8, 1, TRACK_SWITCH, 6, UNKNOWN, 0);
	add_track_section(9, 1, TRACK_SWITCH, 6, TRACK_SWITCH, 7);
	add_track_section(10, 1, TRACK_SWITCH, 5, TRACK_SWITCH, 8);
	add_track_section(11, 1, TRACK_SWITCH, 8, TRACK_SWITCH, 7);
	add_track_section(12, 1, TRACK_SWITCH, 7, TRACK_SWITCH, 2);
	add_track_section(13, 1, TRACK_SWITCH, 8, TRACK_SECTION, 14);
	add_track_section(14, 1, TRACK_SWITCH, 9, TRACK_SECTION, 13);
	add_track_section(15, 1, TRACK_SWITCH, 9, TRACK_SWITCH, 1);
	add_track_section(16, 1, TRACK_SWITCH, 9, UNKNOWN, 0);
	add_track_switch(1, TRACK_SECTION, 15, TRACK_SECTION, 3, TRACK_SWITCH, 2);
	add_track_switch(2, TRACK_SWITCH, 1, TRACK_SECTION, 1, TRACK_SECTION, 12);
	add_track_switch(3, TRACK_SWITCH, 4, TRACK_SECTION, 2, TRACK_SECTION, 5);
	add_track_switch(4, TRACK_SECTION, 6, TRACK_SECTION, 4, TRACK_SWITCH, 3);
	add_track_switch(5, TRACK_SECTION, 7, TRACK_SECTION, 10, TRACK_SWITCH, 6);
	add_track_switch(6, TRACK_SWITCH, 5, TRACK_SECTION, 9, TRACK_SECTION, 8);
	add_track_switch(7, TRACK_SECTION, 12, TRACK_SECTION, 9, TRACK_SECTION, 11);
	add_track_switch(8, TRACK_SECTION, 13, TRACK_SECTION, 10, TRACK_SECTION, 11);
	add_track_switch(9, TRACK_SECTION, 14, TRACK_SECTION, 16, TRACK_SECTION, 15);
}

// struct _track_piece;
// typedef struct _track_piece
// {
// 	unsigned short id;
// 	unsigned char type;
// 	unsigned char used 		: 1;
// 	unsigned char direction : 7;
// 	union
// 	{
// 		struct _track_piece *track_out;
// 		unsigned int length;
// 	};
// 	union
// 	{
// 		struct _track_piece *track_green;
// 		struct _track_piece *track1;
// 	};
// 	union
// 	{
// 		struct _track_piece *track_red;
// 		struct _track_piece *track2;
// 	};
// } track_piece;

typedef struct _BFS_node
{
	unsigned char used 	: 1;
	unsigned char depth : 7;
} BFS_node;

#define GET_NODE_I(tp) ((tp->type == TRACK_SECTION) ? &nodes[tp->id - 1] : &nodes[tp->id - 1 + TOS_TRACK_NUMBER_SECTIONS])
#define BFS_NODE_ENQUEUE(tp) \
		if(tp != NULL) \
		{ \
			node = GET_NODE_I(tp); \
			if (node->used != TRUE) \
			{ \
				queue[queue_end++] = tp; \
				node->used = TRUE; \
				node->depth = depth; \
				if(tp == dst) \
					break; \
			} \
		}
#define BFS_NODE_OUTPUT(tp) \
		if(tp != NULL) \
		{ \
			node = GET_NODE_I(tp); \
			if (node->used == TRUE && \
				node->depth == i) \
			{ \
				trk_pc = tp; \
				continue; \
			} \
		}

// takes track section ids src and dst and computes a path from src to dst using breadth first search
// returns the length of the path, writes the path to path, src not included, dst included if dst != src
// returns -1 if no path from src to destination possible
int BFS_path(const track_piece *src, const track_piece *dst, track_piece **path)
{
	int queue_start = 0;
	int queue_end = 0;
	BFS_node *node;
	int depth = 0;
	int i;
	track_piece *trk_pc;
	track_piece *queue[TOS_TRACK_NUMBER_SWITCHES + TOS_TRACK_NUMBER_SECTIONS];
	BFS_node nodes[TOS_TRACK_NUMBER_SWITCHES + TOS_TRACK_NUMBER_SECTIONS];
	k_memset(nodes, 0, (TOS_TRACK_NUMBER_SWITCHES + TOS_TRACK_NUMBER_SECTIONS) * sizeof(BFS_node));

	if (src == dst)
		return 0;

	node = GET_NODE_I(queue[queue_start]);

	queue[queue_end++] = (track_piece *)src;
	node->used = TRUE;
	node->depth = 0;

	while(queue_start < queue_end)
	{
		trk_pc = queue[queue_start];
		node = GET_NODE_I(trk_pc);

		depth = node->depth + 1;

		if (trk_pc->type == TRACK_SECTION)
		{
			BFS_NODE_ENQUEUE(trk_pc->track1);
			BFS_NODE_ENQUEUE(trk_pc->track2);
		}
		else if (trk_pc->type == TRACK_SWITCH)
		{
			BFS_NODE_ENQUEUE(trk_pc->track_out);
			BFS_NODE_ENQUEUE(trk_pc->track_green);
			BFS_NODE_ENQUEUE(trk_pc->track_red);
		}
		else
		{
			return -1;
		}

		queue_start++;
	}

	// no path found
	if (queue_start >= queue_end)
		return -1;

	// path found, copy it to path
	queue_end--;
	node = GET_NODE_I(queue[queue_end]);
	depth = node->depth;
	trk_pc = queue[queue_end];

	for (i = depth - 1; i > 0; i--)
	{
		path[i] = trk_pc;

		if (trk_pc->type == TRACK_SECTION)
		{
			BFS_NODE_OUTPUT(trk_pc->track1);
			BFS_NODE_OUTPUT(trk_pc->track2);
		}
		else if (trk_pc->type == TRACK_SWITCH)
		{
			BFS_NODE_OUTPUT(trk_pc->track_out);
			BFS_NODE_OUTPUT(trk_pc->track_green);
			BFS_NODE_OUTPUT(trk_pc->track_red);
		}
		else
		{
			return -1;
		}
	}

	path[0] = trk_pc;

	return depth;
}

void reset_TOS_track()
{
	set_switch(&TOS_track_switches[1], GREEN);
	set_switch(&TOS_track_switches[2], RED);
	set_switch(&TOS_track_switches[3], GREEN);
	set_switch(&TOS_track_switches[4], GREEN);
	set_switch(&TOS_track_switches[5], GREEN);
	set_switch(&TOS_track_switches[6], GREEN);
	set_switch(&TOS_track_switches[7], RED);
	set_switch(&TOS_track_switches[8], GREEN);
	set_switch(&TOS_track_switches[9], RED);
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

	init_TOS_track_graph();

	while(1)
	{
		wprintf(train_wnd, "train> ");

		msg = (Train_Message *)receive(&sender);

		// input was cleaned in shell
		for (i = 0; i < msg->argc; i++)
		{
			wprintf(train_wnd, "%s ", msg->argv[i]);
		}
		wprintf(train_wnd, "\n");

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
			wprintf(train_wnd, "%s\n", msg.input_buffer[1] == '1' ? "True" : "False");
		}

		return 0;
	}
	else
	{
		return 1;
	}
}

int pause_func(int argc, char **argv)
{
	if (argc == 1)
	{
		wprintf(train_wnd, "%d\n", train_cmd_pause);
	}
	else if (argc > 1)
	{
		if (is_num(argv[1]))
		{
			train_cmd_pause = atoi(argv[1]);
		}
		else
		{
			return 1;
		}
	}
	return 0;
}

int path_func(int argc, char **argv)
{
	int src_id;
	int dst_id;
	int length;
	int i;
	track_piece *path[TOS_TRACK_NUMBER_SECTIONS + TOS_TRACK_NUMBER_SWITCHES];

	if (argc < 2)
	{
		wprintf(train_wnd, "Usage: path src_id dst_id\n");
		return 1;
	}
	src_id = atoi(argv[1]);
	dst_id = atoi(argv[2]);

	if (src_id < 1 || src_id > TOS_TRACK_NUMBER_SECTIONS || dst_id < 1 || dst_id > TOS_TRACK_NUMBER_SECTIONS)
	{
		wprintf(train_wnd, "Invalid section id\n");
		return 2;
	}

	length = BFS_path(&TOS_track_sections[src_id], &TOS_track_sections[dst_id], path);

	if (length == 0)
	{
		wprintf(train_wnd, "%d\n", TOS_track_sections[src_id].id);
	}
	else
	{
		wprintf(train_wnd, "%d - ", TOS_track_sections[src_id].id);
		for (i = 0; i < length - 1; i++)
			wprintf(train_wnd, "%s%d - ", (path[i]->type == TRACK_SWITCH) ? "S": "", path[i]->id);
		wprintf(train_wnd, "%s%d\n", (path[i]->type == TRACK_SWITCH) ? "S": "", path[i]->id);
	}
	return 0;
}

// initializes train related data structures, and train shell commands
// creates the train process
void init_train(WINDOW* wnd)
{
	int i = 0;

	train_wnd = wnd;

	// init used commands
	init_command("help", print_train_commands_func, "Prints all train commands", &train_cmd[i++]);
	init_command("clear", clear_train_func, "Clears the train shell", &train_cmd[i++]);
	init_command("cmd", run_command_func, "Runs a train command", &train_cmd[i++]);
	init_command("pause", pause_func, "Prints the current pause or sets it when an argument is given", &train_cmd[i++]);
	init_command("path", path_func, "Prints a shortest path from one track section to another", &train_cmd[i++]);

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
