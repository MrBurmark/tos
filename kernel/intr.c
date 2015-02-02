
#include <kernel.h>

BOOL interrupts_initialized = FALSE;

IDT idt [MAX_INTERRUPTS];
PROCESS interrupt_table [MAX_INTERRUPTS];
PROCESS p;


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
}




/*
 * Timer ISR
 */
void isr_timer ();
void dummy_isr_timer ()
{
}



/*
 * COM1 ISR
 */
void isr_com1 ();
void dummy_isr_com1 ()
{
}


/*
 * Keyboard ISR
 */
void isr_keyb();
void dummy_isr_keyb()
{
    /*
     *	PUSHL	%EAX		; Save process' context
     *  PUSHL   %ECX
     *  PUSHL   %EDX
     *  PUSHL   %EBX
     *  PUSHL   %EBP
     *  PUSHL   %ESI
     *  PUSHL   %EDI
     */
    asm ("isr_keyb:");
    asm ("pushl %eax;pushl %ecx;pushl %edx");
    asm ("pushl %ebx;pushl %ebp;pushl %esi;pushl %edi");

    /* Save the context pointer ESP to the PCB */
    asm ("movl %%esp,%0" : "=m" (active_proc->esp) : );

    p = interrupt_table[KEYB_IRQ];

    if (p == NULL) {
	panic ("service_intr_0x61: Spurious interrupt");
    }

    if (p->state != STATE_INTR_BLOCKED) {
	panic ("service_intr_0x61: No process waiting");
    }

    /* Add event handler to ready queue */
    add_ready_queue (p);

    active_proc = dispatcher();

    /* Restore context pointer ESP */
    asm ("movl %0,%%esp" : : "m" (active_proc->esp) );

    /*
     *	MOVB  $0x20,%AL	; Reset interrupt controller
     *	OUTB  %AL,$0x20
     *	POPL  %EDI      ; Restore previously saved context
     *  POPL  %ESI
     *  POPL  %EBP
     *  POPL  %EBX
     *  POPL  %EDX
     *  POPL  %ECX
     *  POPL  %EAX
     *	IRET		; Return to new process
     */
    asm ("movb $0x20,%al;outb %al,$0x20");
    asm ("popl %edi;popl %esi;popl %ebp;popl %ebx");
    asm ("popl %edx;popl %ecx;popl %eax");
    asm ("iret");
}

void wait_for_interrupt (int intr_no)
{
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

void init_interrupts()
{
}
