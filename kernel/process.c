
#include <kernel.h>


#define STACK_TOP 655360
#define FRAME_SIZE 30720

PCB pcb[MAX_PROCS];


PORT create_process (void (*ptr_to_new_proc) (PROCESS, PARAM),
		     int prio,
		     PARAM param,
		     char *name)
{
	BYTE *esp;
	PROCESS proc;
	PROCESS end = pcb + MAX_PROCS;

	/* Allocate available PCB */
	for (proc = pcb; proc < end; proc++)
	{
		if (proc->used == FALSE)
		{
			break;
		}
	}
	assert(proc != end);

	/* Initialize PCB */
	proc->magic = MAGIC_PCB;
	proc->used = TRUE;
	proc->state = STATE_READY;
	proc->priority = prio;
	proc->first_port = NULL;
	proc->name = name;

	/* Allocate stack frame */
	esp = (BYTE*)(STACK_TOP - (proc - pcb) * FRAME_SIZE);

	/* initialize stack frame */
	esp -= sizeof(PARAM); /* param */
	*((PARAM*)esp) = param;
	esp -= sizeof(PROCESS); /* self */
	*((PROCESS*)esp) = proc;
	esp -= sizeof(void(*)(void)); /* return address, dummy arg */
	*((void(**)(void))esp) = (void(*)(void))NULL;
	esp -= sizeof(void(*)(PROCESS, PARAM)); /* func, EIP */
	*((void(**)(PROCESS, PARAM))esp) = ptr_to_new_proc;
	esp -= 7 * sizeof(LONG); /* registers, EAX, ECX, EDX, EBX, EBP, ESI, EDI */

	/* assign stack pointer */
	proc->esp = (MEM_ADDR)esp;

	/* add process to ready queue */
	add_ready_queue(proc);

	/* return NULL pointer */
	return (PORT) NULL;
}


PROCESS fork()
{
    // Dummy return to make gcc happy
    return (PROCESS) NULL;
}


void print_process(WINDOW* wnd, PROCESS p)
{
	/* relies on order of states given in kernel.h */
	char state[16*6] = "READY          \0SEND_BLOCKED   \0REPLY_BLOCKED  \0RECEIVE_BLOCKED\0MESSAGE_BLOCKED\0INTR_BLOCKED   ";
	if(p->used == TRUE)
		wprintf(wnd, "%s\t%s\t%4d\t%s\n", &state[0] + p->state * 16, (active_proc == p) ? "*     " : "      ", p->priority, p->name);
}

void print_all_processes(WINDOW* wnd)
{
	PROCESS proc, end;

	/* ensure cursor at start of new line */
	if (wnd->cursor_x != 0)
	{
		wprintf(wnd, "\n");
	}

	/* print head of table */
	wprintf(wnd, "State          \tActive\tPrio\tName\n");
	wprintf(wnd, "----------------------------------------------------\n");

	/* print all used processes */
	end = pcb + MAX_PROCS;
	for (proc = pcb; proc < end; proc++)
	{
		if (proc->used == TRUE)
		{
			print_process(wnd, proc);
		}
	}
}


void init_process()
{
	PROCESS proc, end;

	/* initialize all pcbs as used = FALSE */
	end = pcb + MAX_PROCS;
	for (proc = pcb; proc < end; proc++)
	{
		proc->magic = ~MAGIC_PCB;
		proc->used = FALSE;
	}

	/* setup initial/null process */
	pcb->magic = MAGIC_PCB;
	pcb->used = TRUE;
	pcb->state = STATE_READY;
	pcb->priority = 1;
	pcb->first_port = NULL;
	pcb->name = "Boot process";

	/* initialize active process to initial/null process */
	active_proc = pcb;
}
