XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

CFLAGS += -Werror
CFLAGS += -DXC_WANT_COMPAT_DEVICEMODEL_API -g -O0

CFLAGS += $(CFLAGS_libxenctrl)
CFLAGS += $(CFLAGS_libxenguest)
CFLAGS += $(CFLAGS_libxenevtchn)
CFLAGS += $(CFLAGS_xeninclude)
CFLAGS += $(CFLAGS_libxendevicemodel)
CFLAGS += $(CFLAGS_libxenforeignmemory)

TARGETS-y := monitor
TARGETS := $(TARGETS-y)

.PHONY: all
all: build

.PHONY: build
build: $(TARGETS)

.PHONY: clean
clean:
	$(RM) *.o $(TARGETS) *~ $(DEPS_RM)

.PHONY: distclean
distclean: clean

monitor: monitor.o Makefile
	$(CC) -o $@ $< $(LDFLAGS) $(LDLIBS_libxenctrl) $(LDLIBS_libxenguest) $(LDLIBS_libxenevtchn) $(LDLIBS_libxendevicemodel) $(LDLIBS_libxenforeignmemory)

install uninstall:

-include $(DEPS_INCLUDE)
