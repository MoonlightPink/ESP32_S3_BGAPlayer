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

	const int FrameDescsSectors = ((BGA_FramesCount * 5) + 511) / 512;
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

static void BGA_DrawFrame(const int FrameIndex) {
	const u8* pDescs = &BGA_pFrameDescs[FrameIndex * 5];
	const u32 SectorIndex = ((u32)pDescs[0] << 16) | ((u32)pDescs[1] << 8) | ((u32)pDescs[2] << 0);
	const u8 SectorsCount = pDescs[3];
	const u8 bpp = pDescs[4];

	Raw_ReadFrameData(SectorIndex, SectorsCount);

	if (vga.bits != 8) {
		Serial.println(String(FrameIndex) + " Not support 16bits VGA mode. " + String(vga.bits));
		while (true) { delay(1); }
	}

	const u32 us = micros();

	const u8* pSrcBuf = BGA_pBuf;
	switch (bpp) {
	case 1: {
		for (int y = 0; y < 480; y++) {
			u8* pVRAM = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
			const u8* pVRAMTerm = &pVRAM[640];
			while (pVRAM < pVRAMTerm) {
				const u8 Data = *pSrcBuf++;
				int len = Data & 0b00111111;
				if (len == 0b00111111) { len += *pSrcBuf++; }
				len++;
				switch (Data >> 6) {
				case 0b00: {
					for (; 0 < len; len--) { *pVRAM++ = 0b000; }
				} break;
				case 0b01: {
					for (; 0 < len; len--) { *pVRAM++ = 0b111; }
				} break;
				}
			}
			VRAMFlush(y);
		}
	} break;
	case 2: {
		for (int y = 0; y < 480; y++) {
			u8* pVRAM = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
			const u8* pVRAMTerm = &pVRAM[640];
			while (pVRAM < pVRAMTerm) {
				const u8 Data = *pSrcBuf++;
				int len = Data & 0b00111111;
				if (len == 0b00111111) { len += *pSrcBuf++; }
				len++;
				switch (Data >> 6) {
				case 0b00: {
					for (; 0 < len; len--) { *pVRAM++ = 0b000; }
				} break;
				case 0b01: {
					for (; 0 < len; len--) { *pVRAM++ = 0b111; }
				} break;
				case 0b10: {
					for (; 0 < len; len--) { *pVRAM++ = 0b010; }
				} break;
				case 0b11: {
					for (; 0 < len; len--) { *pVRAM++ = 0b100; }
				} break;
				}
			}
			VRAMFlush(y);
		}
	} break;
	case 3: {
		for (int y = 0; y < 480; y++) {
			u8* pVRAM = vga.dmaBuffer->getLineAddr8(y, vga.backBuffer);
			const u8* pVRAMTerm = &pVRAM[640];
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
				} break;
				case 0b11: {
					*pVRAM++ = (Data >> 0) & 0b111;
					*pVRAM++ = (Data >> 3) & 0b111;
				} break;
				}
			}
			VRAMFlush(y);
		}
	} break;
	}

	Serial.println(String(FrameIndex)); // この行を消すと極端に遅くなる。最適化を阻害している？
}

