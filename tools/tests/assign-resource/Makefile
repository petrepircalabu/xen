XEN_ROOT=$(CURDIR)/../../..
include $(XEN_ROOT)/tools/Rules.mk

CFLAGS += -Werror
CFLAGS += -DXC_WANT_COMPAT_DEVICEMODEL_API

CFLAGS += $(CFLAGS_libxenctrl)
CFLAGS += $(CFLAGS_xeninclude)

TARGETS-y := assign-resource
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

assign-resource: assign-resource.o Makefile
	$(CC) -o $@ $< $(LDFLAGS) $(LDLIBS_libxenctrl) #$(LDLIBS_libxencall)

install uninstall:

-include $(DEPS_INCLUDE)
