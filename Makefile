default: lib bin

.PHONY: lib bin test clean clean_test

CC = gcc
LD = gcc
AR = gcc-ar
MD = mkdir -p
RM = rm -rf

CSTD          = c99
CFLAGS        = -std=$(CSTD) -Wall -Wextra -pedantic -Werror -I$(INCLUDEDIR)
LDFLAGS       =
LIBS          = -lm
ARFLAGS       = cr

TESTFLAGS     = -fsanitize=address -fsanitize=leak -fsanitize=undefined
DEBUGFLAGS   := -g -O0 $(TESTFLAGS)
RELEASEFLAGS  = -DNDEBUG -flto -O3
DEPFLAGS      = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

ifdef RELEASE
	CFLAGS  += $(RELEASEFLAGS)
	LDFLAGS += $(RELEASEFLAGS)
else
	CFLAGS  += $(DEBUGFLAGS)
	LDFLAGS += $(DEBUGFLAGS)
	TESTFLAGS =
endif

BUILDDIR   = build
OBJDIR     = build/objs
DEPDIR     = build/deps
TESTDIR    = $(BUILDDIR)/tests
DIRS       = $(BUILDDIR) $(OBJDIR) $(DEPDIR) $(TESTDIR)

INCLUDEDIR = include
SRCDIR     = src

OBJS       = $(patsubst %.c,$(OBJDIR)/%.o,$(filter-out %_test.c,$(notdir $(wildcard $(SRCDIR)/*.c))))
TESTS      = $(patsubst %_test.c,$(TESTDIR)/%,$(notdir $(wildcard $(SRCDIR)/*_test.c)))
TESTLOG    = $(BUILDDIR)/test.log

$(DIRS):
	$(MD) $@

$(OBJDIR)/%.o : $(SRCDIR)/%.c
$(OBJDIR)/%.o : $(SRCDIR)/%.c $(DEPDIR)/%.d | $(DIRS)
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

$(BUILDDIR)/libsmlisp.a : $(filter-out %/main.o,$(OBJS)) | $(DIRS)
	$(AR) $(ARFLAGS) $@ $^

$(BUILDDIR)/smlisp : $(OBJDIR)/main.o $(BUILDDIR)/libsmlisp.a | $(DIRS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TESTDIR)/% : $(SRCDIR)/%_test.c $(BUILDDIR)/libsmlisp.a | $(DIRS)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

lib: $(BUILDDIR)/libsmlisp.a
bin: $(BUILDDIR)/smlisp

test: clean_test $(TESTS)
	@echo Starting test suite
	@$(RM) $(TESTLOG)
	@for test in $^; do \
		printf "Testing $$(basename "$$test")...\r"; \
		"$$test" 2>&1 | tee $(TESTLOG) | grep "PANIC\|FAIL\|tests passed"; \
	done

clean_test:
	$(RM) $(TESTDIR)

clean:
	$(RM) $(DIRS)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(DEPDIR)/*.d)
