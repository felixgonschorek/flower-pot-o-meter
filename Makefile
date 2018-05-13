#############################################################################
#
# Makefile for flower-pot-o-meter bridge on Raspberry Pi
#
# By:   Felix Gonschorek 
# Date: 2018-05-13
#
# Description:
# ------------
# makefile for bridge between MQTT and NRF24 on raspberry pi for
# flower-pot-o-meter project v2
# usage: run as root!
prefix := /usr/local

ARCH=armv6zk
ifeq "$(shell uname -m)" "armv7l"
ARCH=armv7-a
endif

# Detect the Raspberry Pi from cpuinfo
#Count the matches for BCM2708 or BCM2709 in cpuinfo
#RPI=$(shell cat /proc/cpuinfo | grep Hardware | grep -c BCM2708)
#ifneq "${RPI}" "1"
#RPI=$(shell cat /proc/cpuinfo | grep Hardware | grep -c BCM2709)
#endif
#
#ifeq "$(RPI)" "1"
# The recommended compiler flags for the Raspberry Pi
CCFLAGS=-Ofast -mfpu=vfp -mfloat-abi=hard -march=$(ARCH) -mtune=arm1176jzf-s -std=c++0x
#endif

# define all programs
PROGRAMS = fpom
SOURCES = ${PROGRAMS:=.cpp}

all: ${PROGRAMS}

${PROGRAMS}: ${SOURCES}
	g++ ${CCFLAGS} -Wall -I../ -lrf24-bcm -lrf24network -lrf24mesh -lmosquitto $@.cpp -o $@

clean:
	rm -rf $(PROGRAMS)
	
install: all
	test -d $(prefix) || mkdir $(prefix)
	test -d $(prefix)/bin || mkdir $(prefix)/bin
	for prog in $(PROGRAMS); do \
	  install -m 0755 $$prog $(prefix)/bin; \
	done
