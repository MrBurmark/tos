
#include <kernel.h>


PCB pcb[MAX_PROCS];


PORT create_process (void (*ptr_to_new_proc) (PROCESS, PARAM),
		     int prio,
		     PARAM param,
		     char *name)
{
}


PROCESS fork()
{
    // Dummy return to make gcc happy
    return (PROCESS) NULL;
}




void print_process(WINDOW* wnd, PROCESS p)
{
}

void print_all_processes(WINDOW* wnd)
{
}



void init_process()
{
}
