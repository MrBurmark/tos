
#include <kernel.h>

PORT_DEF port[MAX_PORTS];



PORT create_port()
{
	return create_new_port(active_proc);
}


PORT create_new_port (PROCESS owner)
{
	PORT p, end;

	volatile int saved_if;
	DISABLE_INTR(saved_if);

	assert(owner->magic == MAGIC_PCB);

	/* allocate unused port */
	end = port + MAX_PORTS;
	for (p = port; p < end; p++)
	{
		if (p->used == FALSE)
		{
			break;
		}
	}
	assert(p != end);

	/* Initialize PORT */
	p->magic 				= MAGIC_PORT;
	p->used 				= TRUE;
	p->open 				= TRUE;
	p->owner 				= owner;
	p->blocked_list_head 	= NULL;
    p->blocked_list_tail 	= NULL;
    // p->next 				= NULL;

    /* Add new port to head of owner's port list */
    p->next 			= owner->first_port;
    owner->first_port 	= p;

	ENABLE_INTR(saved_if); 

	return p;
}

// deallocates all ports associated with a process
void remove_ports (PROCESS owner)
{
	PORT p, p_tmp;

	volatile int saved_if;
	DISABLE_INTR(saved_if);

	assert(owner->magic == MAGIC_PCB);

	p = owner->first_port;
	while (p != NULL)
	{
		/* Initialize PORT */
		p->magic 				= ~MAGIC_PORT;
		p->used 				= FALSE;
		p->open 				= TRUE;
		p->owner 				= NULL;
		p->blocked_list_head 	= NULL;
	    p->blocked_list_tail 	= NULL;
	    p_tmp = p->next;
	    p->next 				= NULL;
	    p = p_tmp;
	}

	ENABLE_INTR(saved_if);
}

// returns true if a message is waiting
// false otherwise, checks closed ports
BOOL check_messages (PROCESS proc)
{
	BOOL found = FALSE;

	volatile int saved_if;
	DISABLE_INTR(saved_if);

	/* Find first message waiting */
	PORT p = proc->first_port;
	while(p != NULL)
	{
		if (p->blocked_list_head != NULL)
		{
			/* Found message */
			/* remove that process from blocked list */
			found = TRUE;
			break;
		}
		p = p->next;
	}

	ENABLE_INTR(saved_if);

	return found;
}


void open_port (PORT port)
{
	// volatile int saved_if;
	// DISABLE_INTR(saved_if);

	assert(port->magic == MAGIC_PORT);
	port->open = TRUE;

	// ENABLE_INTR(saved_if);
}


void close_port (PORT port)
{
	// volatile int saved_if;
	// DISABLE_INTR(saved_if);

	assert(port->magic == MAGIC_PORT);
	port->open = FALSE;

	// ENABLE_INTR(saved_if);
}


void add_to_blocked_list(PORT dest_port)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	if(dest_port->blocked_list_head == NULL)
	{
		/* Empty list */
		dest_port->blocked_list_head = active_proc;
	}
	else
	{
		/* Add to tail of existing list */
		dest_port->blocked_list_tail->next_blocked = active_proc;
	}
	dest_port->blocked_list_tail = active_proc;
	active_proc->next_blocked = NULL;

	ENABLE_INTR(saved_if);
}


void send (PORT dest_port, void* data)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	assert(dest_port->magic == MAGIC_PORT);

	if(dest_port->open && dest_port->owner->state == STATE_RECEIVE_BLOCKED)
	{
		/* Destination can recieve immediately */
		/* store data in dest PCB */
		dest_port->owner->param_proc = active_proc;
		dest_port->owner->param_data = data;
		active_proc->state = STATE_REPLY_BLOCKED;
		add_ready_queue(dest_port->owner);
	}
	else
	{
		/* Must wait for receive */
		/* Store data to own PCB */
		active_proc->param_proc = active_proc;
		active_proc->param_data = data;

		add_to_blocked_list(dest_port);
		
		active_proc->state = STATE_SEND_BLOCKED;
	}
	remove_ready_queue(active_proc);

	ENABLE_INTR(saved_if);

	resign();
}


void message (PORT dest_port, void* data)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);

	assert(dest_port->magic == MAGIC_PORT);

	if(dest_port->open && dest_port->owner->state == STATE_RECEIVE_BLOCKED)
	{
		/* Destination can recieve immediately */
		/* store data in dest PCB */
		dest_port->owner->param_proc = active_proc;
		dest_port->owner->param_data = data;
		add_ready_queue(dest_port->owner);
	}
	else
	{
		/* Must wait for receive */
		/* Store data to own PCB */
		active_proc->param_proc = active_proc;
		active_proc->param_data = data;

		add_to_blocked_list(dest_port);

		remove_ready_queue(active_proc);
		active_proc->state = STATE_MESSAGE_BLOCKED;
	}

	ENABLE_INTR(saved_if);

	resign();
}


PROCESS pop_message( void )
{
	PROCESS sender = NULL;

	volatile int saved_if;
	DISABLE_INTR(saved_if);

	/* Find first message waiting */
	PORT p = active_proc->first_port;
	while(p != NULL)
	{
		if (p->open && p->blocked_list_head != NULL)
		{
			/* Found first message */
			/* remove that process from blocked list */
			sender = p->blocked_list_head;
			p->blocked_list_head = sender->next_blocked;
			sender->next_blocked = NULL;
			if(p->blocked_list_head == NULL)
			{
				p->blocked_list_tail = NULL;
			}
			break;
		}
		p = p->next;
	}

	ENABLE_INTR(saved_if);

	return sender;
}


void* receive (PROCESS* sender)
{
	void* mess = NULL;

	volatile int saved_if;
	DISABLE_INTR(saved_if);

	/* take a message if one is waiting */
	*sender = pop_message();

	if(*sender != NULL)
	{
		/* Message found */
		/* data in sender PCB */
		mess = (*sender)->param_data;

		if((*sender)->state == STATE_MESSAGE_BLOCKED)
		{
			add_ready_queue(*sender);
		}
		else if((*sender)->state == STATE_SEND_BLOCKED)
		{
			(*sender)->state = STATE_REPLY_BLOCKED;
		}
	}
	else
	{
		/* Wait for a message */
		remove_ready_queue(active_proc);
		active_proc->state = STATE_RECEIVE_BLOCKED;

		ENABLE_INTR(saved_if);
		resign();
		DISABLE_INTR(saved_if);

		/* Message received */
		/* data in own PCB */
		mess = active_proc->param_data;
		*sender = active_proc->param_proc;
	}

	ENABLE_INTR(saved_if);

	return mess;
}


void reply (PROCESS sender)
{
	volatile int saved_if;
	DISABLE_INTR(saved_if);
	
	if (sender->state == STATE_REPLY_BLOCKED)
	{
		/* put sender back on ready queue */
		add_ready_queue(sender);
	}

	ENABLE_INTR(saved_if);

	resign();
}


void init_ipc()
{
	PORT p, end;

	/* initialize all ports as used = FALSE */
	end = port + MAX_PORTS;
	for (p = port; p < end; p++)
	{
		p->magic = MAGIC_PORT;
		p->used = FALSE;
	}

	/* Create port for boot process */
	// create_port();
}
