
#include <kernel.h>

BOOL interrupts_initialized = FALSE;

IDT idt [MAX_INTERRUPTS];
PROCESS interrupt_table [MAX_INTERRUPTS];
PROCESS p;
// shouldn't overflow for about a month if 1000 timer interupts per second
unsigned int _TOS_time = 0;


void load_idt (IDT* base)
{
    unsigned short           limit;
    volatile unsigned char   mem48 [6];
    volatile unsigned       *base_ptr;
    volatile short unsigned *limit_ptr;

    limit      = MAX_INTERRUPTS * IDT_ENTRY_SIZE - 1;
    base_ptr   = (unsigned *) &mem48[2];
    limit_ptr  = (short unsigned *) &mem48[0];
    *base_ptr  = (unsigned) base;
    *limit_ptr = limit;
    asm ("lidt %0" : "=m" (mem48));
}


void init_idt_entry (int intr_no, void (*isr) (void))
{
    IDT *i = idt + intr_no;

    i->offset_0_15  = (unsigned int)isr & 0xffff;
    i->selector     = CODE_SELECTOR;
    i->dword_count  = 0;
    i->unused       = 0;
    i->type         = 0xe;
    i->dt           = 0;
    i->dpl          = 0;
    i->p            = 1;
    i->offset_16_31 = ((unsigned int)isr >> 16) & 0xffff;
}

void add_ready_queue_p()
{
    add_ready_queue(p);
}

/*
 * Timer ISR
 */
void isr_timer ();
void dummy_isr_timer ()
{
    /*
     *  PUSHL   %EAX        ; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     * Save the context pointer ESP to the PCB
     */
     asm volatile (
        "isr_timer:;"
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

    p = interrupt_table[TIMER_IRQ];

    if (p != NULL && p->state == STATE_INTR_BLOCKED)
    {        
        /* Add event handler to ready queue */
        add_ready_queue_p();
    }

    _TOS_time++;

    active_proc = dispatcher();

    /*
     *  Restore context pointer ESP
     *  MOVB  $0x20,%AL ; Reset interrupt controller
     *  OUTB  %AL,$0x20
     *  POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *  IRET        ; Return to new process
     */
    asm volatile (
        "movl %0, %%esp;"
        "movb $0x20,%%al;"
        "outb %%al,$0x20;"
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
}



/*
 * COM1 ISR
 */
void isr_com1 ();
void dummy_isr_com1 ()
{
    /*
     *  PUSHL   %EAX        ; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     * Save the context pointer ESP to the PCB
     */
     asm volatile (
        "isr_com1:;"
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

    p = interrupt_table[COM1_IRQ];

    if (p == NULL) {
        panic ("service_intr_0x64: Spurious interrupt");
    }

    if (p->state != STATE_INTR_BLOCKED) {
        panic ("service_intr_0x64: No process waiting");
    }

    /* Add event handler to ready queue */
    add_ready_queue_p();

    active_proc = dispatcher();

    /*
     *  Restore context pointer ESP
     *  MOVB  $0x20,%AL ; Reset interrupt controller
     *  OUTB  %AL,$0x20
     *  POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *  IRET        ; Return to new process
     */
    asm volatile (
        "movl %0, %%esp;"
        "movb $0x20,%%al;"
        "outb %%al,$0x20;"
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
}


/*
 * Keyboard ISR
 */
void isr_keyb();
void dummy_isr_keyb()
{
    /*
     *  PUSHL   %EAX        ; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     * Save the context pointer ESP to the PCB
     */
     asm volatile (
        "isr_keyb:;"
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

    p = interrupt_table[KEYB_IRQ];

    if (p == NULL) {
	panic ("service_intr_0x61: Spurious interrupt");
    }

    if (p->state != STATE_INTR_BLOCKED) {
	panic ("service_intr_0x61: No process waiting");
    }

    /* Add event handler to ready queue */
    add_ready_queue_p();

    active_proc = dispatcher();

    /*
     *  Restore context pointer ESP
     *  MOVB  $0x20,%AL ; Reset interrupt controller
     *  OUTB  %AL,$0x20
     *  POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *  IRET        ; Return to new process
     */
    asm volatile (
        "movl %0, %%esp;"
        "movb $0x20,%%al;"
        "outb %%al,$0x20;"
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
}

/*
 * Panic ISR
 */
void isr_panic();
void dummy_isr_panic()
{
    /*
     *  PUSHL   %EAX        ; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     * Save the context pointer ESP to the PCB
     */
     asm volatile (
        "isr_panic:;"
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

    print_all_processes(kernel_window);
    panic ("service_intr_0x0-0xf: Panic interrupt!");

    /*
     *  Restore context pointer ESP
     *  MOVB  $0x20,%AL ; Reset interrupt controller
     *  OUTB  %AL,$0x20
     *  POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *  IRET        ; Return to new process
     */
    asm volatile (
        "movl %0, %%esp;"
        "movb $0x20,%%al;"
        "outb %%al,$0x20;"
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
}

/*
 * Default ISR
 */
void isr_default();
void dummy_isr_default()
{
    /*
     *  PUSHL   %EAX        ; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     * Save the context pointer ESP to the PCB
     */
     asm volatile (
        "isr_default:;"
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

    /* do nothing */

    /*
     *  Restore context pointer ESP
     *  MOVB  $0x20,%AL ; Reset interrupt controller
     *  OUTB  %AL,$0x20
     *  POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *  IRET        ; Return to new process
     */
    asm volatile (
        "movl %0, %%esp;"
        "movb $0x20,%%al;"
        "outb %%al,$0x20;"
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
}

void wait_for_interrupt (int intr_no)
{
    volatile int saved_if;
    DISABLE_INTR(saved_if);

    // assert(intr_no == TIMER_IRQ || intr_no == KEYB_IRQ || intr_no == COM1_IRQ);
    assert(interrupt_table[intr_no] == NULL);

    remove_ready_queue(active_proc);
    active_proc->state = STATE_INTR_BLOCKED;
    interrupt_table[intr_no] = active_proc;

    ENABLE_INTR(saved_if);

    resign();

    DISABLE_INTR(saved_if);
    if (interrupt_table[intr_no] == active_proc) interrupt_table[intr_no] = NULL;
    ENABLE_INTR(saved_if);
}


void delay ()
{
    asm ("nop;nop;nop");
}

void re_program_interrupt_controller ()
{
    /* Shift IRQ Vectors so they don't collide with the
       x86 generated IRQs */

    // Send initialization sequence to 8259A-1
    asm ("movb $0x11,%al;outb %al,$0x20;call delay");
    // Send initialization sequence to 8259A-2
    asm ("movb $0x11,%al;outb %al,$0xA0;call delay");
    // IRQ base for 8259A-1 is 0x60
    asm ("movb $0x60,%al;outb %al,$0x21;call delay");
    // IRQ base for 8259A-2 is 0x68
    asm ("movb $0x68,%al;outb %al,$0xA1;call delay");
    // 8259A-1 is the master
    asm ("movb $0x04,%al;outb %al,$0x21;call delay");
    // 8259A-2 is the slave
    asm ("movb $0x02,%al;outb %al,$0xA1;call delay");
    // 8086 mode for 8259A-1
    asm ("movb $0x01,%al;outb %al,$0x21;call delay");
    // 8086 mode for 8259A-2
    asm ("movb $0x01,%al;outb %al,$0xA1;call delay");
    // Don't mask IRQ for 8259A-1
    asm ("movb $0x00,%al;outb %al,$0x21;call delay");
    // Don't mask IRQ for 8259A-2
    asm ("movb $0x00,%al;outb %al,$0xA1;call delay");
}

unsigned int get_TOS_time()
{
    return _TOS_time;
}

void init_interrupts()
{
    int i;

    load_idt(idt);

    for(i = 0; i < 16; i++)
    {
        init_idt_entry(i, isr_panic);
    }
    for(; i < MAX_INTERRUPTS; i++)
    {
        init_idt_entry(i, isr_default);
    }

    init_idt_entry(TIMER_IRQ, isr_timer);
    init_idt_entry(KEYB_IRQ, isr_keyb);
    init_idt_entry(COM1_IRQ, isr_com1);

    re_program_interrupt_controller();

    for(i = 0; i < MAX_INTERRUPTS; i++)
    {
        interrupt_table[i] = NULL;
    }

    interrupts_initialized = TRUE;

    asm("sti");
}
