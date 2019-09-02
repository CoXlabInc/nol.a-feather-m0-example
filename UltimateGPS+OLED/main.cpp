#include <cox.h>
#include "UltimateGPS.hpp"
#include <nmea.h>
#include <nmea/gpgga.h>
#include <nmea/gprmc.h>
#include <nmea/gpgsv.h>
#include <dev/Adafruit_SSD1306.hpp>
#include <algorithm>

#define SEND_TO_LORAWAN

UltimateGps gps(A5, Serial2);
Adafruit_SSD1306 display(128, 32, &Wire);

uint8_t fixQuality = 0;
nmea_position longToReport, latToReport;

#ifdef SEND_TO_LORAWAN
#include <LoRaMacKR920.hpp>

SystemFeatherM0::RFM9x RFM95(A1, D10, D11);
LoRaMacKR920 LoRaWAN(RFM95, 3);

static uint8_t devEui[] = "\x98\x76\xB6\x00\x00\x10\xED\x7D";
static const uint8_t appEui[] = "\x00\x00\x00\x00\x00\x00\x00\x00";
static uint8_t appKey[] = "\x94\x62\xef\xdb\x8c\x62\x01\xe7\x52\x09\x8f\x17\x3c\x2a\x4b\x1c";

Timer timerSend;
struct timeval tLoRaWANDisplayed;

static void taskPeriodicSend(void *) {
  LoRaMacFrame *f = new LoRaMacFrame(255);
  if (!f) {
    printf("* Out of memory\n");
    return;
  }

  f->port = 1;
  f->type = LoRaMacFrame::CONFIRMED;

  if (fixQuality) {
    f->len = sprintf(
      (char *) f->buf,
      "\"gnss\":{\"latitude\":%lf,\"longitude\":%lf}",
      latToReport.degrees + latToReport.minutes / 60,
      longToReport.degrees + longToReport.minutes / 60
    );
  } else {
    f->len = sprintf((char *) f->buf, "\"gnss\":0");
  }

  /* Uncomment below line to specify frequency. */
  // f->freq = 922500000;

  /* Uncomment below lines to specify parameters manually. */
  // LoRaWAN.useADR = false;
  // f->modulation = Radio::MOD_LORA;
  // f->meta.LoRa.bw = Radio::BW_125kHz;
  // f->meta.LoRa.sf = Radio::SF7;
  // f->power = 1; /* Index 1 => MaxEIRP - 2 dBm */

  /* Uncomment below line to specify number of trials. */
  // f->numTrials = 1;

  error_t err = LoRaWAN.send(f);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Uplink ");
  display.print(f->len);
  display.print(' ');
  display.println("byte");
  display.print(" - Result:");
  display.print(err);
  display.display();
  gettimeofday(&tLoRaWANDisplayed, nullptr);

  Serial0.printf("* Sending periodic report (%s (%u byte)): %d\n", f->buf, f->len, err);
  if (err != ERROR_SUCCESS) {
    delete f;
    timerSend.startOneShot(10000);
    return;
  }

  err = LoRaWAN.requestDeviceTime();
  printf("* Request DeviceTime: %d\n", err);

  digitalWrite(D13, HIGH);
}
#endif //SEND_TO_LORAWAN

void setup() {
  Serial0.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("*** [Feather-M0] ***");
  display.println("Ultimate GPS Test");
  display.display();
  delay(1000);

  gps.begin();
  gps.turnOn();

  gps.onNMEAReceived = [](Gps&) {
    struct timeval tNow, tDiff;
    gettimeofday(&tNow, nullptr);
    timersub(&tNow, &tLoRaWANDisplayed, &tDiff);
    bool doDisplay = (tDiff.tv_sec >= 2);

    if (doDisplay) {
      display.clearDisplay();
      display.setCursor(0, 0);
    }

    char *data;
    nmea_s *parsed = nullptr;
    if (gps.getLatestGGA() != nullptr) {
      data = new char[strlen(gps.getLatestGGA()) + 1];
      Serial0.printf("- GGA\n");
      if (data) {
        strcpy(data, gps.getLatestGGA());
        parsed = nmea_parse(data, strlen(data), 0);
        if (parsed && parsed->type == NMEA_GPGGA) {
          nmea_gpgga_s *gga = (nmea_gpgga_s *) parsed;
          Serial0.printf("  Position fix indicator:%u\n", gga->position_fix);
          Serial0.printf("  Time: %02u:%02u:%02u\n", gga->time.tm_hour, gga->time.tm_min, gga->time.tm_sec);
          Serial0.printf("  Longitude: %d deg %lf %c\n", gga->longitude.degrees, gga->longitude.minutes, gga->longitude.cardinal);
          Serial0.printf("  Latitude: %d deg %lf %c\n", gga->latitude.degrees, gga->latitude.minutes, gga->latitude.cardinal);
          Serial0.printf("  Altitude: %d %c\n", gga->altitude, gga->altitude_unit);
          Serial0.printf("  Number of satellites: %d\n", gga->n_satellites);

          latToReport = gga->latitude;
          longToReport = gga->longitude;
          fixQuality = gga->position_fix;

          if (doDisplay) {
            if (gga->time.tm_hour < 10) display.print('0');
            display.print(gga->time.tm_hour);
            display.print(':');
            if (gga->time.tm_min < 10) display.print('0');
            display.print(gga->time.tm_min);
            display.print(':');
            if (gga->time.tm_sec < 10) display.print('0');
            display.print(gga->time.tm_sec);
            display.print("  ");
            int32_t vBat = map(analogRead(D9), 0, 4095, 0, 2 * 3300 * 100 / 148);
            display.print(vBat);
            display.println(" mV");


            if (gga->position_fix == 0) {
              display.println("Position not fixed.");
              display.print("Wait");
              static uint8_t dot_wait = 0;

              switch (dot_wait++ % 3) {
                case 0: display.print(".  "); break;
                case 1: display.print(" . "); break;
                default: display.print("  ."); break;
              }
            } else {
              display.print(gga->latitude.degrees);
              display.print(" deg ");
              display.print(gga->latitude.minutes, 4);
              display.print(' ');
              display.println(gga->latitude.cardinal);
              display.print(gga->longitude.degrees);
              display.print(" deg ");
              display.print(gga->longitude.minutes, 4);
              display.print(' ');
              display.println(gga->longitude.cardinal);
              display.print(gga->altitude);
              display.print(' ');
              display.println(gga->altitude_unit);
            }
            display.display();
          }
        } else {
          Serial0.printf("  Parsing GGA error (%p)\n", parsed);
        }
        delete data;
      } else {
        Serial0.printf("  No memory to parse GGA\n");
      }
    } else {
      Serial0.printf("- GGA: (null)\n");
    }

    if (parsed) {
      nmea_free(parsed);
    }

    for (uint8_t i = 0; i < 3; i++) {
      if (gps.getLatestGSV(i) != nullptr) {
        Serial0.printf("- GSV[%u]\n", i);
        data = new char[strlen(gps.getLatestGSV(i)) + 1];
        if (data) {
          strcpy(data, gps.getLatestGSV(i));
          parsed = nmea_parse(data, strlen(data), 0);
          if (parsed && parsed->type == NMEA_GPGSV) {
            nmea_gpgsv_s *gsv = (nmea_gpgsv_s *) parsed;
            Serial0.printf("  Sentence number:%u\n", gsv->sentence_number);
            Serial0.printf("  Number of satellites:%u\n", gsv->satellites);
            for (uint8_t i = 0; i < 4; i++) {
              Serial0.printf("  SAT[%u] ", i);
              if (gsv->sat[i].prn >= 0) {
                Serial0.printf(
                  "PRN:%d, elevation:%d, azimuth:%d, snr:%d\n",
                  gsv->sat[i].prn, gsv->sat[i].elevation, gsv->sat[i].azimuth, gsv->sat[i].snr
                );
              } else {
                Serial0.printf("(null)\n");
              }
            }
          } else {
            Serial0.printf("  Parsing GSV error (%p)\n", parsed);
          }
          delete data;
        } else {
          Serial0.printf("  No memory to parse GSV\n");
        }
      } else {
        Serial0.printf("- GSV[%u]: (null)\n", i);
      }

      if (parsed) {
        nmea_free(parsed);
      }
    }

    if (gps.getLatestRMC() != nullptr) {
      Serial0.printf("- RMC\n");
      data = new char[strlen(gps.getLatestRMC()) + 1];
      if (data) {
        strcpy(data, gps.getLatestRMC());
        parsed = nmea_parse(data, strlen(data), 0);
        if (parsed && parsed->type == NMEA_GPRMC) {
          nmea_gprmc_s *rmc = (nmea_gprmc_s *) parsed;
          Serial0.printf("  Status: %s\n", rmc->valid ? "valid" : "not valid");
          Serial0.printf(
            "  Date time: %u-%02u-%02u %02u:%02u:%02u\n",
            rmc->time.tm_year + 1900, rmc->time.tm_mon + 1, rmc->time.tm_mday,
            rmc->time.tm_hour, rmc->time.tm_min, rmc->time.tm_sec
          );
          Serial0.printf("  Longitude: %d deg %lf %c\n", rmc->longitude.degrees, rmc->longitude.minutes, rmc->longitude.cardinal);
          Serial0.printf("  Latitude: %d deg %lf %c\n", rmc->latitude.degrees, rmc->latitude.minutes, rmc->latitude.cardinal);
          Serial0.printf("  Speed: %lf\n", rmc->speed);
        } else {
          Serial0.printf("  Parsing RMC error (%p)\n", parsed);
        }
        delete data;
      } else {
        Serial0.printf("  No memory to parse RMC\n");
      }
    } else {
      Serial0.printf("- RMC: (null)\n");
    }

    if (parsed) {
      nmea_free(parsed);
    }

    Serial0.printf("GSA: %s\n", (gps.getLatestGSA()) ? gps.getLatestGSA() : "(null)");
  };

  pinMode(D5, INPUT_PULLUP);
  attachInterrupt(D5, []() {
    display.clearDisplay();
    display.setCursor(0, 0);
    if (gps.isOn()) {
      display.print("Turn off GPS !");
      gps.turnOff();
    } else {
      display.print("Turn on GPS !");
      gps.turnOn();
    }
    display.display();
  }, FALLING);

  pinMode(D13, OUTPUT);
  digitalWrite(D13, LOW);

  pinMode(D9, INPUT);

#ifdef SEND_TO_LORAWAN
  System.setTimeDiff(9 * 60);  // KST
  RFM95.antennaGain = -6; // for low-gain antennas

  LoRaWAN.begin([]() -> uint8_t {
    int32_t vBat = map(analogRead(D9), 0, 4095, 0, 2 * 3300 * 100 / 148);
    if (vBat < 0) {
      return 255; // not measured
    } else {
      return map(vBat, 3300, 4200, 1, 254); // measured battery level
    }
  });
  LoRaWAN.onSendDone([](LoRaMac &lw, LoRaMacFrame *frame) {
    digitalWrite(D13, LOW);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Uplink (");
    display.print(frame->fCnt);
    display.println(") done");
    display.print(" - Result:");
    display.print(frame->result);
    display.display();
    gettimeofday(&tLoRaWANDisplayed, nullptr);

    Serial0.printf(
      "* Send done(%d): destined for port:%u, fCnt:0x%lX, Freq:%lu Hz, "
      "Power:%d dBm, # of Tx:%u, ",
      frame->result,
      frame->port,
      frame->fCnt,
      frame->freq,
      frame->power,
      frame->numTrials
    );

    if (frame->modulation == Radio::MOD_LORA) {
      const char *strBW[] = {
        "Unknown", "125kHz", "250kHz", "500kHz", "Unexpected value"
      };
      if (frame->meta.LoRa.bw > 3) {
        frame->meta.LoRa.bw = (Radio::LoRaBW_t) 4;
      }
      Serial0.printf(
        "LoRa, SF:%u, BW:%s, ", frame->meta.LoRa.sf, strBW[frame->meta.LoRa.bw]
      );
    } else if (frame->modulation == Radio::MOD_FSK) {
      Serial0.printf("FSK, ");
    } else {
      Serial0.printf("Unkndown modulation, ");
    }
    if (frame->type == LoRaMacFrame::UNCONFIRMED) {
      Serial0.printf("UNCONFIRMED");
    } else if (frame->type == LoRaMacFrame::CONFIRMED) {
      Serial0.printf("CONFIRMED");
    } else if (frame->type == LoRaMacFrame::MULTICAST) {
      Serial0.printf("MULTICAST (error)");
    } else if (frame->type == LoRaMacFrame::PROPRIETARY) {
      Serial0.printf("PROPRIETARY");
    } else {
      Serial0.printf("unknown type");
    }
    Serial0.printf(" frame\n");

    for (uint8_t t = 0; t < frame->numTrials; t++) {
      const char *strTxResult[] = {
        "not started",
        "success",
        "no ack",
        "air busy",
        "Tx timeout",
      };
      Serial0.printf("- [%u] %s\n", t, strTxResult[std::min((int) frame->txResult[t], 4)]);
    }
    delete frame;
    timerSend.startOneShot(10000);
  });
  LoRaWAN.onJoin([](
    LoRaMac &lw,
    bool joined,
    const uint8_t *joinedDevEui,
    const uint8_t *joinedAppEui,
    const uint8_t *joinedAppKey,
    const uint8_t *joinedNwkSKey,
    const uint8_t *joinedAppSKey,
    uint32_t joinedDevAddr,
    const RadioPacket &,
    uint32_t airTime
  ) {
    Serial0.printf("* Tx time of JoinRequest: %lu usec.\n", airTime);

    if (joined) {
      Serial0.println("* Joining done!");
      timerSend.startOneShot(10000);
    } else {
      Serial0.println("* Joining failed. Retry to join.");
      lw.beginJoining(devEui, appEui, appKey);
    }
  });
  LoRaWAN.onDeviceTimeAnswered(
    [](
      LoRaMac &lw,
      bool success,
      uint32_t tSeconds,
      uint8_t tFracSeconds
    ) {
      if (success) {
        struct tm tLocal, tUtc;
        System.getDateTime(tLocal);
        System.getUTC(tUtc);
        printf(
          "* LoRaWAN DeviceTime answered: (%lu + %u/256) GPS epoch.\n"
          "- Adjusted local time: %u-%u-%u %02u:%02u:%02u\n"
          "- Adjusted UTC time: %u-%u-%u %02u:%02u:%02u\n",
          tSeconds, tFracSeconds,
          tLocal.tm_year + 1900, tLocal.tm_mon + 1, tLocal.tm_mday,
          tLocal.tm_hour, tLocal.tm_min, tLocal.tm_sec,
          tUtc.tm_year + 1900, tUtc.tm_mon + 1, tUtc.tm_mday,
          tUtc.tm_hour, tUtc.tm_min, tUtc.tm_sec
        );
      } else {
        printf("* LoRaWAN DeviceTime not answered\n");
      }
    },
    &System
  );

  LoRaWAN.setPublicNetwork(false);

  Serial0.printf(
    "* DevEUI: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
    devEui[0], devEui[1], devEui[2], devEui[3],
    devEui[4], devEui[5], devEui[6], devEui[7]
  );

  Serial0.printf(
    "* AppKey: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
    appKey[0], appKey[1], appKey[2], appKey[3],
    appKey[4], appKey[5], appKey[6], appKey[7],
    appKey[8], appKey[9], appKey[10], appKey[11],
    appKey[12], appKey[13], appKey[14], appKey[15]
  );

  Serial0.println("* Let's start join!");
  LoRaWAN.beginJoining(devEui, appEui, appKey);

  timerSend.onFired(taskPeriodicSend, nullptr);
#endif //SEND_TO_LORAWAN
}
