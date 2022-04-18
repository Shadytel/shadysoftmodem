CC = gcc
CXX = g++
RESAMPLE_OBJS = resample.o

MODEM_PATH=pkg-sl-modem/modem

CFLAGS += -Wall -g -O -I. -DCONFIG_DEBUG_MODEM -m32
CXXFLAGS += $(CFLAGS) -I$(MODEM_PATH)

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
ALL_OBJS := $(MODEM_PATH)/modem_cmdline.o $(MODEM_OBJS) $(DP_OBJS) \
	$(MODEM_PATH)/dsplibs.o $(SYSDEP_OBJS) $(RESAMPLE_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.o: %.cc
	$(CC) $(CXXFLAGS) -c -o $@ $^

inbound_modem: inbound_modem.o $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^