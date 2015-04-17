
#include <kernel.h>

struct _Timer;

typedef struct _Timer
{
    int num_of_ticks;
    struct _Timer *next;
} Timer;
Timer tml[MAX_PROCS];

Timer *tml_head;

PORT timer_port;


void sleep(int ticks)
{
	Timer_Message msg;
	msg.num_of_ticks = ticks;
	send(timer_port, (void *)&msg);
}

void timer_notifier(PROCESS self, PARAM param)
{
	while(1)
	{
		wait_for_interrupt(TIMER_IRQ);
		message(timer_port, (void *)NULL);
	}
}

// adds a timer to the timer differential list
// don't worry about interrupts, data-structure not shared
void add_timer_list(int num_of_ticks, PROCESS proc)
{
	Timer *t;
	
	// kprintf("adding %d to timer list with %d ticks", proc-pcb, num_of_ticks);

	// head null, insert as head
	if (tml_head == NULL)
	{
		tml_head = &tml[proc - pcb];
		tml_head->num_of_ticks = num_of_ticks;
		tml_head->next = NULL;
		return;
	}

	// insert at head of list
	if (num_of_ticks < tml_head->num_of_ticks)
	{
		// decrease head by num_of_ticks
		tml_head->num_of_ticks -= num_of_ticks;

		tml[proc - pcb].num_of_ticks = num_of_ticks;

		// insert before head
		tml[proc - pcb].next = tml_head;
		tml_head = &tml[proc - pcb];
		return;
	}

	// insert after head
	t = tml_head;
	while(1)
	{
		if (num_of_ticks >= t->num_of_ticks)
		{
			num_of_ticks -= t->num_of_ticks;
		}
		else
		{
			break;
		}

		if (t->next == NULL) break;

		t = t->next;
	}

	// decrease next by num_of_ticks
	if (t->next != NULL)
	{
		t->next->num_of_ticks -= num_of_ticks;
	}

	tml[proc - pcb].num_of_ticks = num_of_ticks;

	// insert between t and t->next
	tml[proc - pcb].next = t->next;
	t->next = &tml[proc - pcb];
}

// returns a PROCESS ready to wake or NULL
PROCESS pop_timer_list()
{
	PROCESS p;
	if (tml_head != NULL)
	{
		if (tml_head->num_of_ticks <= 0)
		{
			p = pcb + (tml_head - tml);
			tml_head = tml_head->next;
			return p;
		}
	}
	return NULL;
}

void timer_process(PROCESS self, PARAM param)
{
	Timer_Message *msg;
	PROCESS proc;
	create_process(timer_notifier, 7, 0, "Timer notifier");
	while(1)
	{
		msg = (Timer_Message *)receive(&proc);
		if (msg == NULL)
		{
			// message from timer_notifier
			if(tml_head != NULL)
			{
				tml_head->num_of_ticks--;
				while((proc = pop_timer_list()) != NULL)
				{
					reply(proc);
				}
			}
		}
		else
		{
			// message from process wishing to sleep
			add_timer_list(msg->num_of_ticks, proc);
		}
	}
}

void init_timer ()
{
	Timer *t;
	tml_head = tml + MAX_PROCS;
	for(t = tml; t < tml_head; t++)
	{
		t->num_of_ticks = 0;
		t->next = NULL;
	}
	tml_head = NULL;

	timer_port = create_process(timer_process, 6, 0, "Timer process");
}
