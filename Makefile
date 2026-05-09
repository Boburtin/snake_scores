CC = gcc
CFLAGS = -std=c23 -Wall -Werror 
LDFLAGS = -static
LDLIBS = -lws2_32

SRCDIR := src
BUILDDIR := build
TARGET := bin/snake_scores
SRCEXT := c

SRCS := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJS := $(addprefix $(BUILDDIR)/,$(notdir $(SRCS:.$(SRCEXT)=.o)))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/lib/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	@$(RM) -r $(BUILDDIR) $(TARGET)

.PHONY: clean