CC      ?= cc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wpedantic -Werror
OPT_REL := -O2
OPT_DBG := -O0 -g
SAN     := -fsanitize=address,undefined

CFLAGS_RELEASE := $(CSTD) $(WARN) $(OPT_REL)
CFLAGS_TEST    := $(CSTD) $(WARN) $(OPT_DBG) $(SAN) -DRENDER_TEST_INSPECT
LDLIBS         := -lm

PROD_SRC := main.c physics.c term.c term_parse.c render.c scenes.c
PROD_OBJ := $(PROD_SRC:.c=.o)
TARGET   := physics

TEST_SRC := test/runner.c test/term_stub.c test/test_physics.c test/test_render.c \
            test/test_term.c test/test_scenes.c \
            physics.c render.c scenes.c term_parse.c
TEST_BIN := test/runner

# Default: build the demo binary.
all: $(TARGET)

$(TARGET): $(PROD_SRC)
	$(CC) $(CFLAGS_RELEASE) -o $@ $^ $(LDLIBS)

# Test build: pulls real physics.c and render.c, replaces term.c with the stub.
$(TEST_BIN): $(TEST_SRC)
	$(CC) $(CFLAGS_TEST) -o $@ $^ $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TARGET) $(TEST_BIN) *.o test/*.o

.PHONY: all test clean
