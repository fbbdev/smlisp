default: lib

.PHONY: lib test clean clean_test

CC = gcc
LD = gcc
AR = gcc-ar
MD = mkdir -p
RM = rm -rf

CSTD          = c99
CFLAGS        = -std=$(CSTD) -Wall -Wextra -pedantic -Werror -I$(INCLUDEDIR)
LDFLAGS       =
LIBS          =
ARFLAGS       = crf

TESTFLAGS     = -fsanitize=address -fsanitize=leak -fsanitize=undefined
DEBUGFLAGS   := -g -O0 $(TESTFLAGS)
RELEASEFLAGS  = -DNDEBUG -flto -O3

ifdef RELEASE
	CFLAGS  += $(RELEASEFLAGS)
	LDFLAGS += $(RELEASEFLAGS)
else
	CFLAGS  += $(DEBUGFLAGS)
	LDFLAGS += $(DEBUGFLAGS)
	TESTFLAGS =
endif

BUILDDIR   = build
TESTDIR    = $(BUILDDIR)/tests
DIRS       = $(BUILDDIR) $(TESTDIR)

INCLUDEDIR = include
SRCDIR     = src

OBJS       = $(patsubst %.c,$(BUILDDIR)/%.o,$(filter-out %_test,$(notdir $(wildcard $(SRCDIR)/*.c))))
TESTLOG    = $(BUILDDIR)/test.log

$(DIRS):
	$(MD) $@

$(BUILDDIR)/%.o : $(SRCDIR)/%.c | $(DIRS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/libsmlisp.a : $(OBJS) | $(DIRS)
	$(AR) $(ARFLAGS) $@ $^

lib: $(BUILDDIR)/libsmlisp.a

$(TESTDIR)/% : $(SRCDIR)/%_test.c $(SRCDIR)/%.c $(SRCDIR)/util.c | $(DIRS)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(LDFLAGS) -o $@ $^

test: clean_test $(patsubst %_test.c,$(TESTDIR)/%,$(notdir $(wildcard $(SRCDIR)/*_test.c)))
	@echo Starting test suite
	@$(RM) $(TESTLOG)
	@for test in $^; do \
		echo -ne "Testing $$(basename "$$test")...\r"; \
		"$$test" 2>&1 | tee $(TESTLOG) | grep "PANIC\|FAIL\|tests passed"; \
	done

clean_test:
	$(RM) $(TESTDIR)

clean:
	$(RM) $(DIRS)
