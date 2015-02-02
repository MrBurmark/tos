
#include <kernel.h>





PORT create_port()
{
}


PORT create_new_port (PROCESS owner)
{
}




void open_port (PORT port)
{
}



void close_port (PORT port)
{
}


void send (PORT dest_port, void* data)
{
}


void message (PORT dest_port, void* data)
{
}



void* receive (PROCESS* sender)
{
}


void reply (PROCESS sender)
{
}


void init_ipc()
{
}
