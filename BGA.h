#pragma once

static const int Header_TopSector = 0;
static const int FrameDescs_TopSector = 1;
static const int FrameData_TopSector = 1024;

static int BGA_FramesCount;
static int BGA_MaxSize;
static u8* BGA_pFrameDescs = NULL;

static u8* BGA_pBuf = NULL;
static int BGA_FrameIndex = 0;

static void Raw_ReadHeader() {
	ConsoleWriteLine("Raw: Read header.");

	u8 buf[512];
	if (!sdReadSectors(pdrv, (char*)buf, Header_TopSector, 1)) {
		ConsoleWriteLine("Raw_ReadHeader: Failed.");
		while (true) { delay(1); }
	}

	BGA_FramesCount = (u32)buf[0] << 0;
	BGA_FramesCount |= (u32)buf[1] << 8;
	BGA_MaxSize = (u32)buf[2] << 0;
	BGA_MaxSize |= (u32)buf[3] << 8;

	ConsoleWriteLine("BGA: FramesCount=" + String(BGA_FramesCount));
	ConsoleWriteLine("BGA: MaxSize=" + String(BGA_MaxSize));

	BGA_pBuf = (u8*)malloc((BGA_MaxSize + 511) / 512 * 512);
}

static void Raw_ReadFrameDescs() {
	ConsoleWriteLine("Raw: Read frame descs.");

	const int FrameDescsSectors = ((BGA_FramesCount * 4) + 511) / 512;
	BGA_pFrameDescs = (u8*)malloc(FrameDescsSectors * 512);

	const u32 us = micros();
	if (!sdReadSectors(pdrv, (char*)BGA_pFrameDescs, FrameDescs_TopSector, FrameDescsSectors)) {
		ConsoleWriteLine("Raw_ReadFrameDescs: Failed.");
		while (true) { delay(1); }
	}
	ConsoleWriteLine("Disk read speed: " + String((FrameDescsSectors * 512) / ((double)(micros() - us) / 1000 / 1000) / 1024) + "KiBytes/sec.");
}

static void Raw_ReadFrameData(u32 SectorIndex, u32 SectorsCount) {
	//Serial.println("Raw: Read frame data. " + String(SectorIndex) + " " + String(SectorsCount));

	if (!sdReadSectors(pdrv, (char*)BGA_pBuf, FrameData_TopSector + SectorIndex, SectorsCount)) {
		ConsoleWriteLine("Raw_ReadFrameData: Failed.");
		while (true) { delay(1); }
	}
}

static void BGA_Close() {
	if (BGA_pFrameDescs != NULL) {
		free(BGA_pFrameDescs); BGA_pFrameDescs = NULL;
	}

	if (BGA_pBuf != NULL) {
		free(BGA_pBuf); BGA_pBuf = NULL;
	}
}

static void BGA_Open() {
	BGA_Close();

	Raw_ReadHeader();
	Raw_ReadFrameDescs();
}

static void BGA_DrawFrame_C(const int FrameIndex) {
	const u8* pDescs = &BGA_pFrameDescs[FrameIndex * 4];
	const u32 SectorIndex = ((u32)pDescs[0] << 0) | ((u32)pDescs[1] << 8) | ((u32)pDescs[2] << 16);
	const u8 SectorsCount = pDescs[3];

	Raw_ReadFrameData(SectorIndex, SectorsCount);

	if (vga.bits != 8) {
		Serial.println(String(FrameIndex) + " Not support 16bits VGA mode. " + String(vga.bits));
		while (true) { delay(1); }
	}

	const u32 us = micros();

	const u8* pSrcBuf = BGA_pBuf;

	for (int y = 0; y < 480; y++) {
		u8* pVRAM = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
		pVRAM += (*pSrcBuf++) * 4;
		const u8* pVRAMTerm = &pVRAM[(*pSrcBuf++) * 4];
		while (pVRAM < pVRAMTerm) {
			const u8 Data = *pSrcBuf++;
			switch (Data >> 6) {
			case 0b00: {
				int len = Data & 0b00111111;
				if (len == 0b00111111) { len += *pSrcBuf++; }
				len++;
				for (; 0 < len; len--) { *pVRAM++ = 0b000; }
			} break;
			case 0b01: {
				int len = Data & 0b00111111;
				if (len == 0b00111111) { len += *pSrcBuf++; }
				len++;
				for (; 0 < len; len--) { *pVRAM++ = 0b111; }
			} break;
			case 0b10: {
				*pVRAM++ = (Data >> 0) & 0b111;
				*pVRAM++ = (Data >> 3) & 0b111;
			} break;
			case 0b11: {
				int len = (Data & 0b00111000) >> 3;
				len++;
				byte v = Data & 0b00000111;
				for (; 0 < len; len--) { *pVRAM++ = v; }
			} break;
			}
		}
		VRAMFlush(y);
	}

	Serial.println(String(FrameIndex)); // この行を消すと極端に遅くなる。最適化を阻害している？
}

static void BGA_DrawFrame_Asm(const int FrameIndex) {
	const u8* pDescs = &BGA_pFrameDescs[FrameIndex * 4];
	const u32 SectorIndex = ((u32)pDescs[0] << 0) | ((u32)pDescs[1] << 8) | ((u32)pDescs[2] << 16);
	const u8 SectorsCount = pDescs[3];

	Raw_ReadFrameData(SectorIndex, SectorsCount);

	if (vga.bits != 8) {
		Serial.println(String(FrameIndex) + " Not support 16bits VGA mode. " + String(vga.bits));
		while (true) { delay(1); }
	}

	const u8* pSrcBuf = BGA_pBuf;

	{
		static bool f = false;
		static u8 rtbl[0x100], gtbl[0x100], btbl[0x100];
		if (!f) {
			f = true;
			for (u32 a = 0; a < 0x100; a++) {
				const u32 v = (a * a * a * a) / (0xff * 0xff * 0xff);
				rtbl[a] = v * (1 - 0.299) * 0.25;
				gtbl[a] = v * (1 - 0.587) * 0.25;
				btbl[a] = v * (1 - 0.114) * 0.25;
			}
		}
		const u8 R = *pSrcBuf++;
		const u8 G = *pSrcBuf++;
		const u8 B = *pSrcBuf++;
		ESP32_S3_LED_SetRGBDirect(rtbl[R], gtbl[G], btbl[B]);
	}

	const u32 us = micros();

	for (int y = 0; y < 480; y++) {
		u8* pVRAM = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
		pVRAM += (*pSrcBuf++) * 4;
		const u8* pVRAMTerm = &pVRAM[(*pSrcBuf++) * 4];

		if (pVRAM != pVRAMTerm) {
			u8 Data, len, Value;
			u8 BitMask6 = 0b00111111;
			asm volatile(
				".Start: \n"
				"l8ui %[Data], %[pSrcBuf], 0 \n"
				"addi %[pSrcBuf], %[pSrcBuf], 1 \n"

				"extui %[Value], %[Data], 6, 2 \n"
				//"beqi %[Value], 0, .Start_0b00 \n"
				"beqi %[Value], 1, .Start_0b01 \n"
				"beqi %[Value], 2, .Start_0b10 \n"
				"beqi %[Value], 3, .Start_0b11 \n"

				".Start_0b00: \n"
				"extui %[len], %[Data], 0, 6 \n"
				"bne %[len], %[BitMask6], .Skip_0b00 \n"
				"l8ui %[len], %[pSrcBuf], 0 \n"
				"addi %[pSrcBuf], %[pSrcBuf], 1 \n"
				"add %[len], %[len], %[BitMask6] \n"
				".Skip_0b00: \n"
				"addi %[len], %[len], 1 \n"
				"movi %[Value], 0x00 \n"
				"loop %[len],.LoopEnd_0b00 \n"
				"  s8i %[Value], %[pVRAM], 0 \n"
				"  addi %[pVRAM], %[pVRAM], 1 \n"
				".LoopEnd_0b00: \n"
				"bne %[pVRAM], %[pVRAMTerm], .Start \n"
				"j .End \n"

				".Start_0b01: \n"
				"extui %[len], %[Data], 0, 6 \n"
				"bne %[len], %[BitMask6], .Skip_0b01 \n"
				"l8ui %[len], %[pSrcBuf], 0 \n"
				"addi %[pSrcBuf], %[pSrcBuf], 1 \n"
				"add %[len], %[len], %[BitMask6] \n"
				".Skip_0b01: \n"
				"addi %[len], %[len], 1 \n"
				"movi %[Value], 0x07 \n"
				"loop %[len],.LoopEnd_0b01 \n"
				"  s8i %[Value], %[pVRAM], 0 \n"
				"  addi %[pVRAM], %[pVRAM], 1 \n"
				".LoopEnd_0b01: \n"
				"bne %[pVRAM], %[pVRAMTerm], .Start \n"
				"j .End \n"

				".Start_0b10: \n"
				"extui %[Value], %[Data], 0, 3 \n"
				"s8i %[Value], %[pVRAM], 0 \n"
				"extui %[Value], %[Data], 3, 3 \n"
				"s8i %[Value], %[pVRAM], 1 \n"
				"addi %[pVRAM], %[pVRAM], 2 \n"
				"bne %[pVRAM], %[pVRAMTerm], .Start \n"
				"j .End \n"

				".Start_0b11: \n"
				"extui %[len], %[Data], 3, 3 \n"
				"addi %[len], %[len], 1 \n"
				"extui %[Value], %[Data], 0, 3 \n"
				"loop %[len],.LoopEnd_0b11 \n"
				"  s8i %[Value], %[pVRAM], 0 \n"
				"  addi %[pVRAM], %[pVRAM], 1 \n"
				".LoopEnd_0b11: \n"
				"bne %[pVRAM], %[pVRAMTerm], .Start \n"

				".End: \n"

				: [pVRAM] "m+r" (pVRAM), [pVRAMTerm] "m+r" (pVRAMTerm), [pSrcBuf] "m+r" (pSrcBuf), [Data] "l=r" (Data), [len] "l=r" (len), [Value] "l=r" (Value), [BitMask6] "l+r" (BitMask6) // output_list
				: // input_list
				: // clobber-list
				);
			VRAMFlush(y);
		}
	}
}


