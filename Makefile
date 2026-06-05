CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude -MMD -MP
LDFLAGS =
DEVFLAGS = -fsanitize=address,undefined -g
RELFLAGS = -O3

DEP_DIRS := $(wildcard deps/*)
DEP_NAMES := $(notdir $(DEP_DIRS)) # extract all dependency names 

CFLAGS += $(foreach dir,$(DEP_DIRS),-I$(dir)/include)
LDFLAGS += $(foreach dir,$(DEP_DIRS),-L$(dir))
LDFLAGS += $(foreach name,$(DEP_NAMES),-l$(name))

# Assume expected archive targets for dependencies are of the path deps/$(name)/lib$(name).a
DEP_LIBS := $(foreach name,$(DEP_NAMES),deps/$(name)/lib$(name).a)

BIN = cshell
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

TEST_SRC := $(wildcard tests/test_*.c)
TEST_BIN := $(TEST_SRC:.c=)

DEPS := $(OBJ:.o=.d) $(TEST_SRC:.c=.d)

.PHONY: all clean test debug setup_deps

all: $(BIN)

debug: CFLAGS += $(DEVFLAGS)
debug: LDFLAGS += $(DEVFLAGS)
debug: all

$(BIN): $(OBJ) $(DEP_LIBS)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

$(DEP_LIBS):
	$(MAKE) -C $(@D)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

setup_deps:
	git submodule update --init --recursive

test: CFLAGS += $(DEVFLAGS)
test: LDFLAGS += $(DEVFLAGS)
test: $(TEST_BIN)
	@for bin in $(TEST_BIN); do ./$$bin; done

tests/%: tests/%.c $(OBJ) $(DEP_LIBS)
	$(CC) $(CFLAGS) $< $(filter-out src/main.o, $(OBJ)) $(LDFLAGS) -o $@

-include $(DEPS)

clean:
	rm -f src/*.o $(BIN) $(TEST_BIN) $(DEPS)

clean_deps: clean
	@for dir in $(DEP_DIRS); do $(MAKE) -C $$dir clean; done
