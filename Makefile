CC = gcc
CXX = g++
RESAMPLE_OBJS = resample.o

MODEM_PATH=pkg-sl-modem/modem

CFLAGS += -Wall -g -O -I. -I$(MODEM_PATH) -DCONFIG_DEBUG_MODEM -m32 -DDEBUG_LOG -DDEBUG_SAMPLES
CXXFLAGS += $(CFLAGS)

MODEM_OBJS := \
	$(MODEM_PATH)/modem.o \
	$(MODEM_PATH)/modem_datafile.o \
	$(MODEM_PATH)/modem_at.o \
	$(MODEM_PATH)/modem_timer.o \
	$(MODEM_PATH)/modem_pack.o \
	$(MODEM_PATH)/modem_ec.o \
	$(MODEM_PATH)/modem_comp.o \
	$(MODEM_PATH)/modem_param.o \
	$(MODEM_PATH)/modem_debug.o \
	$(MODEM_PATH)/homolog_data.o

DP_OBJS := $(MODEM_PATH)/dp_sinus.o $(MODEM_PATH)/dp_dummy.o
SYSDEP_OBJS := $(MODEM_PATH)/sysdep_common.o
ALL_COMPILED_OBJS := $(MODEM_PATH)/modem_cmdline.o $(MODEM_OBJS) $(DP_OBJS) \
	$(SYSDEP_OBJS) $(RESAMPLE_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.o: %.cc
	$(CC) $(CXXFLAGS) -c -o $@ $^

all: inbound_modem resample_test

inbound_modem: inbound_modem.o $(MODEM_PATH)/dsplibs.o $(ALL_COMPILED_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

resample_test: resample_test.o $(RESAMPLE_OBJS)
	$(CC) $(CXXFLAGS) -o $@ $^

clean:
	rm -f inbound_modem inbound_modem.o resample_test resample_test.o $(ALL_COMPILED_OBJS)
.PHONY:
	clean

