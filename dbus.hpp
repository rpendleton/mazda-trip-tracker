//
//  dbus.hpp
//  drive-journal
//
//  Created by Ryan Pendleton on 1/1/16.
//  Copyright © 2016 Ryan Pendleton. All rights reserved.
//

#pragma once
#include <dbus/dbus.h>

extern DBusConnection *service_bus;

void dbus_initialize(void);
void dbus_join(void);
void dbus_disconnect(void);
