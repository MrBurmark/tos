
#include <kernel.h>

#include "disptable.c"


PROCESS active_proc;


/*
 * Ready queues for all eight priorities.
 */
PCB *ready_queue [MAX_READY_QUEUES];



/*
 * add_ready_queue
 *----------------------------------------------------------------------------
 * The process pointed to by p is put the ready queue.
 * The appropiate ready queue is determined by p->priority.
 */

void add_ready_queue (PROCESS proc)
{
	PROCESS head;
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	// kprintf("add proc number %d to rq\n", proc - pcb);
	// print_pcb(kernel_window, proc);
	// print_all_processes(kernel_window);

	assert(proc->used == TRUE);
	assert(proc->magic == MAGIC_PCB);

	// clear_window(kernel_window);

	head = ready_queue[proc->priority];
	if(head != NULL)
	{
		proc->next = head;
		proc->prev = head->prev;
		head->prev->next = proc;
		head->prev = proc;
	}
	else
	{
		ready_queue[proc->priority] = proc;
		proc->next = proc;
		proc->prev = proc;
	}
	proc->state = STATE_READY;

	ENABLE_INTR(saved_if); 
}


BOOL in_ready_queue(PROCESS proc)
{
	PROCESS rq_proc = ready_queue[proc->priority];
	if (rq_proc != NULL)
	{
		// priority level has items
		do
		{
		 	if (rq_proc == proc)
		 	{
		 		return TRUE;
		 	}
		 	rq_proc = rq_proc->next;
		} 
		while (rq_proc != ready_queue[proc->priority]);
	}
	return FALSE;
}

/*
 * remove_ready_queue
 *----------------------------------------------------------------------------
 * The process pointed to by p is dequeued from the ready
 * queue.
 */

void remove_ready_queue (PROCESS proc)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	assert(proc->priority < 8 && proc->magic == MAGIC_PCB);

	if(!in_ready_queue(proc))
	{
		// not on ready queue
		// do nothing
	}
	else if(proc->next == proc) /* proc only process in queue */
	{
		ready_queue[proc->priority] = NULL;
	}
	else /* multiple processes in queue */
	{
		ready_queue[proc->priority] = proc->next;
		proc->next->prev = proc->prev;
		proc->prev->next = proc->next;
	}
	// proc->next = NULL;
	// proc->prev = NULL;

	// kprintf("remove proc num %d from rq\n", proc - pcb);
	// print_pcb(kernel_window, proc);

	ENABLE_INTR(saved_if);
}



/*
 * dispatcher
 *----------------------------------------------------------------------------
 * Determines a new process to be dispatched. The process
 * with the highest priority is taken. Within one priority
 * level round robin is used.
 */

PROCESS dispatcher()
{
	int prio;
	PROCESS proc = NULL;
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	for(prio = MAX_READY_QUEUES - 1; prio >= 0; prio--)
	{
		if(ready_queue[prio] == NULL)
		{
			continue;
		}
		else if(prio == active_proc->priority)
		{
			proc = active_proc->next;
			break;
		} 
		else
		{
			proc = ready_queue[prio];
			break;
		}
	}
	ENABLE_INTR(saved_if); 
	return proc;
}


/*
 * resign
 *----------------------------------------------------------------------------
 * The current process gives up the CPU voluntarily. The
 * next running process is determined via dispatcher().
 * The stack of the calling process is setup such that it
 * looks like an interrupt.
 */
void resign()
{
	asm("pushfl;"
		"cli;"
		"popl %%eax;"
		"xchg (%%esp), %%eax;"
		"push %%cs;"
		"pushl %%eax;"
		"pushl %%eax;"
		"pushl %%ecx;"
		"pushl %%edx;"
		"pushl %%ebx;"
		"pushl %%ebp;"
		"pushl %%esi;"
		"pushl %%edi;"
		"movl %%esp, %0"
		: "=r" (active_proc->esp)
		:
		);
	active_proc = dispatcher();
	asm("movl %0, %%esp;"
		"popl %%edi;"
		"popl %%esi;"
		"popl %%ebp;"
		"popl %%ebx;"
		"popl %%edx;"
		"popl %%ecx;"
		"popl %%eax;"
		"iret"
		: 
		: "r" (active_proc->esp)
		);
	/* implicit ret */
}



/*
 * init_dispatcher
 *----------------------------------------------------------------------------
 * Initializes the necessary data structures.
 */

void init_dispatcher()
{
	PROCESS *rq, *end;

	/* initialize all slots in ready queue to NULL */
	end = ready_queue + MAX_READY_QUEUES;
	
	for(rq = ready_queue; rq < end; rq++)
	{
		*rq = (PROCESS)NULL;
	}

	/* add initial/null process to ready queue */

	assert(active_proc->magic == MAGIC_PCB);

	add_ready_queue(active_proc);
}
