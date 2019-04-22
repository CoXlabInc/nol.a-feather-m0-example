#include <cox.h>
#include "UltimateGPS.hpp"
#include <nmea.h>
#include <nmea/gpgga.h>
#include <nmea/gprmc.h>
#include <nmea/gpgsv.h>

UltimateGps gps(A5, Serial2);

void setup() {
  Serial0.begin(115200);
  gps.begin();
  gps.turnOn();

  gps.onNMEAReceived = [](Gps&) {
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
}
