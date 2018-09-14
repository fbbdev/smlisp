default: none

.PHONY: test clean

CSTD          = c99
CC            = gcc
LD            = gcc
CFLAGS        = -std=$(CSTD) -Wall -Wextra -pedantic -Werror -I$(INCLUDEDIR)
LDFLAGS       =
CFLAGS_TEST   = -fsanitize=address -fsanitize=leak -fsanitize=undefined
LDFLAGS_TEST  =
DEBUGFLAGS    = -g -O0
RELEASEFLAGS  = -DNDEBUG -flto -O3

ifdef RELEASE
	CFLAGS  += $(RELEASEFLAGS)
	LDFLAGS += $(RELEASEFLAGS)
else
	CFLAGS  += $(DEBUGFLAGS)
	LDFLAGS += $(DEBUGFLAGS)
endif

MD=mkdir -p
RM=rm -rf

BUILDDIR   = build
TESTDIR    = $(BUILDDIR)/tests
DIRS       = $(BUILDDIR) $(TESTDIR)

TESTLOG    = $(BUILDDIR)/test.log

INCLUDEDIR = include
SRCDIR     = src

$(DIRS):
	$(MD) $@

$(TESTDIR)/% : $(SRCDIR)/%_test.c $(SRCDIR)/%.c $(SRCDIR)/util.c | $(DIRS)
	$(CC) $(CFLAGS) $(CFLAGS_TEST) $(LDFLAGS) -o $@ $^

test: $(patsubst %_test.c,$(TESTDIR)/%,$(notdir $(wildcard $(SRCDIR)/*_test.c)))
	@echo Starting test suite
	@$(RM) $(TESTLOG)
	@for test in $^; do \
		echo -ne "Testing $$(basename "$$test")...\r"; \
		"$$test" 2>&1 | tee $(TESTLOG) | grep "PANIC\|FAIL\|tests passed"; \
	done

clean:
	$(RM) $(DIRS)
