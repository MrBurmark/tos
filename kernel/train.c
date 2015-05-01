
#include <kernel.h>

static WINDOW train_window_def = {0, 0, 80 - MAZE_WIDTH, 10, 0, 0, '_'};
WINDOW* train_wnd = &train_window_def;


command train_cmd[MAX_COMMANDS + 1];

PORT train_port;

volatile BOOL TOS_train_getting_cargo;

unsigned int last_cmd_sent = 0;

int train_cmd_pause = 15;
int TOS_track_length_time_multiplier = 8000;
int zamboni_default_speed = 5;
int tos_switch_length = 1;
int tos_capture_speed = 4;
int tos_default_speed = 5;

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
#define TOS_NUMBER_TRAINS 3

// track pieces have an id and type
// TRACK_SECTION: uses length, track1, track2
// TRAIN_SWITCH: uses track_out, track_green, track_red
// Track Assumptions:
//  Track switches:
//   track_out is never null 
//   at least one of track_green or track_red is not null
//   eventually surrounded by track sections
//  Track Sections:
//   only one end of a track segment may lead to null
struct _track_piece;
typedef struct _track_piece
{
	unsigned char id;
	unsigned char type;
	unsigned char default_direction;
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
	track_piece *position;
	track_piece *destination;
	track_piece *next;
	track_piece *prev;
} train_train;

typedef struct _track_status
{
	unsigned char s88_clear 	: 1;
	unsigned char configured 	: 1;
	unsigned char setup			: 1;
	unsigned char number_trains;
	train_train trains[TOS_NUMBER_TRAINS];
} track_status;
track_status TOS_track_status;
train_train *red_train = &TOS_track_status.trains[0];
train_train *cargo_car = &TOS_track_status.trains[1];
train_train *black_train = &TOS_track_status.trains[2];

typedef struct _track_path
{
	int length;
	track_piece *path[TOS_TRACK_NUMBER_SECTIONS + TOS_TRACK_NUMBER_SWITCHES];
} track_path;

track_piece *find_next_piece(track_piece *trk1, track_piece *trk2);

void send_train_com_msg(COM_Message *msg)
{
	unsigned int time_since_last_cmd = get_TOS_time() - last_cmd_sent;

	if ( time_since_last_cmd < train_cmd_pause )
		sleep(train_cmd_pause - time_since_last_cmd);

	send(com_port, msg);

	last_cmd_sent = get_TOS_time();
}

// clears s88 memory in train controller, required before checking a segment
void clear_s88(track_status *trk)
{
	COM_Message msg;
	char out_buf[] = "R\r";
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (trk->s88_clear == FALSE)
	{
		send_train_com_msg(&msg);
		trk->s88_clear = TRUE;
	}
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

	send_train_com_msg(&msg);

	trk->s88_clear = FALSE;

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

	send_train_com_msg(&msg);

	trn->speed = (unsigned char)speed;

	return TRUE;
}

// returns true if command could be executed, false otherwise
BOOL change_direction(train_train *trn)
{
	track_piece *tmp;
	COM_Message msg;
	char out_buf[16];
	msg.output_buffer = out_buf;
	msg.input_buffer = NULL;
	msg.len_input_buffer = 0;
	
	if (trn->speed != 0)
		return FALSE;

	k_sprintf(msg.output_buffer, "L%dD\r", trn->id);

	send_train_com_msg(&msg);

	tmp = trn->next;
	trn->next = trn->prev;
	trn->prev = tmp;

	return TRUE;
}

void print_train(train_train *trn)
{
	wprintf(train_wnd, "id %d: spd %d, pos %s%d, dst %s%d, nxt %s%d, prv %s%d\n", 
			trn->id, 
			trn->speed, 
			trn->position ? trn->position->type == TRACK_SWITCH ? "S" : "" : "", 
			trn->position ? trn->position->id : 0, 
			trn->destination ? trn->destination->type == TRACK_SWITCH ? "S" : "" : "", 
			trn->destination ? trn->destination->id : 0, 
			trn->next ? trn->next->type == TRACK_SWITCH ? "S" : "" : "", 
			trn->next ? trn->next->id : 0, 
			trn->prev ? trn->prev->type == TRACK_SWITCH ? "S" : "" : "", 
			trn->prev ? trn->prev->id : 0 );
}

// updates the position and direction of a train that has just followed a path
void update_position(train_train *trn, track_path *path)
{
	if (path->length > 1)
	{
		// moved far away
		trn->next = find_next_piece(path->path[path->length-1], path->path[path->length-2]);
		trn->prev = path->path[path->length-2];
	}
	else if (path->length == 1)
	{
		// moved to adjacent segment
		trn->next = find_next_piece(path->path[path->length-1], trn->position);
		trn->prev = trn->position;
	}
	else
	{
		// didn't move
		return;
	}
	trn->position = path->path[path->length - 1];
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

	if (swt->direction != direction || TOS_track_status.setup != TRUE)
	{
		k_sprintf(msg.output_buffer, "M%d%c\r", swt->id, direction);

		send_train_com_msg(&msg);

		swt->direction = direction;
	}

	return TRUE;
}

// returns a pointer to the next track piece going in line from trk2 -> trk1 -> next
track_piece *find_next_piece(track_piece *trk1, track_piece *trk2)
{
	if (trk1->type == TRACK_SWITCH)
	{
		if (trk2 == trk1->track_out)
		{
			// moving toward Red / Green
			if (trk1->direction == GREEN)
			{
				return trk1->track_green;
			}
			else if (trk1->direction == RED)
			{
				return trk1->track_red;
			}
			else
			{
				// switch not set
				set_switch(trk1, GREEN);
				return trk1->track_green;
			}
		}
		else
		{
			// moving toward track_out
			return trk1->track_out;
		}
	}
	else if (trk1->type == TRACK_SECTION)
	{
		if (trk2 == trk1->track1)
		{
			return trk1->track2;
		}
		else if  (trk2 == trk1->track2)
		{
			return trk1->track1;
		}
		else
		{
			// should never happen
			return NULL;
		}
	}
	else
	{
		// should never happen
		return NULL;
	}
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

void add_track_switch(unsigned char id, unsigned char default_dir, unsigned char type_out, unsigned int id_out, 
	unsigned char type_G, unsigned int id_G, unsigned char type_R, unsigned int id_R)
{
	track_piece *trk = &TOS_track_switches[id];

	trk->id 				= id;
	trk->type 				= TRACK_SWITCH;
	trk->default_direction 	= default_dir;
	trk->direction 			= UNKNOWN;

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
	add_track_section(1, 3, TRACK_SWITCH, 2, TRACK_SECTION, 2);
	add_track_section(2, 3, TRACK_SWITCH, 3, TRACK_SECTION, 1);
	add_track_section(3, 4, TRACK_SWITCH, 1, TRACK_SECTION, 4);
	add_track_section(4, 4, TRACK_SWITCH, 4, TRACK_SECTION, 3);
	add_track_section(5, 3, TRACK_SWITCH, 3, UNKNOWN, 0);
	add_track_section(6, 2, TRACK_SWITCH, 4, TRACK_SECTION, 7);
	add_track_section(7, 4, TRACK_SWITCH, 5, TRACK_SECTION, 6);
	add_track_section(8, 2, TRACK_SWITCH, 6, UNKNOWN, 0);
	add_track_section(9, 2, TRACK_SWITCH, 6, TRACK_SWITCH, 7);
	add_track_section(10, 4, TRACK_SWITCH, 5, TRACK_SWITCH, 8);
	add_track_section(11, 3, TRACK_SWITCH, 8, TRACK_SWITCH, 7);
	add_track_section(12, 3, TRACK_SWITCH, 7, TRACK_SWITCH, 2);
	add_track_section(13, 2, TRACK_SWITCH, 8, TRACK_SECTION, 14);
	add_track_section(14, 2, TRACK_SWITCH, 9, TRACK_SECTION, 13);
	add_track_section(15, 1, TRACK_SWITCH, 9, TRACK_SWITCH, 1);
	add_track_section(16, 6, TRACK_SWITCH, 9, UNKNOWN, 0);
	add_track_switch(1, GREEN, TRACK_SECTION, 15, TRACK_SECTION, 3, TRACK_SWITCH, 2);
	add_track_switch(2, GREEN, TRACK_SWITCH, 1, TRACK_SECTION, 1, TRACK_SECTION, 12);
	add_track_switch(3, GREEN, TRACK_SWITCH, 4, TRACK_SECTION, 2, TRACK_SECTION, 5);
	add_track_switch(4, GREEN, TRACK_SECTION, 6, TRACK_SECTION, 4, TRACK_SWITCH, 3);
	add_track_switch(5, GREEN, TRACK_SECTION, 7, TRACK_SECTION, 10, TRACK_SWITCH, 6);
	add_track_switch(6, GREEN, TRACK_SWITCH, 5, TRACK_SECTION, 9, TRACK_SECTION, 8);
	add_track_switch(7, RED, TRACK_SECTION, 12, TRACK_SECTION, 9, TRACK_SECTION, 11);
	add_track_switch(8, GREEN, TRACK_SECTION, 13, TRACK_SECTION, 10, TRACK_SECTION, 11);
	add_track_switch(9, RED, TRACK_SECTION, 14, TRACK_SECTION, 16, TRACK_SECTION, 15);
}

void reset_TOS_switches()
{
	set_switch(&TOS_track_switches[1], TOS_track_switches[1].default_direction);
	set_switch(&TOS_track_switches[2], TOS_track_switches[2].default_direction);
	set_switch(&TOS_track_switches[3], TOS_track_switches[3].default_direction);
	set_switch(&TOS_track_switches[4], TOS_track_switches[4].default_direction);
	set_switch(&TOS_track_switches[5], TOS_track_switches[5].default_direction);
	set_switch(&TOS_track_switches[6], TOS_track_switches[6].default_direction);
	set_switch(&TOS_track_switches[7], TOS_track_switches[7].default_direction);
	set_switch(&TOS_track_switches[8], TOS_track_switches[8].default_direction);
	set_switch(&TOS_track_switches[9], TOS_track_switches[9].default_direction);
}

void reset_TOS_track_status()
{
	if (TOS_track_status.setup == FALSE)
	{
		TOS_track_status.s88_clear = FALSE;
	}

	TOS_track_status.configured = FALSE;

	reset_TOS_switches();

	TOS_track_status.setup = TRUE;
}

// find and set-up initial locations for red, cargo, and black trains
BOOL find_TOS_configuration()
{
	int i, wait_time;

	reset_TOS_track_status();

	TOS_track_status.number_trains = 0;

	// find engine
	if (check_segment(&TOS_track_status, 8))
	{
		red_train->position = &TOS_track_sections[8];
		red_train->destination = &TOS_track_sections[8];
		red_train->next = &TOS_track_switches[6];
		red_train->prev = NULL;
	}
	else if (check_segment(&TOS_track_status, 5))
	{
		red_train->position = &TOS_track_sections[5];
		red_train->destination = &TOS_track_sections[5];
		red_train->next = &TOS_track_switches[3];
		red_train->prev = NULL;
	}
	else
	{
		return FALSE;
	}
	TOS_track_status.number_trains++;

	// find car
	if (check_segment(&TOS_track_status, 2))
	{
		cargo_car->position = &TOS_track_sections[2];
		cargo_car->destination = &TOS_track_sections[8];
		cargo_car->next = UNKNOWN;
		cargo_car->prev = UNKNOWN;
	}
	else if (check_segment(&TOS_track_status, 11))
	{
		cargo_car->position = &TOS_track_sections[11];
		cargo_car->destination = &TOS_track_sections[5];
		cargo_car->next = UNKNOWN;
		cargo_car->prev = UNKNOWN;
	}
	else if (check_segment(&TOS_track_status, 16))
	{
		cargo_car->position = &TOS_track_sections[16];
		cargo_car->destination = &TOS_track_sections[5];
		cargo_car->next = UNKNOWN;
		cargo_car->prev = UNKNOWN;
	}
	else
	{
		return FALSE;
	}
	TOS_track_status.number_trains++;

	// wait long enough for 3 round trips
	wait_time = TOS_track_length_time_multiplier * 28 * 3 / (zamboni_default_speed * zamboni_default_speed);

	// find Zamboni
	i = get_TOS_time();
	while (get_TOS_time() - i < wait_time)
	{
		if (check_segment(&TOS_track_status, 7) == TRUE)
		{
			// zamboni found
			black_train->position = &TOS_track_sections[7];
			black_train->destination = UNKNOWN;
			black_train->next = UNKNOWN;
			black_train->prev = UNKNOWN;
			TOS_track_status.number_trains++;
			break;
		}
	}
	if (black_train->position != NULL)
	{
		// wait long enough for 3 trips to segment 10
		wait_time = TOS_track_length_time_multiplier * 4 * 3 / (zamboni_default_speed * zamboni_default_speed);
		i = get_TOS_time();
		while (get_TOS_time() - i < wait_time)
		{
			if (check_segment(&TOS_track_status, 10) == TRUE)
			{
				// zamboni going clockwise
				black_train->next = &TOS_track_switches[5];
				black_train->prev = &TOS_track_sections[6];
				break;
			}
		}
		if (black_train->next == NULL)
		{
			// zamboni going anti-clockwise
			black_train->next = &TOS_track_sections[6];
			black_train->prev = &TOS_track_switches[5];
		}
	}

	TOS_track_status.configured = TRUE;

	return TRUE;
}

BOOL configure_TOS_track()
{
	if (TOS_track_status.configured != TRUE)
		find_TOS_configuration();

	return TRUE;
}


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
				node->depth == i - 1) \
			{ \
				trk_pc = tp; \
				continue; \
			} \
		}

// takes track section src and dst and computes a path from src to dst using breadth first search
// returns the length of the path, writes the path to path, src included, dst included if dst != src
// returns -1 if no path from src to destination possible
void BFS_path(const track_piece *src, const track_piece *dst, track_path *path)
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
	{
		path->length = 1;
		path->path[0] = (track_piece *)src;
		return;
	}

	queue[queue_end++] = (track_piece *)src;

	node = GET_NODE_I(queue[queue_start]);
	node->used = TRUE;
	node->depth = depth;

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
			// should never happen
			path->length = -1;
			return;
		}

		queue_start++;
	}

	// no path found
	if (queue_start >= queue_end)
	{
		// should never happen
		path->length = -1;
		return;
	}

	// path found, copy it to path
	queue_end--;
	node = GET_NODE_I(queue[queue_end]);
	depth = node->depth;
	trk_pc = queue[queue_end];

	for (i = depth; i > 0; i--)
	{
		path->path[i] = trk_pc;

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
			// should never happen
			path->length = -1;
			return;
		}
	}

	path->path[0] = (track_piece *)src;

	path->length = depth + 1;
}

void print_path(track_path *path)
{
	int i;
	for(i = 0; i < path->length; i++)
	{
		if (path->path[i]->type == TRACK_SWITCH)
		{
			wprintf(train_wnd, "S%d ", path->path[i]->id);
		}
		else if (path->path[i]->type == TRACK_SECTION)
		{
			wprintf(train_wnd, "%d ", path->path[i]->id);
		}
	}
	if (path->length > 0)
		wprintf(train_wnd, "\n");
}

// returns an estimate of the time required to run this path
int get_path_time(track_path *path, int speed)
{
	int i;
	int path_time = 0;
	if (speed > 5 || speed <= 0)
		return -1;

	if (path->length < 2)
		return 0;

	i = 0;
	path_time += path->path[i]->length / 2;
	i++;

	for (; i < path->length - 1; i++)
	{
		if (path->path[i]->type == TRACK_SWITCH)
		{
			path_time += tos_switch_length;
		}
		else if (path->path[i]->type == TRACK_SECTION)
		{
			path_time += path->path[i]->length;
		}
		else
		{
			// should never run
			return -1;
		}
	}

	path_time += path->path[i]->length / 2;
	i++;

	return path_time * TOS_track_length_time_multiplier / (speed * speed) ;
}

// finds a path up to the first turn around
// alters path to head to first track section after the turn around
void turn_around_path(track_path *path)
{
	int i;

	for (i = 0; i < path->length; i++)
	{
		if (path->path[i]->type == TRACK_SWITCH)
		{
			// path never ends or starts on a switch
			if ((path->path[i+1] == path->path[i]->track_green && path->path[i-1] == path->path[i]->track_red) || 
				(path->path[i+1] == path->path[i]->track_red && path->path[i-1] == path->path[i]->track_green))
			{
				// turn-around
				for (; path->path[i]->type != TRACK_SECTION; i++)
				{
					path->path[i+1] = find_next_piece(path->path[i], path->path[i-1]);
				}
				path->length = i + 1;
			}
		}
	}
}

// extends a path to the next track section
void extend_path_one_section(track_path *path)
{
	int i;

	if (path->length < 2)
		return;

	i = path->length - 1;
	do
	{
		i++;
		path->path[i] = find_next_piece(path->path[i-1], path->path[i-2]);
	}
	while (path->path[i] ? path->path[i]->type != TRACK_SECTION : 0);

	if (path->path[i] != NULL)
	{
		// avoid adding segments that lead to a deadend
		path->length = i + 1;
	}
}

// reduces a path to the last track section before its end
void reduce_path_one_section(track_path *path)
{
	int i;

	if (path->length < 2)
		return;

	i = path->length - 1;
	while (path->path[--i]->type != TRACK_SECTION);

	path->length = i + 1;
}

// sets all the switches on the path to the correct setting
void set_path_switches(track_path *path)
{
	int i;
	for (i = 0; i < path->length; i++)
	{
		if (path->path[i]->type == TRACK_SWITCH)
		{
			// path never ends on a switch
			if (path->path[i+1] == path->path[i]->track_green)
			{
				set_switch(path->path[i], GREEN);
			}
			else if (path->path[i+1] == path->path[i]->track_red)
			{
				set_switch(path->path[i], RED);
			}
		}
	}
}

// waits until a segment is cleared
void wait_till_clear(track_piece* trk)
{
	while (check_segment(&TOS_track_status, trk->id) == TRUE);
}

// waits until a segment is occupied
void wait_till_occupied(track_piece* trk)
{
	while (check_segment(&TOS_track_status, trk->id) == FALSE);
}

// move train to last, poll last until train appears
BOOL move_train_poll(train_train *trn, int speed, track_path *path)
{
	if (path->length < 2)
		return TRUE;

	if (trn->prev == path->path[1])
	{
		change_direction(trn);
	}

	// move train
	if (!set_speed(trn, speed))
		return FALSE;

	// wait until trn gets to track segment
	wait_till_occupied(path->path[path->length - 1]);

	// stop at requested segment
	set_speed(trn, 0);

	update_position(trn, path);

	return TRUE;
}

// move train to last, time train
BOOL move_train_time(train_train *trn, int speed, track_path *path)
{
	if (path->length < 2)
		return TRUE;

	if (trn->prev == path->path[1])
	{
		change_direction(trn);
	}

	// move train
	if (!set_speed(trn, speed))
		return FALSE;

	// wprintf(train_wnd, "estimated time %d\n", get_path_time(path, speed));

	// wait until trn gets to track segment
	sleep(get_path_time(path, speed));

	// stop at requested segment
	set_speed(trn, 0);

	update_position(trn, path);

	return TRUE;
}

// should only be called on RED_TRAIN
// moves a train to its destination
int go_to_destination_time(train_train *trn)
{
	track_path path;

	BFS_path(trn->position, trn->destination, &path);

	turn_around_path(&path);

	set_path_switches(&path);

	// follows this basic algorithm
	// 1. find path
	// 2. set switches
	// 3. go to first turn around
	// 4. stop and turn-around, repeat until path length 1 (src == dst)
	while (path.length > 1)
	{
		move_train_time(trn, 5, &path);

		BFS_path(trn->position, trn->destination, &path);

		turn_around_path(&path);

		set_path_switches(&path);
	}

	return 0;
}

// should only be called on RED_TRAIN
// moves a train to its destination
int go_to_destination(train_train *trn)
{
	track_path path;

	BFS_path(trn->position, trn->destination, &path);

	turn_around_path(&path);

	set_path_switches(&path);

	// follows this basic algorithm
	// 1. find path
	// 2. set switches
	// 3. go to first turn around
	// 4. stop and turn-around, repeat until path length 1 (src == dst)
	while (path.length > 1)
	{
		move_train_poll(trn, 5, &path);

		reset_TOS_switches();

		BFS_path(trn->position, trn->destination, &path);

		turn_around_path(&path);

		set_path_switches(&path);
	}

	return 0;
}

// only call on RED_TRAIN
// moves a train to the next section after its destination if it exists
// else tries  to stop near the middle of the destination section
int go_through_destination(train_train *trn)
{
	track_path path;

	BFS_path(trn->position, trn->destination, &path);
	turn_around_path(&path);
	set_path_switches(&path);

	// follows this basic algorithm
	// 1. find path
	// 2. set switches
	// 3. go to first turn around
	// 4. stop and turn-around, repeat until path length 1
	while (path.length > 1)
	{

		if (path.path[path.length-1] == trn->destination)
		{
			// if path ends at destination
			reduce_path_one_section(&path);

			// move train near destination
			move_train_poll(trn, tos_default_speed, &path);

			reset_TOS_switches();

			// recalculate path
			BFS_path(trn->position, trn->destination, &path);
			extend_path_one_section(&path);
			set_path_switches(&path);

			if (path.path[path.length-1] != trn->destination)
			{
				// extension successful
				move_train_poll(trn, tos_capture_speed, &path);
			}
			else
			{
				// deadend
				move_train_time(trn, tos_capture_speed, &path);
			}

			reset_TOS_switches();

			break;
		}

		move_train_poll(trn, tos_default_speed, &path);

		reset_TOS_switches();

		BFS_path(trn->position, trn->destination, &path);
		turn_around_path(&path);
		set_path_switches(&path);
	}

	wprintf(train_wnd, "Got Cargo\n");

	return 0;
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

	reset_TOS_track_status();

	while(1)
	{
		wprintf(train_wnd, "train> ");

		msg = (Train_Message *)receive(&sender);

		if (TOS_train_getting_cargo)
		{
			wprintf(train_wnd, "Train is getting cargo, ignoring commands\n");
			reply(sender);
			continue;
		}

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

int time_multiplier_func(int argc, char **argv)
{
	if (argc == 1)
	{
		wprintf(train_wnd, "%d\n", TOS_track_length_time_multiplier);
	}
	else if (argc > 1)
	{
		if (is_num(argv[1]))
		{
			TOS_track_length_time_multiplier = atoi(argv[1]);
		}
		else
		{
			return 1;
		}
	}
	return 0;
}

int goto_func(int argc, char **argv)
{
	int dst_id;
	unsigned char use_time = FALSE;

	if (argc < 2)
	{
		wprintf(train_wnd, "Usage: goto destination_id [-t]\n");
		return 1;
	}
	dst_id = atoi(argv[1]);


	if (dst_id < 1 || dst_id > TOS_TRACK_NUMBER_SECTIONS)
	{
		wprintf(train_wnd, "Invalid destination id\n");
		return 2;
	}

	if (configure_TOS_track() == FALSE)
	{
		wprintf(train_wnd, "Couldn't set up TOS track\n");
		return 3;
	}

	if (argc > 2)
	{
		if (k_strcmp("-t", argv[2]) == 0)
			use_time = TRUE;
	}

	red_train->destination = &TOS_track_sections[dst_id];

	if (use_time)
		go_to_destination_time(red_train);
	else
		go_to_destination(red_train);

	return 0;
}

void get_cargo_process(PROCESS self, PARAM param)
{
	int tmp = TOS_track_length_time_multiplier;

	TOS_train_getting_cargo = TRUE;

	// forces full setup procedure
	TOS_track_status.setup = FALSE;

	if (param) TOS_track_length_time_multiplier = 0;

	if (find_TOS_configuration() == FALSE)
	{
		if (param) TOS_track_length_time_multiplier = tmp;

		wprintf(train_wnd, "Couldn't set up TOS track\n");
		exit();
	}

	if (param) TOS_track_length_time_multiplier = tmp;


	red_train->destination = cargo_car->position;

	go_through_destination(red_train);

	red_train->destination = cargo_car->destination;

	go_to_destination(red_train);

	wprintf(train_wnd, "Complete\n");

	TOS_train_getting_cargo = FALSE;

	exit();
}

int get_cargo_func(int argc, char **argv)
{
	int param = FALSE;
	if (argc > 1)
	{
		if (k_strcmp("-iz", argv[1]) == 0)
		{
			param = TRUE;
		}
	}

	create_process (get_cargo_process, 4, param, "Get Cargo process");

	resign();

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
	init_command("t_mult", time_multiplier_func, "Prints the current time multiplier or sets it when an argument is given", &train_cmd[i++]);
	init_command("goto", goto_func, "send the red train to the destination", &train_cmd[i++]);
	init_command("gc", get_cargo_func, "red train links with the cargo car and returns to its starting location", &train_cmd[i++]);

	// init unused commands
	while (i < MAX_COMMANDS)
	{
		init_command("----", NULL, "unused train command", &train_cmd[i++]);
	}
	train_cmd[MAX_COMMANDS].name = "NULL";
	train_cmd[MAX_COMMANDS].func = NULL;
	train_cmd[MAX_COMMANDS].description = "NULL";

	TOS_track_status.setup = FALSE;
	TOS_train_getting_cargo = FALSE;
	red_train->id = RED_TRAIN;
	red_train->speed = 0;
	red_train->position = NULL;
	red_train->destination = NULL;
	red_train->next = NULL;
	red_train->prev = NULL;
	cargo_car->id = CARGO_CAR;
	cargo_car->speed = 0;
	cargo_car->position = NULL;
	cargo_car->destination = NULL;
	cargo_car->next = NULL;
	cargo_car->prev = NULL;
	black_train->id = BLACK_TRAIN;
	black_train->speed = zamboni_default_speed;
	black_train->position = NULL;
	black_train->destination = NULL;
	black_train->next = NULL;
	black_train->prev = NULL;

	train_port = create_process (train_process, 3, 0, "Train process");

	resign();
}
