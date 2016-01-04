# Mazda Trip Tracker

Mazda Trip Tracker is a tweak for the Mazda Connect system that allows tracking
various trip statistics alongside full GPS logs. After installing the tweak,
trips will be logged to the navigation SD card, and can be analyzed later using
a computer.

The following data is recorded once every second:
- GPS information
  - time (epoch timestamp)
  - latitude (decimal)
  - longitude (decimal)
  - altitude (meters)
  - number of satellites (integer)
- number of odometer ticks (~20 cm each) since last data point
- total car odometer value (km)

## File Format

Each trip is stored as a single file consisting of data points. As data points
are collected every second, they are encoded using protocol buffers, prefixed
with their length, and then appended to the trip journal file.

The size of data points will vary since they are encoded using protocol buffers,
but you can expect them to be around 45 to 81 bytes each. This means that as a
worst case scenario, it would only take 285 KiB to store an hour of driving. To
put this number into perspective, you could drive 24/7 for an entire year and
store the trip using only 2.38 GiB of storage.

When using a more reasonable estimate of 63 bytes per data point and only two
hours of driving every day, you would only need 0.15 GiB of storage for the
entire year.

## Example Statistics

### Summaries

When invoked with a filename (but no other arguments), a trip summary will be
displayed:

```
$ trip-tracker ~/2016-1-4T7-0-33
Trip Summary:
    Trip Start:  1451890833
    Trip End:    1451891817
    Trip Length: 0:16:24

    Driving Time: 860 seconds (87.49%)
    Idling Time:  123 seconds (12.51%)

    Total Ticks:    63290
    Total Distance: 12.66 km (7.87 miles)

    Max Speed:        64.10 knots (73.76 mph)
    Avg Total Speed:  25.16 knots (28.95 mph)
    Avg Moving Speed: 28.76 knots (33.09 mph)

    Min Altitude: 1376.29 meters
    Max Altitude: 1415.53 meters
    Max Altitude: 1391.45 meters
```

### NMEA data

You can also export the GPS data as NMEA data, suitable for use with viewers
such as [FreeNMEA.net](freenmea.net/decoder):

```
$ trip-tracker ~/2016-1-4T7-0-33 nmea
$GPRMC,070033,A,...
$GPRMC,070034,A,...
$GPRMC,070035,A,...
$GPRMC,070036,A,...
$GPRMC,070037,A,...
...
```

### Protocol Buffer Objects

For debugging purposes, it is also possible to generate a description for each
data entry. The description is created using the description method from
protocol buffers, and will contain all of the information recorded.

```
$ trip-tracker ~/2016-1-4T7-0-33 expand
location {
  time {
    seconds: 1451890833
  }
  latitude: ...
  longitude: ...
  altitude: 1405.02
  speed: 0.1
  satellites: 8
}
ticks: 0
odometer: 2869

...

location {
  time {
    seconds: 1451890883
  }
  latitude: ...
  longitude: ...
  altitude: 1404.25
  speed: 16.4
  bearing: 359
  satellites: 8
}
ticks: 42
odometer: 2869

...

location {
  time {
    seconds: 1451890893
  }
  latitude: ...
  longitude: ...
  altitude: 1404.29
  speed: 26.1
  bearing: 359.2
  satellites: 9
}
ticks: 67
odometer: 2869

...
```
