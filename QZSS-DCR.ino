#include <QZQSM.h>
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>

constexpr char filename[] = "/DCReport.txt";
constexpr uint8_t ubx_nav_pvt[] = { 0xb5, 0x62, 0x01, 0x07, 0x5c, 0x00 };
constexpr uint8_t ubx_rxm_sfrbx[] = { 0xb5, 0x62, 0x02, 0x13, 0x28, 0x00, 0x05 };
uint8_t ubx_frame[100];
uint8_t dcr_message[32];

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  SPI.begin(7, 8, 6, -1);
  SD.begin(-1, SPI, 40000000);

  M5.Display.clear();
  M5.Display.setFont(&fonts::efontJA_10);
  M5.Display.setCursor(0, 0);
  M5.Display.println("QZSS DCR receiver");

  Serial2.begin(115200, SERIAL_8N1, 5, -1);

  memset(ubx_frame, 0, sizeof(ubx_frame));
}

void loop() {
  static File txtFile;
  static QZQSM report;
  static time_t sec = 0;
  static int32_t lat = 0;
  static int32_t lon = 0;

  M5.update();

  if (M5.BtnA.wasClicked()) {
    if (!txtFile) {
      txtFile = SD.open(filename, FILE_APPEND);
      if (txtFile) {
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("File OPEN");
      }
    } else {
      txtFile.close();
      M5.Display.clear();
      M5.Display.setCursor(0, 0);
      M5.Display.println("File CLOSE");
    }
  }

  if (Serial2.available()) {
    uint8_t c = Serial2.read();

    memmove(&ubx_frame[0], &ubx_frame[1], sizeof(ubx_frame) - 1);
    ubx_frame[sizeof(ubx_frame) - 1] = c;

    if (memcmp(ubx_frame, ubx_nav_pvt, sizeof(ubx_nav_pvt)) == 0) {  // UBX-NAV-PVT
      if (txtFile) {
        for (int i = 0; i < sizeof(ubx_frame); i++) {
          char str[4];
          sprintf(str, "%02x,", ubx_frame[i]);
          txtFile.print(str);
        }
        txtFile.print("\n");
      }
      if ((ubx_frame[17] & 0b00000011) == 0b00000011) {  // ValidTime == 1 && ValidDate == 1
        struct tm tm;
        tm.tm_sec = ubx_frame[16];
        tm.tm_min = ubx_frame[15];
        tm.tm_hour = ubx_frame[14];
        tm.tm_mday = ubx_frame[13];
        tm.tm_mon = ubx_frame[12] - 1;
        tm.tm_year = *(uint16_t *)&ubx_frame[10] - 1900;
        sec = mktime(&tm);
        sec = sec + 9 * 60 * 60;  // UTC -> JST
      }
      if (ubx_frame[26] > 1) {
        lon = *(int32_t *)&ubx_frame[30];
        lat = *(int32_t *)&ubx_frame[34];
      }
    }

    if (memcmp(ubx_frame, ubx_rxm_sfrbx, sizeof(ubx_rxm_sfrbx)) == 0) {  // UBX-RXM-SFRBX, gnssId = 5(QZSS)
      if (txtFile) {
        for (int i = 0; i < sizeof(ubx_frame); i++) {
          char str[4];
          sprintf(str, "%02x,", ubx_frame[i]);
          txtFile.print(str);
        }
        txtFile.print("\n");
      }

      for (int i = 0; i < sizeof(dcr_message); i += 4) {  // Little Endian -> Big Endian
        *(uint32_t *)&dcr_message[i] = __builtin_bswap32(*(uint32_t *)&ubx_frame[14 + i]);
      }

      if (txtFile) {
        for (int i = 0; i < sizeof(dcr_message); i++) {
          char str[4];
          sprintf(str, "%02x,", dcr_message[i]);
          txtFile.print(str);
        }
        txtFile.print("\n");
      }

      if (((dcr_message[1] >> 2) == 43) || ((dcr_message[1] >> 2) == 44)) {  // Message Type 43 or 44
        struct tm tm;
        gmtime_r(&sec, &tm);
        report.SetYear(tm.tm_year + 1900);
        report.Decode(dcr_message);
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println(report.GetReport());
        if (txtFile) {
          txtFile.print(ctime(&sec));
          txtFile.print(lat);
          txtFile.print(",");
          txtFile.print(lon);
          txtFile.print("\n");
          txtFile.println(report.GetReport());
        }
      }
    }
  }
}
