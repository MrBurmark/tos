
#include <kernel.h>

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

static const BYTE width_320[] = { 0x5f, 0x4f, 0x50, 0x82, 0x54,
    0x80, 0x28 };

static const BYTE ver_regs  [] = { 0x6,  0x7,  0x9,  0x10, 0x11,
    0x12, 0x15, 0x16 };

static const BYTE height_200[] = { 0xbf, 0x1f, 0x41, 0x9c, 0x8e,
    0x8f, 0x96, 0xb9 };

int init_graph_vga()
// returns 1=ok, 0=fail
{
    int i;

    // here goes the actual modeswitch

    outportb(0x3c2,0x63);
    outportw(0x3d4,0x0e11); // enable regs 0-7

    for(i=0;i<SZ(hor_regs);++i) 
        outportw(0x3d4,(WORD)((width_320[i]<<8)+hor_regs[i]));
    for(i=0;i<SZ(ver_regs);++i)
        outportw(0x3d4,(WORD)((height_200[i]<<8)+ver_regs[i]));

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