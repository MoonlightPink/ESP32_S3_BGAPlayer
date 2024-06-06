
// 再利用可搬性を全く考慮していない汚いソースコードです。
// This is messy source code that does not take reuse or portability into consideration at all.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

#include <SD.h>
#include "isd_diskio.h"

static u8 pdrv = 0xff; // SD card handler

#include "ESP32_S3_LED.h"

#include "LuniVGA_VGA.h"

//                   r,r,r,r,r,  g,g, g, g, g, g,   b, b, b, b, b,   h,v
static const PinConfig pins(4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 1, 2);

//3 bit version (no resistor ladder)
//static const PinConfig pins(-1, -1, -1, -1, 8, -1, -1, -1, -1, -1, 14, -1, -1, -1, -1, 21, 1, 2);

static VGA vga;

static const Mode VGAMode = Mode::MODE_640x480x60;
static const int VGABits = 8;

static void VRAMFlush(int y) { vga.dmaBuffer->flush(vga.backBuffer, y); }

#include "Console.h"

// 黒:GND, 赤:3.3V
static const u8 SD_CS = 38; // 黄
static const u8 SD_SCK = 39; // 白
static const u8 SD_MISO = 40; // 緑 DO
static const u8 SD_MOSI = 41; // 青 DI

#include "BGA.h"

#define AppTitle "ESP32_S3_BGAPlayer"

static void SerialPortInit() {
	Serial.begin(115200);
	Serial.print("Boot ");
	Serial.println(AppTitle);
}

static void DrawColorBar() {
	for (int y = 0; y < VGAMode.vRes; y++) {
		for (int x = 0; x < VGAMode.hRes; x++) {
			vga.dotdit(x, y, x, y, 255 - x);
		}
	}

	for (int y = 0; y < 30; y++) {
		for (int x = 0; x < 256; x++) {
			vga.dotdit(x, y, x, 0, 0);
			vga.dotdit(x, y + 30, 0, x, 0);
			vga.dotdit(x, y + 60, 0, 0, x);
		}
	}

	vga.show();
}

static void ClearVRAM() {
	for (int y = 0; y < VGAMode.vRes; y++) {
		for (int x = 0; x < VGAMode.hRes; x++) {
			vga.dot(x, y, 0, 0, 0);
		}
	}

	vga.show();
}

static void ShowChipInfos() {
	char buf[64];

	uint64_t chipid;
	chipid = ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	snprintf(buf, 64, "ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32));//print High 2 bytes
	ConsoleWriteLine(buf);

	snprintf(buf, 64, "Chip Revision %d", ESP.getChipRevision());
	ConsoleWriteLine(buf);
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	snprintf(buf, 64, "Number of Core: %d", chip_info.cores);
	ConsoleWriteLine(buf);
	snprintf(buf, 64, "CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
	ConsoleWriteLine(buf);
	snprintf(buf, 64, "Flash Chip Size = %d byte", ESP.getFlashChipSize());
	ConsoleWriteLine(buf);
	snprintf(buf, 64, "Flash Frequency = %d Hz", ESP.getFlashChipSpeed());
	ConsoleWriteLine(buf);
	snprintf(buf, 64, "ESP-IDF version = %s", esp_get_idf_version());
	ConsoleWriteLine(buf);
	//利用可能なヒープのサイズを取得
	snprintf(buf, 64, "Available Heap Size= %d", esp_get_free_heap_size());
	ConsoleWriteLine(buf);
	//利用可能な内部ヒープのサイズを取得
	snprintf(buf, 64, "Available Internal Heap Size = %d", esp_get_free_internal_heap_size());
	ConsoleWriteLine(buf);
	//これまでに利用可能だった最小ヒープを取得します
	snprintf(buf, 64, "Min Free Heap Ever Avg Size = %d", esp_get_minimum_free_heap_size());
	ConsoleWriteLine(buf);

	return;

	uint8_t mac0[6];
	esp_efuse_mac_get_default(mac0);
	snprintf(buf, 64, "Default Mac Address = %02X:%02X:%02X:%02X:%02X:%02X", mac0[0], mac0[1], mac0[2], mac0[3], mac0[4], mac0[5]);
	ConsoleWriteLine(buf);

	uint8_t mac3[6];
	esp_read_mac(mac3, ESP_MAC_WIFI_STA);
	snprintf(buf, 64, "[Wi-Fi Station] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X", mac3[0], mac3[1], mac3[2], mac3[3], mac3[4], mac3[5]);
	ConsoleWriteLine(buf);

	uint8_t mac4[7];
	esp_read_mac(mac4, ESP_MAC_WIFI_SOFTAP);
	snprintf(buf, 64, "[Wi-Fi SoftAP] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X", mac4[0], mac4[1], mac4[2], mac4[3], mac4[4], mac4[5]);
	ConsoleWriteLine(buf);

	uint8_t mac5[6];
	esp_read_mac(mac5, ESP_MAC_BT);
	snprintf(buf, 64, "[Bluetooth] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X", mac5[0], mac5[1], mac5[2], mac5[3], mac5[4], mac5[5]);
	ConsoleWriteLine(buf);

	uint8_t mac6[6];
	esp_read_mac(mac6, ESP_MAC_ETH);
	snprintf(buf, 64, "[Ethernet] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X", mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);
	ConsoleWriteLine(buf);
}

void setup() {
	//delay(3000);

	SerialPortInit();

	ESP32_S3_LED_Init();

	if (!vga.init(pins, VGAMode, VGABits)) {
		Serial.println("VGA init error.");
		while (1) { delay(1); }
	}
	vga.start();

	DrawColorBar();
	delay(3000);

	ConsoleSetFontHeight(32);
	ConsoleWriteLine("Boot " AppTitle);
	ConsoleWriteLine("VGA output: " + String(VGAMode.hRes) + "x" + String(VGAMode.vRes) + "pixels, 60Hz, " + String(vga.bits) + "bpp.");

	ConsoleSetFontHeight(24);
	ConsoleWriteLine("-----------------------------------------------");

	ShowChipInfos();

	ESP32_S3_LED_SetRGB(0, 0x40, 0);
	ConsoleSetFontHeight(16); ConsoleWriteLine("Wait 3secs."); delay(3000);

	ConsoleSetFontHeight(24);
	ConsoleWriteLine("-----------------------------------------------");

	ConsoleWriteLine("Open SD card.");
	SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
	pdrv = sdcard_init(SD_CS, &SPI, 40 * 1000 * 1000); // 規格上の最大周波数は24MHzらしい。40MHzで通信可能かはSDカードとの相性による
	ConsoleWriteLine("pdrv: $" + String(pdrv, HEX));
	if (pdrv == 0xff) {
		ConsoleWriteLine("Failed.");
		while (true) { delay(1); }
	}

	ConsoleWriteLine("ff_sd_initialize.");
	u8 sdinitres = ff_sd_initialize(pdrv);
	if (sdinitres != 0x00) {
		ConsoleWriteLine("Failed. $" + String(sdinitres, HEX));
		while (true) { delay(1); }
	}

	ConsoleWriteLine("Read test. (Sector 1)");
	u8 buf[512];
	if (!sdReadSectors(pdrv, (char*)buf, 1, 1)) {
		ConsoleWriteLine("Failed.");
		while (true) { delay(1); }
	}
	ConsoleSetFontHeight(8);
	for (int y = 0; y < 16; y++) {
		String Line = "$" + String(y * 16, HEX) + " ";
		for (int x = 0; x < 16; x++) {
			Line += String(buf[y * 16 + x], HEX) + " ";
		}
		ConsoleWriteLine(Line);
	}
	ConsoleSetFontHeight(24);

	ESP32_S3_LED_SetRGB(0x40, 0, 0);
	ConsoleSetFontHeight(16); ConsoleWriteLine("Wait 3secs."); delay(3000);

	ConsoleSetFontHeight(24);
	ConsoleWriteLine("-----------------------------------------------");

	BGA_Open();

	ESP32_S3_LED_SetRGB(0, 0, 0x40);
	ConsoleSetFontHeight(16); ConsoleWriteLine("Wait 3secs."); delay(3000);

	ESP32_S3_LED_SetBlack();
	ClearVRAM();
}

void loop() {
	static u32 StartMillis = 0;
	if (StartMillis == 0) { StartMillis = millis(); }

	if (BGA_FrameIndex < BGA_FramesCount) {
		const int NextMillis = BGA_FrameIndex * 1000 / 60;
		const int DelayMillis = (millis() - StartMillis) - NextMillis;

		if (false) {
			int LED = DelayMillis * 0x100 / 1000; // 1秒遅れると最大輝度
			if (0xff < LED) { LED = 0xff; }
			ESP32_S3_LED_SetRGB(LED, LED, LED);
		}

		if (16 <= DelayMillis) {
			Serial.println(String(BGA_FrameIndex) + " Delay: " + String(DelayMillis) + "ms.");
		}

		if (0 <= DelayMillis) {
			BGA_DrawFrame_Asm(BGA_FrameIndex);
			BGA_FrameIndex++;

			if (BGA_FrameIndex == BGA_FramesCount) {
				BGA_Close();

				ConsoleSetLine(VGAMode.vRes - ((24 * 2) + (32 * 2) + 8));

				ConsoleSetFontHeight(24);
				ConsoleWriteLine("End of video file.");
				ConsoleWriteLine("");

				ConsoleSetFontHeight(32);
				ConsoleWriteLine("Thank you for watching the stream!");
				ConsoleWriteLine("");

				while (1) { delay(1); }
			}
			return;
		}
	}

	delay(1);
}
