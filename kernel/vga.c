
#include <kernel.h>

int graphics_mode;

// code originally from
// http://xkr47.outerspace.dyndns.org/progs/mode%2013h%20without%20using%20bios.htm
//
// vga mode switcher by Jonas Berlin -98 <jberlin@cc.hut.fi>
// [ Jonas Berlin / xkr~47 ]
// [ tel: +358-9-5483430 / +358-40-5884454 ]
// [ Kiloberget 10 F 55, 02610 Esbo, Finland ]
// [ mailto:jberlin@cc.hut.fi ]
// [ http://www.hut.fi/~jberlin ]
// 
// paired down to what we need only
// 

#define SZ(x) (sizeof(x)/sizeof(x[0]))


static const BYTE hor_regs [] = { 0x0,  0x1,  0x2,  0x3,  0x4, 
    0x5,  0x13 };

static const BYTE ver_regs  [] = { 0x6,  0x7,  0x9,  0x10, 0x11,
    0x12, 0x15, 0x16 };

static const BYTE width_320[] = { 0x5f, 0x4f, 0x50, 0x82, 0x54,
    0x80, 0x28 };

static const BYTE height_200[] = { 0xbf, 0x1f, 0x41, 0x9c, 0x8e,
    0x8f, 0x96, 0xb9 };

static const BYTE text_width_80[] = { 0x5f, 0x4f, 0x50, 0x82, 0x55,
    0x81, 0x28 };

static const BYTE text_height_25[] = { 0xbf, 0x1f, 0x4f, 0x9c, 0x8e,
    0x8f, 0x96, 0xb9 };


unsigned char saved_text_memory[4][GRAPHICS_MEM_PLANE_SIZE];

void save_graphics_memory(unsigned char memory[4][GRAPHICS_MEM_PLANE_SIZE])
{
    int i;
    unsigned char state;

    outportb(0x3CE, 0x04); state = inportb(0x3CF); // save current read map select register

    for (i = 0; i < 4; i++)
    {
        outportb(0x3CE, 0x04); outportb(0x3CF, i); // set plane to read
        k_memcpy(&memory[i][0], (BYTE *)WINDOW_BASE_ADDR, GRAPHICS_MEM_PLANE_SIZE);
    }

    outportb(0x3CE, 0x04); outportb(0x3CF, state); // restore read map select register
}

void restore_graphics_memory(unsigned char memory[4][GRAPHICS_MEM_PLANE_SIZE])
{
    int i;
    unsigned char state;

    outportb(0x3C4, 0x02); state = inportb(0x3C5); // save current write map select register

    for (i = 0; i < 4; i++)
    {
        outportb(0x3C4, 0x04); outportb(0x3C5, 1 << i); // set plane to write
        k_memcpy((BYTE *)WINDOW_BASE_ADDR, &memory[i][0], GRAPHICS_MEM_PLANE_SIZE);
    }

    outportb(0x3CE, 0x02); outportb(0x3C5, state); // restore write map select register
}

void save_colors(unsigned char palette[256][3])
{
    int i;
    // init PEL Address Read Mode Register to 0 to read all colors
    outportb(0x3C7, 0);

    for(i = 0; i < 256; i++)
    {
        // read colors to PEL Data Register
        palette[i][0] = inportb(0x3C9);
        palette[i][1] = inportb(0x3C9);
        palette[i][2] = inportb(0x3C9);
    }
}

void set_colors(unsigned char palette[256][3], int shift)
{
    int i;
    // init PEL Address Write Mode Register to 0 to write all colors
    outportb(0x3C8, 0);

    for(i = 0; i < 256; i++)
    {
        // write colors to PEL Data Register
        outportb(0x3C9, palette[i][0] >> shift);
        outportb(0x3C9, palette[i][1] >> shift);
        outportb(0x3C9, palette[i][2] >> shift);
    }
}

int start_graphic_vga()
{
    int i;

    if (graphics_mode == VGA_MODE)
    {
        return 1;
    }

    save_graphics_memory(saved_text_memory);

    graphics_mode = VGA_MODE;

    // here goes the actual modeswitch

    outportb(0x3c2,0x63);
    outportw(0x3d4,0x0e11); // enable regs 0-7

    for(i=0;i<SZ(hor_regs);++i) 
        outportw(0x3d4,(WORD)((width_320[i]<<8)|hor_regs[i]));
    for(i=0;i<SZ(ver_regs);++i)
        outportw(0x3d4,(WORD)((height_200[i]<<8)|ver_regs[i]));

    outportw(0x3d4,0x0008); // vert.panning = 0

    // chain4
    outportw(0x3d4,0x4014);
    outportw(0x3d4,0xa317);
    outportw(0x3c4,0x0e04);

    outportw(0x3c4,0x0101);
    outportw(0x3c4,0x0f02); // enable writing to all planes
    outportw(0x3ce,0x4005); // 256color mode
    outportw(0x3ce,0x0506); // graph mode & A000-AFFF

    inportb(0x3da);
    outportb(0x3c0,0x30); outportb(0x3c0,0x41);
    outportb(0x3c0,0x33); outportb(0x3c0,0x00);

    for(i=0;i<16;i++) {    // ega pal
        outportb(0x3c0,(BYTE)i); 
        outportb(0x3c0,(BYTE)i); 
    } 

    outportb(0x3c0, 0x20); // enable video

    return 1;
}

//////////////////////////////////////////////////////////////////////////
int start_text_mode()
{
    int i;

    if (graphics_mode == TEXT_MODE)
    {
        return 1;
    }

    // here goes the actual modeswitch

    outportb(0x3c2,0x67);
    outportw(0x3d4,0x0e11); // disable write protection regs 0-7

    for(i=0;i<SZ(hor_regs);++i) 
        outportw(0x3d4,(WORD)((text_width_80[i]<<8)|hor_regs[i]));
    for(i=0;i<SZ(ver_regs);++i)
        outportw(0x3d4,(WORD)((text_height_25[i]<<8)|ver_regs[i]));

    outportw(0x3d4,0x0008); // vert.panning = 0

    // chain4
    outportw(0x3d4,0x1f14);
    outportw(0x3d4,0xa317);
    outportw(0x3c4,0x0704);

    outportw(0x3c4,0x0001);
    outportw(0x3c4,0x0302); // set which planes to write

    outportw(0x3ce,0x1005); // odd/even addressing mode of char, attr
    outportw(0x3ce,0x0e06); // text mode & odd/even addressing & B8000-BFFFF

    inportb(0x3da);
    outportb(0x3c0,0x30); outportb(0x3c0,0x0c); // enable blink, line graphics
    outportb(0x3c0,0x31); outportb(0x3c0,0x00); // verscan color 0
    outportb(0x3c0,0x32); outportb(0x3c0,0x0f); // enable 4 color planes
    outportb(0x3c0,0x33); outportb(0x3c0,0x08); // pixel shift 8
    outportb(0x3c0,0x34); outportb(0x3c0,0x00); // upper color bits 0000

    // reset text pallette
    outportb(0x3c0,0x00); outportb(0x3c0,0x00);
    outportb(0x3c0,0x01); outportb(0x3c0,0x01);
    outportb(0x3c0,0x02); outportb(0x3c0,0x02);
    outportb(0x3c0,0x03); outportb(0x3c0,0x03);
    outportb(0x3c0,0x04); outportb(0x3c0,0x04);
    outportb(0x3c0,0x05); outportb(0x3c0,0x05);
    outportb(0x3c0,0x06); outportb(0x3c0,0x14);
    outportb(0x3c0,0x07); outportb(0x3c0,0x07);
    outportb(0x3c0,0x08); outportb(0x3c0,0x38);
    outportb(0x3c0,0x09); outportb(0x3c0,0x39);
    outportb(0x3c0,0x0a); outportb(0x3c0,0x3a);
    outportb(0x3c0,0x0b); outportb(0x3c0,0x3b);
    outportb(0x3c0,0x0c); outportb(0x3c0,0x3c);
    outportb(0x3c0,0x0d); outportb(0x3c0,0x3d);
    outportb(0x3c0,0x0e); outportb(0x3c0,0x3e);
    outportb(0x3c0,0x0f); outportb(0x3c0,0x3f);
    outportb(0x3c0,0x10); outportb(0x3c0,0x0c);
    outportb(0x3c0,0x11); outportb(0x3c0,0x00);
    outportb(0x3c0,0x12); outportb(0x3c0,0x0f);
    outportb(0x3c0,0x13); outportb(0x3c0,0x08);
    outportb(0x3c0,0x14); outportb(0x3c0,0x00);

    outportb(0x3c0, 0x20); // enable video

    restore_graphics_memory(saved_text_memory);

    graphics_mode = TEXT_MODE;

    return 1;
}


void init_graphics()
{
    graphics_mode = TEXT_MODE;
}