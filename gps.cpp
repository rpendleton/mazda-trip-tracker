//
//  gps.cpp
//  tracker
//
//  Created by Ryan Pendleton on 1/1/16.
//  Copyright © 2016 Ryan Pendleton. All rights reserved.
//

#include "gps.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sys/time.h>
#include <math.h>

#include <thread>
#include <vector>
#include <utility>

#define GPS_DEVICE_PATH "/dev/ttymxc2"
#define CHAR_TO_INT(a) (a - '0')

static std::thread gps_thread;
static std::vector<std::pair<GPSFilterFunction, void *>> filters;

char gps_checksum_line(char *line, size_t len) {
	char checksum = 0;
	char *next = line;
	
	while (*(++next) != '\0') {
		checksum ^= *next;
	}
	
	return checksum;
}

static bool gps_dispatch(FILE *gps) {
	char *line = (char*)malloc(sizeof(char) * 1024);
	size_t len = 1024;
	size_t read;
	
	GPSDataPoint data;
	
	{ // parse RMC message
		while ((read = getline(&line, &len, gps))) {
			if (strstr(line, "$GPRMC,") == line || read == -1 ) {
				break;
			}
		}
		
		if (read <= 1 || read == -1) {
			free(line);
			return false;
		}
		
		char *orig = line;
		strsep(&line, ",");
		
		char *time = strsep(&line, ",");
		/*char *status = */strsep(&line, ",");
		char *latitudeStr = strsep(&line, ",");
		char *latitudeDir = strsep(&line, ",");
		char *longitudeStr = strsep(&line, ",");
		char *longitudeDir = strsep(&line, ",");
		char *speed = strsep(&line, ",");
		char *angle = strsep(&line, ",");
		char *date = strsep(&line, ",");
//		char *magneticVar = strsep(&line, ",");
//		char *magneticVarDir = strsep(&line, ",*");
		
		struct tm timestamp;
		timestamp.tm_hour = CHAR_TO_INT(time[0]) * 10 + CHAR_TO_INT(time[1]);
		timestamp.tm_min = CHAR_TO_INT(time[2]) * 10 + CHAR_TO_INT(time[3]);
		timestamp.tm_sec = CHAR_TO_INT(time[4]) * 10 + CHAR_TO_INT(time[5]);
		timestamp.tm_mday = CHAR_TO_INT(date[0]) * 10 + CHAR_TO_INT(date[1]);
		timestamp.tm_mon = CHAR_TO_INT(date[2]) * 10 + CHAR_TO_INT(date[3]) - 1;
		timestamp.tm_year = CHAR_TO_INT(date[4]) * 10 + CHAR_TO_INT(date[5]) + 2000 - 1900;
		timestamp.tm_isdst = false;
		
		time_t timeSeconds = timegm(&timestamp);
		double timeMicroseconds = atof(&time[6]) * (double)1e6;
		
		double latitude = CHAR_TO_INT(latitudeStr[0]) * 10 + CHAR_TO_INT(latitudeStr[1]) + atof(&latitudeStr[2]) / 60.0f;
		double longitude = CHAR_TO_INT(longitudeStr[0]) * 100 + CHAR_TO_INT(longitudeStr[1]) * 10 + CHAR_TO_INT(longitudeStr[2]) + atof(&longitudeStr[3]) / 60.0f;
		
		if (latitudeDir[0] == 'S') {
			latitude *= -1;
		}
		
		if (longitudeDir[0] == 'W') {
			longitude *= -1;
		}
		
		GPSDataPoint_Time *timeMsg = new GPSDataPoint_Time();
		timeMsg->set_seconds(timeSeconds);
		data.set_allocated_time(timeMsg);
		
		if (timeMicroseconds > 0) {
			timeMsg->set_microseconds(timeMicroseconds);
		}
		
		data.set_latitude(latitude);
		data.set_longitude(longitude);
		
		if (atof(speed) != 0) {
			data.set_speed(atof(speed));
		}
		
		if (atof(angle) != 0) {
			data.set_bearing(atof(angle));
		}
		
		line = orig;
	}
	
	{ // parse GGA message
		while ((read = getline(&line, &len, gps))) {
			if (strstr(line, "$GPGGA,") == line || read == -1 ) {
				break;
			}
		}
		
		if (read <= 1 || read == -1) {
			free(line);
			return false;
		}
		
		char *orig = line;
		strsep(&line, ",");
		
		/*char *time = */strsep(&line, ",");
		/*char *latitudeStr = */strsep(&line, ",");
		/*char *latitudeDir = */strsep(&line, ",");
		/*char *longitudeStr = */strsep(&line, ",");
		/*char *longitudeDir = */strsep(&line, ",");
		/*char *fixType = */strsep(&line, ",");
		char *numSatellites = strsep(&line, ",");
		/*char *hdop = */strsep(&line, ",");
		char *altitude = strsep(&line, ",");
//		char *altitudeUnits = strsep(&line, ",");
//		char *heightAboveWGS84 = strsep(&line, ",");
//		char *heightUnits = strsep(&line, ",");
//		char *timeSinceDGPSUpdate = strsep(&line, ",");
//		char *dgpsStationId = strsep(&line, ",*");
//		char *checksum = strsep(&line, ",*\n");
		
		data.set_satellites(atoi(numSatellites));
		data.set_altitude(atof(altitude));
		
		line = orig;
	}
	
	free(line);
	
	for (auto filter : filters) {
		filter.first(data, filter.second);
	}
	
	return true;
}

void gps_initialize(void) {
	FILE *gps;
	
	if (!(gps = fopen(GPS_DEVICE_PATH, "r"))) {
		errx(1, "failed to open GPS device: %s\n", strerror(errno));
	}
	
	gps_thread = std::thread([gps]() {
		while (gps_dispatch(gps));
		fprintf(stderr, "gps connection closed\n");
	});
}

void gps_add_filter(GPSFilterFunction filter, void *userData) {
	filters.push_back(std::pair<GPSFilterFunction, void*>(filter, userData));
}

void gps_join(void) {
	gps_thread.join();
}
