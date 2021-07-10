PINDIR := ${CURDIR}/third_party/PIN/
PIN_ROOT := $(PINDIR)pin
SRCDIR := ${CURDIR}/src/
LIBDIR := ${CURDIR}/lib/

.PHONY: all
all: tool | $(PIN_ROOT)

$(PIN_ROOT):
	echo "Extracting pin files..."
	tar -C $(PINDIR) -xvzf $(PINDIR)pin-3.17-98314-g0c048d619-gcc-linux.tar.gz
	mv $(PINDIR)pin-3.17-98314-g0c048d619-gcc-linux $(PINDIR)pin

.PHONY: tool
tool: lib | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT)

.PHONY: debug
debug: lib | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT) debug

.PHONY: lib
lib: 
	$(MAKE) -C $(LIBDIR)
