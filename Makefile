ifeq ($(ARCH),arm)
	TOOLCHAIN=/Volumes/mazda-toolchain/x-tools/arm-cortexa9_neon-linux-musleabihf/arm-cortexa9_neon-linux-musleabihf
	LIBRARIES=/Volumes/mazda-toolchain/libraries/builds/usr/local

	CC=mazda-g++
	SYSROOT=--sysroot ${TOOLCHAIN}/sysroot
	INC=${LIBRARIES}/include
	LIB=${LIBRARIES}/lib
else
	CC=g++
	SYSROOT=
	INC=/usr/local/include
	LIB=/usr/local/lib
endif

tracker: Makefile tracker.cpp dbus.cpp gps.cpp dbus.hpp gps.hpp tracker.pb.cc tracker.pb.h
	$(CC) ${SYSROOT} -std=c++0x -o tracker -I${INC} -L${LIB} -ldbus-1 -lprotobuf tracker.cpp dbus.cpp gps.cpp tracker.pb.cc
