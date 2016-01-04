//
//  main.c
//  tracker
//
//  Created by Ryan Pendleton on 1/1/16.
//  Copyright © 2016 Ryan Pendleton. All rights reserved.
//

#include <stdio.h>
#include <err.h>
#include <mutex>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

#include "dbus.hpp"
#include "gps.hpp"

using namespace google::protobuf::io;

struct OdometerContext {
	bool recording;
	uint8_t lastTick;
	uint32_t ticks;
	uint32_t odometer;
	std::mutex mutex;
	std::ofstream *output;
};

static inline bool check_dbus_iter_type(DBusMessageIter *iter, int expectedType, const char *msg) {
	int actualType = dbus_message_iter_get_arg_type(iter);
	
	if (actualType == expectedType) {
		return true;
	}
	else {
		fprintf(stderr, "received unexpected type for %s: %c\n", msg, actualType);
		return false;
	}
}

static DBusHandlerResult handle_dbus_odocount(DBusConnection *conn, DBusMessage *msg, void *ptr) {
	struct OdometerContext *ctx = (struct OdometerContext *)ptr;
	
	if (strcmp(dbus_message_get_interface(msg), "com.jci.vbs.vdt") == 0 && strcmp(dbus_message_get_member(msg), "OdoCount") == 0) {
		DBusMessageIter iter;
		dbus_message_iter_init(msg, &iter);
		
		if (check_dbus_iter_type(&iter, DBUS_TYPE_BYTE, "OdoCount")) {
			uint8_t value;
			dbus_message_iter_get_basic(&iter, &value);
			printf("OdoCount = %i\n", value);
			
			ctx->mutex.lock();
			
			uint8_t diff = value - ctx->lastTick;
			ctx->lastTick = value;
			ctx->ticks += diff;
			
			ctx->mutex.unlock();
			
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}
	
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult handle_dbus_distance(DBusConnection *conn, DBusMessage *msg, void *ptr) {
	struct OdometerContext *ctx = (struct OdometerContext *)ptr;
	
	if (strcmp(dbus_message_get_interface(msg), "com.jci.vbs.vdt") == 0 && strcmp(dbus_message_get_member(msg), "Total_Distance") == 0) {
		DBusMessageIter iter;
		dbus_message_iter_init(msg, &iter);
		
		if (check_dbus_iter_type(&iter, DBUS_TYPE_UINT32, "Total_Distance")) {
			uint32_t value;
			dbus_message_iter_get_basic(&iter, &value);
			printf("Total_Distance = %i\n", value);
			
			ctx->mutex.lock();
			ctx->odometer = value;
			ctx->mutex.unlock();
			
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}
	
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult handle_dbus_ignition(DBusConnection *conn, DBusMessage *msg, void *ptr) {
	struct OdometerContext *ctx = (struct OdometerContext *)ptr;
	
	if (strcmp(dbus_message_get_interface(msg), "com.jci.vbs.vwm") == 0 && strcmp(dbus_message_get_member(msg), "Ignition_Status") == 0) {
		DBusMessageIter iter;
		dbus_message_iter_init(msg, &iter);
		
		if (check_dbus_iter_type(&iter, DBUS_TYPE_INT16, "Ignition_Status")) {
			uint16_t value;
			dbus_message_iter_get_basic(&iter, &value);
			printf("Ignition_Status = %i\n", value);
			
			ctx->mutex.lock();
			
			if (value == 5) {
				ctx->recording = true;
			}
			else if (value == 1) {
				ctx->recording = false;
				ctx->lastTick = 0;
			}
			
			ctx->mutex.unlock();
			
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}
	
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void handle_gps_datapoint(const GPSDataPoint &data, void *ptr) {
	struct OdometerContext *ctx = (struct OdometerContext *)ptr;
	
	bool recording;
	uint32_t ticks;
	uint32_t odometer;
	
	ctx->mutex.lock();
	
	recording = ctx->recording;
	ticks = ctx->ticks;
	odometer = ctx->odometer;
	
	ctx->ticks = 0;
	
	ctx->mutex.unlock();
	
	if (recording) {
		printf("time = %lli\nticks = %u\n", data.time().seconds(), ticks);
		
		if (!ctx->output) {
			while (true) {
				struct stat s;
				
				if (stat("/mnt/sd_nav/mods", &s) == 0) {
					break;
				}
			}
			
			time_t time = data.time().seconds();
			struct tm *timeComponents = gmtime(&time);
			char *filename;
			asprintf(&filename, "/mnt/sd_nav/mods/trips/%i-%i-%iT%i-%i-%i",
					 timeComponents->tm_year + 1900,
					 timeComponents->tm_mon + 1,
					 timeComponents->tm_mday,
					 timeComponents->tm_hour,
					 timeComponents->tm_min,
					 timeComponents->tm_sec);
			
			ctx->output = new std::ofstream(filename);
			free(filename);
		}
		
		GPSDataPoint *dataCopy = new GPSDataPoint(data);
		
		TravelDataPoint travelData;
		travelData.set_allocated_location(dataCopy);
		travelData.set_ticks(ticks);
		travelData.set_odometer(odometer);
		
		OstreamOutputStream output(ctx->output);
		CodedOutputStream coded(&output);
		
		coded.WriteVarint32(travelData.ByteSize());
		travelData.SerializeToCodedStream(&coded);
		
		ctx->output->flush();
	}
	else if (ctx->output) {
		ctx->output->flush();
		ctx->output->close();
		delete ctx->output;
		ctx->output = nullptr;
	}
}

static void _dbus_register(struct OdometerContext *ctx) {
	DBusError err;
	dbus_error_init(&err);
	
	// add OdoCount filter
	dbus_connection_add_filter(service_bus, handle_dbus_odocount, ctx, nullptr);
	
	// add Total Distance filter
	dbus_connection_add_filter(service_bus, handle_dbus_distance, ctx, nullptr);

	// add Ignition Status filter
	dbus_connection_add_filter(service_bus, handle_dbus_ignition, ctx, nullptr);
	
	// add OdoCount match
	dbus_bus_add_match(service_bus, "type='signal',interface='com.jci.vbs.vdt',member='OdoCount'", &err);
	if (dbus_error_is_set(&err)) {
		errx(1, "failed to add OdoCount match: %s: %s\n", err.name, err.message);
	}
	
	// add TotalDistance match
	dbus_bus_add_match(service_bus, "type='signal',interface='com.jci.vbs.vdt',member='Total_Distance'", &err);
	if (dbus_error_is_set(&err)) {
		errx(1, "failed to add Total_Distance match: %s: %s\n", err.name, err.message);
	}
	
	// add Ignition match
	dbus_bus_add_match(service_bus, "type='signal',interface='com.jci.vbs.vwm',member='Ignition_Status'", &err);
	if (dbus_error_is_set(&err)) {
		errx(1, "failed to add Ignition_Status match: %s: %s\n", err.name, err.message);
	}
}

static void _gps_register(struct OdometerContext *ctx) {
	gps_add_filter(handle_gps_datapoint, ctx);
}

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		std::ifstream input(argv[1]);
		IstreamInputStream stream(&input);
		CodedInputStream coded(&stream);
		
		uint32_t size;
		
		enum {
			SUMMARY,
			EXPAND,
			NMEA
		} action = SUMMARY;
		
		if (argc > 2) {
			if (strcmp(argv[2], "expand") == 0) {
				action = EXPAND;
			}
			if (strcmp(argv[2], "nmea") == 0) {
				action = NMEA;
			}
			else {
				errx(1, "unknown action: %s", argv[2]);
			}
		}
		
		int firstTimestamp = 0;
		int lastTimestamp = 0;

		double maxSpeed = 0;
		double totalSpeed = 0;
		
		double minAltitude = 1e10;
		double maxAltitude = 0;
		double totalAltitude = 0;

		int totalTicks = 0;
		int firstOdometer = 0;
		int lastOdometer = 0;
		
		int idlingTime = 0;
		int drivingTime = 0;
		
		int count = 0;
		int speedCount = 0;
		int driveSpeedCount = 0;
		
		while (!coded.ConsumedEntireMessage() && coded.ReadVarint32(&size)) {
			int oldLimit = coded.PushLimit(size);
			
			TravelDataPoint dataPoint;
			
			if(dataPoint.ParseFromCodedStream(&coded) && coded.BytesUntilLimit() == 0) {
				if (action == EXPAND) {
					printf("%s\n", dataPoint.DebugString().c_str());
				}
				else if (action == NMEA) {
					time_t timestamp = dataPoint.location().time().seconds();
					struct tm *tm = gmtime(&timestamp);
					
					int latWhole = dataPoint.location().latitude();
					double latFrac = (dataPoint.location().latitude() - (double)latWhole) * 60.0;
					char latDir;
					
					if (latWhole > 0) {
						latDir = 'N';
					}
					else {
						latDir = 'S';
						latWhole *= -1;
						latFrac *= -1;
					}
					
					int longWhole = dataPoint.location().longitude();
					double longFrac = (dataPoint.location().longitude() - (double)longWhole) * 60.0;
					char longDir;
					
					if (longWhole > 0) {
						longDir = 'E';
					}
					else {
						longDir = 'W';
						longWhole *= -1;
						longFrac *= -1;
					}
					
					char *record;
					int len = asprintf(&record, "$GPRMC,%02i%02i%02i,A,%02i%f,%c,%02i%f,%c,%f,%f,%02i%02i%02i,,",
									   tm->tm_hour, tm->tm_min, tm->tm_sec,
									   latWhole, latFrac, latDir,
									   longWhole, longFrac, longDir,
									   dataPoint.location().speed(),
									   dataPoint.location().bearing(),
									   tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900-2000
									   );
					
					printf("%s*%02x\n", record, gps_checksum_line(record, len));
					free(record);
				}
				else if (action == SUMMARY) {
					double time = dataPoint.location().time().seconds();
					double altitude = dataPoint.location().altitude();
					double speed = dataPoint.location().speed();
					double ticks = dataPoint.ticks();
					double odometer = dataPoint.odometer();
					
					count++;
					
					if (firstTimestamp == 0) {
						firstTimestamp = time;
					}
					
					lastTimestamp = time;
					
					if (speed > maxSpeed && speed < 500) {
						maxSpeed = speed;
					}
					
					if (speed < 500) {
						totalSpeed += speed;
						speedCount++;
						
						if (ticks > 0) {
							driveSpeedCount++;
						}
					}
					
					totalAltitude += altitude;
					
					if (altitude > maxAltitude) {
						maxAltitude = altitude;
					}
					
					if (altitude < minAltitude) {
						minAltitude = altitude;
					}
					
					totalTicks += ticks;
					
					if (firstOdometer == 0 && odometer != 0) {
						firstOdometer = odometer;
					}
					
					lastOdometer = odometer;
					
					if (ticks == 0) {
						idlingTime++;
					}
					else {
						drivingTime++;
					}
				}
			}
			else {
				errx(1, "failed to parse input file");
			}
			
			coded.PopLimit(oldLimit);
		}
		
		if (action == SUMMARY) {
			int diff = lastTimestamp - firstTimestamp;
			int hours = diff / 3600;
			int minutes = (diff % 3600) / 60;
			int seconds = (diff % 60);
			
			printf("Trip Summary:\n");
			printf("    Trip Start:  %i\n", firstTimestamp);
			printf("    Trip End:    %i\n", lastTimestamp);
			printf("    Trip Length: %i:%i:%i\n\n", hours, minutes, seconds);
			
			printf("    Driving Time: %i seconds (%0.2f%%)\n", drivingTime, (double)drivingTime / (double)(idlingTime + drivingTime) * 100.0);
			printf("    Idling Time:  %i seconds (%0.2f%%)\n\n", idlingTime, (double)idlingTime / (double)(idlingTime + drivingTime) * 100.0);
			
			printf("    Total Ticks:    %i\n", totalTicks);
			printf("    Total Distance: %0.2f km (%0.2f miles)\n\n", totalTicks * 0.0002, totalTicks * 0.000124274);
			
			printf("    Max Speed:        %0.2f knots (%0.2f mph)\n", maxSpeed, maxSpeed * 1.15078);
			printf("    Avg Total Speed:  %0.2f knots (%0.2f mph)\n", totalSpeed / (double)speedCount, (totalSpeed / (double)speedCount) * 1.15078);
			printf("    Avg Moving Speed: %0.2f knots (%0.2f mph)\n\n", totalSpeed / (double)driveSpeedCount, (totalSpeed / (double)driveSpeedCount) * 1.15078);
			
			printf("    Min Altitude: %0.2f meters\n", minAltitude);
			printf("    Max Altitude: %0.2f meters\n", maxAltitude);
			printf("    Max Altitude: %0.2f meters\n", totalAltitude / (double)count);
			
		}
	}
	else {
		struct OdometerContext ctx;
		ctx.recording = true;
		ctx.ticks = 0;
		ctx.odometer = 0;
		ctx.output = nullptr;
		
		dbus_initialize();
		_dbus_register(&ctx);
		
		gps_initialize();
		_gps_register(&ctx);
		
		gps_join();
		dbus_disconnect();
		
		ctx.output->close();
		delete ctx.output;
		
		return 0;
	}
}
