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

bool calc_checksum(uint8_t *p) {
  uint8_t ck_a = 0;
  uint8_t ck_b = 0;
  uint16_t payload_length = *(uint16_t *)&p[4];
  for (int i = 0; i < payload_length + 4; i++) {
    ck_a += p[2 + i];
    ck_b += ck_a;
  }
  return ((ck_a == p[6 + payload_length]) && (ck_b == p[7 + payload_length]));
}

uint32_t calc_crc24q(uint8_t *p, int len) {
  int i, j;
  uint32_t crc = 0;
  for (i = 0; i < len; i++) {
    crc ^= (uint32_t)p[i] << 16;
    for (j = 0; j < 8; j++)
      if ((crc <<= 1) & 0x1000000) crc ^= 0x1864cfb;  // CRC24Q
  }
  return crc;
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
      if (calc_checksum(ubx_frame)) {
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
          sec += 9 * 60 * 60;  // UTC -> JST
        }
        if (ubx_frame[26] > 1) {
          lon = *(int32_t *)&ubx_frame[30];
          lat = *(int32_t *)&ubx_frame[34];
        }
      }
    }

    if (memcmp(ubx_frame, ubx_rxm_sfrbx, sizeof(ubx_rxm_sfrbx)) == 0) {  // UBX-RXM-SFRBX, gnssId = 5(QZSS)
      if (calc_checksum(ubx_frame)) {
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
        dcr_message[sizeof(dcr_message) - 1] &= 0b11000000; // dcr_message is 250bits(8bits * 31 + 2bits)
        if (calc_crc24q(dcr_message, sizeof(dcr_message)) == 0) {
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
  }
}
