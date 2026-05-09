CC = gcc
CFLAGS = -std=c23 -Wall -Werror 
LDFLAGS = -static
LDLIBS = -lws2_32

SRCDIR := src
BUILDDIR := build
TARGET := bin/snake_scores
SRCEXT := c

SRCS := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJS := $(patsubst $(SRCDIR)/%.$(SRCEXT),$(BUILDDIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR) $(BUILDDIR)/lib bin
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	@$(RM) -r $(BUILDDIR) $(TARGET) bin
	@$(RM) scores.db

.PHONY: clean