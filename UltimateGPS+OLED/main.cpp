#include <cox.h>
#include "UltimateGPS.hpp"
#include <nmea.h>
#include <nmea/gpgga.h>
#include <nmea/gprmc.h>
#include <nmea/gpgsv.h>
#include <dev/Adafruit_SSD1306.hpp>

UltimateGps gps(A5, Serial2);
Adafruit_SSD1306 display(128, 32, &Wire);

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
    display.clearDisplay();
    display.setCursor(0, 0);

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
          Serial0.printf("  Longitude: %d degee %lf %c\n", gga->longitude.degrees, gga->longitude.minutes, gga->longitude.cardinal);
          Serial0.printf("  Latitude: %d degee %lf %c\n", gga->latitude.degrees, gga->latitude.minutes, gga->latitude.cardinal);
          Serial0.printf("  Altitude: %d %c\n", gga->altitude, gga->altitude_unit);
          Serial0.printf("  Number of satellites: %d\n", gga->n_satellites);

          if (gga->time.tm_hour < 10) display.print('0');
          display.print(gga->time.tm_hour);
          display.print(':');
          if (gga->time.tm_min < 10) display.print('0');
          display.print(gga->time.tm_min);
          display.print(':');
          if (gga->time.tm_sec < 10) display.print('0');
          display.println(gga->time.tm_sec);

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
            display.print(gga->longitude.degrees);
            display.print(" deg ");
            display.println(gga->longitude.minutes, 4);
            display.print(gga->latitude.degrees);
            display.print(" deg ");
            display.println(gga->latitude.minutes, 4);
            display.print(gga->altitude);
            display.print(' ');
            display.println(gga->altitude_unit);
          }
          display.display();
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
          Serial0.printf("  Longitude: %d degree %lf %c\n", rmc->longitude.degrees, rmc->longitude.minutes, rmc->longitude.cardinal);
          Serial0.printf("  Latitude: %d degree %lf %c\n", rmc->latitude.degrees, rmc->latitude.minutes, rmc->latitude.cardinal);
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

  pinMode(D9, INPUT_PULLUP);
  attachInterrupt(D9, []() {
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
}
