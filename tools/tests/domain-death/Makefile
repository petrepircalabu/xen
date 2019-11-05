XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

CFLAGS += -Werror
CFLAGS += -DXC_WANT_COMPAT_DEVICEMODEL_API

CFLAGS += $(CFLAGS_libxenctrl)
CFLAGS += $(CFLAGS_libxenguest)
CFLAGS += $(CFLAGS_libxenevtchn)
CFLAGS += $(CFLAGS_xeninclude)
CFLAGS += $(CFLAGS_libxenlight)
CFLAGS += $(CFLAGS_libxentoollog)

TARGETS-y := domain-death
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

domain-death: domain-death.o Makefile
	$(CC) -o $@ $< $(LDFLAGS) $(LDLIBS_libxenctrl) $(LDLIBS_libxenguest) $(LDLIBS_libxenevtchn) $(LDLIBS_libxenlight) $(LDLIBS_libxentoollog)

install uninstall:

-include $(DEPS_INCLUDE)
