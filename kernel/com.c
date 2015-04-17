/* 
 * Internet ressources:
 * 
 * http://workforce.cup.edu/little/serial.html
 *
 * http://www.lammertbies.nl/comm/info/RS-232.html
 *
 */


#include <kernel.h>

PORT com_port;


void com_reader_process (PROCESS self, PARAM param) 
{
    int i;
    COM_Message *msg;
    PROCESS com_proc;
    PORT com_port2 = (PORT)param;

    while (1) { 

        msg = receive(&com_proc);

        // read
        for(i = 0; i < msg->len_input_buffer; i++)
        {
            wait_for_interrupt (COM1_IRQ);
            msg->input_buffer[i] = inportb(COM1_PORT);
        }

        message(com_port2, (void *)msg);
    } 
}

void com_process (PROCESS self, PARAM param) 
{
    int i;
    COM_Message *msg;
    PROCESS user_proc;
    PROCESS reader_proc;
    PORT com_reader_port = create_new_port(active_proc);
    com_reader_port = create_process(com_reader_process, 7, (LONG)com_reader_port, "COM_reader");
    
    while (1) { 
        
        msg = receive(&user_proc);

        close_port(com_port);

        message(com_reader_port, (void *)msg);

        kprintf("%s\n", msg->output_buffer);

        // write
        for(i = 0; msg->output_buffer[i] != '\0'; i++)
        {
            kprintf("%c", msg->output_buffer[i]);

            while (!(inportb(COM1_PORT+5) & (1<<5)));

            outportb(COM1_PORT, msg->output_buffer[i]);

            kprintf("%c", msg->output_buffer[i]);
        }

        // wait for reader to finish
        receive(&reader_proc);

        // done
        open_port(com_port);

        reply(user_proc);
    } 
}


// void init_uart()
// {
//     /* LineControl disabled to set baud rate */
//     outportb (COM1_PORT + 3, 0x80);
//     /* lower byte of baud rate */
//     outportb (COM1_PORT + 0, 0x30);
//     /* upper byte of baud rate */
//     outportb (COM1_PORT + 1, 0x00);
//     /* 8 Bits, No Parity, 2 stop bits */
//     outportb (COM1_PORT + 3, 0x07);
//     /* Interrupt enable*/
//     outportb (COM1_PORT + 1, 1);
//     /* Modem control */
//     outportb (COM1_PORT + 4, 0x0b);
//     inportb (COM1_PORT);
// }

void init_uart()
{ 
 int divisor; 
 divisor = 115200 / 1200; 
 /* LineControl disabled to set baud rate */ 
 outportb (COM1_PORT + 3, 0x80); 
 /* lower byte of baud rate */ 
 outportb (COM1_PORT, divisor & 255); 
 /* upper byte of baud rate */ 
 outportb (COM1_PORT + 1, (divisor >> 8) & 255); 
 /* LineControl 2 stop bits */ 
 outportb (COM1_PORT + 3, 2); 
 /* Interrupt enable*/ 
 outportb (COM1_PORT + 1, 1); 
 /* Modem control */ 
 outportb (COM1_PORT + 4, 0x0b); 
 inportb (COM1_PORT); 
}

void init_com ()
{
    init_uart();

    com_port = create_process(com_process, 6, 0, "COM_process");
}
