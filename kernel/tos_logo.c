
#include <kernel.h>
#include <tos_logo.h>

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
    unsigned char *window = (unsigned char*)0xA0000;

    set_tos_colors();

    for (i = 0; i < tos_logo_height; i++)
        for (j = 0; j < tos_logo_width; j++)
        {
            window[i*320 + j] = header_data[i*tos_logo_width + j];
        }

}