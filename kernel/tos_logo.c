
#include <kernel.h>
#include <tos_logo.h>

#define WINDOW_OFFSET ( (GRAPHICS_WINDOW_TOTAL_HEIGHT / 2 - tos_logo_height / 2) * GRAPHICS_WINDOW_TOTAL_WIDTH \
        + (GRAPHICS_WINDOW_TOTAL_WIDTH / 2 - tos_logo_width / 2))

void set_tos_colors(void)
{
    int i;
    // init PEL Address Write Mode Register to 0 to write all colors
    outportb(0x3C8, 0);

    for(i = 0; i < 256; i++)
    {
        // output colors to PEL Data Register
        // note VGA supports 18 bit not 24 bit color
        outportb(0x3C9, header_data_cmap[i][0] / 4);
        outportb(0x3C9, header_data_cmap[i][1] / 4);
        outportb(0x3C9, header_data_cmap[i][2] / 4);
    }
}


void draw_tos_logo(void)
{
    int i;
    int j;    
    unsigned char *window = (unsigned char*)GRAPHICS_WINDOW_BASE_ADDR + WINDOW_OFFSET;

    set_tos_colors();
    while(1){
    	for (i = 0; i < tos_logo_height; i++)
            k_memcpy(window + i*GRAPHICS_WINDOW_TOTAL_WIDTH, header_data + i*tos_logo_width, tos_logo_width);
		// for (j = 0; j < tos_logo_width; j++)
		// {
		//     window[i*GRAPHICS_WINDOW_TOTAL_WIDTH + j] = header_data[i*tos_logo_width + j];
            
		// }
        sleep(15);
        for (i = 0; i < tos_logo_height; i++)
            k_memcpy(window + i*GRAPHICS_WINDOW_TOTAL_WIDTH, header_data2 + i*tos_logo_width, tos_logo_width);
		// for (j = 0; j < tos_logo_width; j++)
		// {
		//     window[i*GRAPHICS_WINDOW_TOTAL_WIDTH + j] = header_data2[i*tos_logo_width + j];
		// }
        sleep(15);
        
        }

}
