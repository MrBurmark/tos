
#include <kernel.h>




void init_null_process()
{

	/* setup PCB */
	active_proc->magic 		= MAGIC_PCB;
	active_proc->used 		= TRUE;
	active_proc->state 		= STATE_READY;
	active_proc->priority 	= 1;
	active_proc->first_port = create_port();
	active_proc->name 		= boot_name;

	/* add to ready queue */
	add_ready_queue(active_proc);
}
