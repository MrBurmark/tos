
#include <kernel.h>

void kernel_main()
{
    // this turns off the VGA hardware cursor
    // otherwise we get an annoying, meaningless,
    // blinking cursor in the middle of our screen
    outportb(0x03D4, 0x0E);
    outportb(0x03D5, 0xFF);
    outportb(0x03D4, 0x0F);
    outportb(0x03D5, 0xFF);

    clear_window(kernel_window);

    init_process();
    init_dispatcher();
    init_ipc();
    init_interrupts();
    init_null_process();
    init_timer();

    // tos_splash_screen(1000);

    init_com();
    init_keyb();
    init_shell();
    // init_pacman(pacman_wnd, 2);

    exit();
    // exit should never return
    while (1);
}
