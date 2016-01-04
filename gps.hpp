//
//  gps.hpp
//  tracker
//
//  Created by Ryan Pendleton on 1/1/16.
//  Copyright © 2016 Ryan Pendleton. All rights reserved.
//

#pragma once
#include "tracker.pb.h"

typedef void (*GPSFilterFunction)(const GPSDataPoint &dataPoint, void *userData);

char gps_checksum_line(char *line, size_t len);
void gps_initialize(void);
void gps_add_filter(GPSFilterFunction filter, void *userData);
void gps_join(void);
