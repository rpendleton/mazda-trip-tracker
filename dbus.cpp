//
//  dbus.cpp
//  drive-journal
//
//  Created by Ryan Pendleton on 1/1/16.
//  Copyright © 2016 Ryan Pendleton. All rights reserved.
//

#include "dbus.hpp"

#include <err.h>
#include <thread>

#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"

DBusConnection *service_bus;
static std::thread service_thread;

void dbus_initialize(void) {
	DBusError err;

	dbus_threads_init_default();
	dbus_error_init(&err);

	if (!(service_bus = dbus_connection_open(SERVICE_BUS_ADDRESS, &err))) {
		dbus_error_free(&err);
		errx(1, "failed to connect to service bus: %s: %s", err.name, err.message);
	}

	if (!dbus_bus_register(service_bus, &err)) {
		dbus_error_free(&err);
		errx(1, "failed to register with service bus: %s: %s", err.name, err.message);
	}
	
	dbus_error_free(&err);
	
	service_thread = std::thread([]() {
		// dispatch messages until disconnect
		while (dbus_connection_read_write_dispatch(service_bus, -1));
		dbus_connection_unref(service_bus);
	});

    service_thread.detach();
}

void dbus_join(void) {
	service_thread.join();
}

void dbus_disconnect(void) {
	
}
