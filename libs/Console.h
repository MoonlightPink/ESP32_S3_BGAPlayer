#pragma once

#include "font-9x8.h"
#include "font-9x16.h"
#include "font-12x24.h"
#include "font-16x32.h"

static int Console_Line = 0;
static int Console_FontHeight = 8;

static void ConsoleSetLine(int Line) {
	Console_Line = Line;
}

static void ConsoleSetFontHeight(int h) {
	Console_FontHeight = h;
}

static void ConsoleScroll(int Height) {
	Console_Line += Height;

	if (VGAMode.vRes < Console_Line) {
		const int v = Console_Line - VGAMode.vRes;
		for (int y = 0; y < VGAMode.vRes - v; y++) {
			u8* psrc = vga.dmaBuffer->getLineAddr8(y + v, vga.backBuffer);
			u8* pdst = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
			for (int x = 0; x < VGAMode.hRes; x++) {
				*pdst++ = *psrc++;
			}
			VRAMFlush(y);
		}
		for (int y = VGAMode.vRes - v; y < VGAMode.vRes; y++) {
			u8* pdst = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
			for (int x = 0; x < VGAMode.hRes; x++) {
				*pdst++ = 0x00;
			}
			VRAMFlush(y);
		}
		Console_Line -= v;
	}

	Console_Line -= Height;
}

static void ConsoleWriteLine(int Line, String Text) {
	const u16* pFont;
	int Width, Height;

	switch (Console_FontHeight) {
	case 8: pFont = console_font_9x8; Width = 9; Height = 8; break;
	case 16: pFont = console_font_9x16; Width = 9; Height = 16; break;
	case 24: pFont = console_font_12x24; Width = 12; Height = 24; break;
	case 32: pFont = console_font_16x32; Width = 16; Height = 32; break;
	default: Serial.println("Illigal font height. Console_FontHeight:" + String(Console_FontHeight));
	}

	for (int chidx = 0; chidx < Text.length(); chidx++) {
		u8 ch = Text[chidx];
		const u16* pbm = &pFont[Height * ch];
		for (int y = 0; y < Height; y++) {
			u16 bm = *pbm++;
			for (int x = 0; x < Width; x++) {
				u8 col = (bm & 0x8000) ? 0xff : 0x00;
				bm <<= 1;
				vga.dot(chidx * Width + x, Line + y, vga.rgb(col, col, col));
			}
		}
	}
	for (int y = 0; y < Height; y++) {
		VRAMFlush(Line + y);
	}
}

static void ConsoleWriteLine(String Text) {
	int PaddingY;
	switch (Console_FontHeight) {
	case 8: PaddingY = 1; break;
	case 16: PaddingY = 0; break;
	case 24: PaddingY = 0; break;
	case 32: PaddingY = 0; break;
	default: Serial.println("Illigal font height. Console_FontHeight:" + String(Console_FontHeight));
	}

	ConsoleScroll(Console_FontHeight + PaddingY);
	ConsoleWriteLine(Console_Line, Text);
	Console_Line += Console_FontHeight + PaddingY;
}