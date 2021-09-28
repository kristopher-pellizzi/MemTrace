PINDIR := ${CURDIR}/third_party/PIN/
AFLDIR := ${CURDIR}/third_party/AFLPlusPlus/
PIN_ROOT := $(PINDIR)pin
SRCDIR := ${CURDIR}/src/
LIBDIR := ${CURDIR}/lib/

.PHONY: all
all: tool | $(PIN_ROOT)

$(PIN_ROOT):
	@if ! ([ -e "$(PINDIR)pin" ]); then \
		echo "Extracting pin files..."; \
		tar -C $(PINDIR) -xvzf $(PINDIR)pin-3.17-98314-g0c048d619-gcc-linux.tar.gz; \
		mv $(PINDIR)pin-3.17-98314-g0c048d619-gcc-linux $(PINDIR)pin; \
	else \
		echo "Intel PIN is ready"; \
	fi

.PHONY: tool
tool: lib fuzzer | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT)

.PHONY: debug
debug: lib fuzzer | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT) debug

.PHONY: tool_custom_malloc
tool_custom_malloc: lib fuzzer | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT) ALLOCATOR_DEF=-DCUSTOM_ALLOCATOR

.PHONY: debug_custom_malloc
debug_custom_malloc: lib fuzzer | $(PIN_ROOT)
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT) ALLOCATOR_DEF=-DCUSTOM_ALLOCATOR debug

.PHONY: lib
lib: 
	$(MAKE) -C $(LIBDIR)

.PHONY: fuzzer
fuzzer: fuzzer_repo
	@if ! ([ -e "$(AFLDIR)afl-qemu-trace" ] && [ -e "$(AFLDIR)afl-fuzz" ]); then \
		$(MAKE) -C $(AFLDIR); \
		$(MAKE) -C $(AFLDIR) binary-only; \
	else \
		echo "Fuzzer already compiled"; \
	fi

.PHONY: fuzzer_repo
fuzzer_repo: 
ifeq (0, $(shell ls $(AFLDIR) | wc -l))
	git submodule init
	git submodule update
endif

.PHONY: nulltool
nulltool:
	$(MAKE) -C $(SRCDIR) PIN_ROOT=$(PIN_ROOT) nulltool