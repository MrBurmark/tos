
#include <kernel.h>
#include <tos_logo.h>

#define TOS_LOGO_GRAPHICS_WINDOW_OFFSET ( (GRAPHICS_WINDOW_TOTAL_HEIGHT / 2 - tos_logo_height / 2) * GRAPHICS_WINDOW_TOTAL_WIDTH \
        + (GRAPHICS_WINDOW_TOTAL_WIDTH / 2 - tos_logo_width / 2))


unsigned char default_palette[256][3];

void draw_tos_logo(unsigned char *data)
{
    int i;
    // int j;    
    unsigned char *window = (unsigned char*)GRAPHICS_WINDOW_BASE_ADDR + TOS_LOGO_GRAPHICS_WINDOW_OFFSET;

	for (i = 0; i < tos_logo_height; i++)
        k_memcpy(window + i*GRAPHICS_WINDOW_TOTAL_WIDTH, data + i*tos_logo_width, tos_logo_width);
	// for (j = 0; j < tos_logo_width; j++)
	// {
	//     window[i*GRAPHICS_WINDOW_TOTAL_WIDTH + j] = header_data[i*tos_logo_width + j];
        
	// }
}


void tos_splash_screen(int time)
{

    assert( start_graphic_vga() );

    save_colors(default_palette);

    set_colors(header_data_cmap, 2);

    // clear screen
    k_memset((unsigned char *)GRAPHICS_WINDOW_BASE_ADDR, 0x00, GRAPHICS_WINDOW_TOTAL_WIDTH * GRAPHICS_WINDOW_TOTAL_HEIGHT);

    while (time > 0)
    {
        // draw tos logo
        draw_tos_logo(header_data2);
        sleep(LOGO_ANIMATION_SLIDE_TIME);

        draw_tos_logo(header_data);
        sleep(LOGO_ANIMATION_SLIDE_TIME);

        time -= 2 * LOGO_ANIMATION_SLIDE_TIME;
    }

    set_colors(default_palette, 0);

    assert( start_text_mode() );
}
