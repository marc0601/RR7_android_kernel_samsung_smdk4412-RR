#ifndef __MDNIE_TABLE_EBOOK_H__
#define __MDNIE_TABLE_EBOOK_H__

#include "mdnie_kona.h"

#if defined(CONFIG_FB_MDNIE_PWM)
static unsigned short tune_ebook[] = {
    0x0000,0x0000,  /*BANK 0*/
	0x0008,0x00a0,  /*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030,0x0000,  /*FA cs1 de8 hdr2 fa1*/
	0x0090,0x0080,  /*DE egth*/
	0x0092,0x0030,  /*DE pe*/
	0x0093,0x0030,  /*DE pf*/
	0x0094,0x0030,  /*DE pb*/
	0x0095,0x0030,  /*DE ne*/
	0x0096,0x0030,  /*DE nf*/
	0x0097,0x0030,  /*DE nb*/
	0x0098,0x1000,  /*DE max ratio*/
	0x0099,0x0100,  /*DE min ratio*/
	0x00b0,0x1010,  /*CS hg ry*/
	0x00b1,0x1010,  /*CS hg gc*/
	0x00b2,0x1010,  /*CS hg bm*/
	0x00b3,0x1804,  /*CS weight grayTH*/
	0x00e1,0xff00,  /*SCR RrCr*/
	0x00e2,0x00ff,  /*SCR RgCg*/
	0x00e3,0x00ff,  /*SCR RbCb*/
	0x00e4,0x00ff,  /*SCR GrMr*/
	0x00e5,0xff00,  /*SCR GgMg*/
	0x00e6,0x00ff,  /*SCR GbMb*/
	0x00e7,0x00ff,  /*SCR BrYr*/
	0x00e8,0x00ff,  /*SCR BgYg*/
	0x00e9,0xff00,  /*SCR BbYb*/
	0x00ea,0x00ff,	/*SCR KrWr*/
	0x00eb,0x00ef,	/*SCR KgWg*/
	0x00ec,0x00e4,	/*SCR KbWb*/
	0x0000,0x0001,  /*BANK 1*/
	0x001f,0x0080,  /*CC chsel strength*/
	0x0020,0x0000,  /*CC lut r	 0*/
	0x0021,0x1090,  /*CC lut r	16 144*/
	0x0022,0x20a0,  /*CC lut r	32 160*/
	0x0023,0x30b0,  /*CC lut r	48 176*/
	0x0024,0x40c0,  /*CC lut r	64 192*/
	0x0025,0x50d0,  /*CC lut r	80 208*/
	0x0026,0x60e0,  /*CC lut r	96 224*/
	0x0027,0x70f0,  /*CC lut r 112 240*/
	0x0028,0x80ff,  /*CC lut r 128 255*/
	0x00ff,0x0000,  /*Mask Release*/
	END_SEQ, 0x0000,
};

static unsigned short tune_ebook_cabc[] = {
	0x0000,0x0000,	/*BANK 0*/
	0x0008,0x02a0,	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030,0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090,0x0080,	/*DE egth*/
	0x0092,0x0030,	/*DE pe*/
	0x0093,0x0030,	/*DE pf*/
	0x0094,0x0030,	/*DE pb*/
	0x0095,0x0030,	/*DE ne*/
	0x0096,0x0030,	/*DE nf*/
	0x0097,0x0030,	/*DE nb*/
	0x0098,0x1000,	/*DE max ratio*/
	0x0099,0x0100,	/*DE min ratio*/
	0x00b0,0x1010,	/*CS hg ry*/
	0x00b1,0x1010,	/*CS hg gc*/
	0x00b2,0x1010,	/*CS hg bm*/
	0x00b3,0x1804,	/*CS weight grayTH*/
	0x00e1,0xff00,	/*SCR RrCr*/
	0x00e2,0x00ff,	/*SCR RgCg*/
	0x00e3,0x00ff,	/*SCR RbCb*/
	0x00e4,0x00ff,	/*SCR GrMr*/
	0x00e5,0xff00,	/*SCR GgMg*/
	0x00e6,0x00ff,	/*SCR GbMb*/
	0x00e7,0x00ff,	/*SCR BrYr*/
	0x00e8,0x00ff,	/*SCR BgYg*/
	0x00e9,0xff00,	/*SCR BbYb*/
	0x00ea,0x00ff,	/*SCR KrWr*/
	0x00eb,0x00ef,	/*SCR KgWg*/
	0x00ec,0x00e4,	/*SCR KbWb*/
	0x0000,0x0001,	/*BANK 1*/
	0x001f,0x0080,	/*CC chsel strength*/
	0x0020,0x0000,	/*CC lut r	 0*/
	0x0021,0x1090,	/*CC lut r	16 144*/
	0x0022,0x20a0,	/*CC lut r	32 160*/
	0x0023,0x30b0,	/*CC lut r	48 176*/
	0x0024,0x40c0,	/*CC lut r	64 192*/
	0x0025,0x50d0,	/*CC lut r	80 208*/
	0x0026,0x60e0,	/*CC lut r	96 224*/
	0x0027,0x70f0,	/*CC lut r 112 240*/
	0x0028,0x80ff,	/*CC lut r 128 255*/
	0x0075,0x0000,	/*CABC dgain*/
	0x0076,0x0000,
	0x0077,0x0000,
	0x0078,0x0000,
	0x007f,0x0002,	/*dynamic lcd*/
	0x00ff,0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};
#else
static unsigned short tune_ebook[] = {
	0x0000,0x0000,	/*BANK 0*/
	0x0008,0x00a0,	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030,0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090,0x0080,	/*DE egth*/
	0x0092,0x0030,	/*DE pe*/
	0x0093,0x0030,	/*DE pf*/
	0x0094,0x0030,	/*DE pb*/
	0x0095,0x0030,	/*DE ne*/
	0x0096,0x0030,	/*DE nf*/
	0x0097,0x0030,	/*DE nb*/
	0x0098,0x1000,	/*DE max ratio*/
	0x0099,0x0100,	/*DE min ratio*/
	0x00b0,0x1010,	/*CS hg ry*/
	0x00b1,0x1010,	/*CS hg gc*/
	0x00b2,0x1010,	/*CS hg bm*/
	0x00b3,0x1804,	/*CS weight grayTH*/
	0x00e1,0xff00,	/*SCR RrCr*/
	0x00e2,0x00ff,	/*SCR RgCg*/
	0x00e3,0x00ff,	/*SCR RbCb*/
	0x00e4,0x00ff,	/*SCR GrMr*/
	0x00e5,0xff00,	/*SCR GgMg*/
	0x00e6,0x00ff,	/*SCR GbMb*/
	0x00e7,0x00ff,	/*SCR BrYr*/
	0x00e8,0x00ff,	/*SCR BgYg*/
	0x00e9,0xff00,	/*SCR BbYb*/
	0x00ea,0x00ff,	/*SCR KrWr*/
	0x00eb,0x00ef,	/*SCR KgWg*/
	0x00ec,0x00e4,	/*SCR KbWb*/
	0x0000,0x0001,	/*BANK 1*/
	0x001f,0x0080,	/*CC chsel strength*/
	0x0020,0x0000,	/*CC lut r	 0*/
	0x0021,0x1090,	/*CC lut r	16 144*/
	0x0022,0x20a0,	/*CC lut r	32 160*/
	0x0023,0x30b0,	/*CC lut r	48 176*/
	0x0024,0x40c0,	/*CC lut r	64 192*/
	0x0025,0x50d0,	/*CC lut r	80 208*/
	0x0026,0x60e0,	/*CC lut r	96 224*/
	0x0027,0x70f0,	/*CC lut r 112 240*/
	0x0028,0x80ff,	/*CC lut r 128 255*/
	0x00ff,0x0000,	/*Mask Release*/
	END_SEQ, 0x0000,
};
#endif

struct mdnie_tuning_info ebook_table[CABC_MAX] = {
#if defined(CONFIG_FB_MDNIE_PWM)
	{"EBOOK",		tune_ebook},
	{"EBOOK_CABC",	tune_ebook_cabc},
#else
	{"EBOOK",		tune_ebook},
#endif
};
#endif /* __MDNIE_TABLE_EBOOK_H__ */
