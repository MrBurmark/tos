
#include <kernel.h>

void kernel_main()
{
    unsigned char color = 0;
    unsigned char *window;

    // this turns off the VGA hardware cursor
    // otherwise we get an annoying, meaningless,
    // blinking cursor in the middle of our screen
    outportb(0x03D4, 0x0E);
    outportb(0x03D5, 0xFF);
    outportb(0x03D4, 0x0F);
    outportb(0x03D5, 0xFF);

    // init_process();
    // init_dispatcher();
    // init_ipc();
    // init_interrupts();
    // init_null_process();
    // init_timer();
    // init_com();
    // init_keyb();
    // init_shell();
    // init_pacman(kernel_window, 3);

    assert( init_graph_vga(320,200,1));

    // blank screen
    for(window = (unsigned char *)0xA0000; window < (unsigned char *)(0xA0000 + 320*200); window++)
        *window = color;

    // draw tos logo
    draw_tos_logo();

    // draw color gradient line across middle of screen
    for(window = (unsigned char *)(0xA0000 + 100*320); window < (unsigned char *)(0xA0000 + 320*100 + 256); window++)
        *window = color++;

    while (1);
}
