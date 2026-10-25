# =====================================================================
#  TreeKeeper - build system
#
#  make              optimised build            -> ./a.out
#  make debug        -O0 -g3 for gdb / valgrind -> ./a.out
#  make asan         AddressSanitizer + UBSan   -> ./a.out
#  make test         build and run unit tests
#  make smoke        run the scripted CLI demo (Tests/demo_script.txt)
#  make valgrind     leak-check tests + CLI under valgrind (Linux)
#  make leaks        leak-check tests + CLI with macOS 'leaks'
#  make run          build and start the program
#  make clean        remove object files and binaries
#  make distclean    clean + delete saved data/
# =====================================================================

ifeq ($(origin CC),default)
CC := cc
endif

MODE     ?= release
SRC_DIR  := Source
INC_DIR  := include
TEST_DIR := Tests
OBJ_ROOT := object
OBJ_DIR  := $(OBJ_ROOT)/$(MODE)
TARGET   := a.out
TEST_BIN := $(OBJ_DIR)/test_runner

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIB_OBJS  := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))
TEST_OBJS := $(OBJ_DIR)/tests/test_main.o
DEPS      := $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

CPPFLAGS := -I$(INC_DIR) -D_POSIX_C_SOURCE=200809L -MMD -MP
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wmissing-prototypes -Wformat=2 -Wpointer-arith -Wcast-align \
            -Wundef -Wwrite-strings
CFLAGS   := -std=c11 $(WARNINGS) -pthread
LDFLAGS  := -pthread
LDLIBS   := -lm

ifeq ($(MODE),release)
  CFLAGS  += -O2 -DNDEBUG
else ifeq ($(MODE),debug)
  CFLAGS  += -O0 -g3 -DTK_DEBUG
else ifeq ($(MODE),asan)
  SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
  CFLAGS  += -O1 -g $(SAN)
  LDFLAGS += $(SAN)
else
  $(error Unknown MODE '$(MODE)': use release, debug or asan)
endif

# Relink a.out whenever the build mode changes.
MODE_STAMP := $(OBJ_ROOT)/.mode

SMOKE_DIR := $(OBJ_ROOT)/smoke_data
VALGRIND  := valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1

.PHONY: all debug asan test smoke valgrind leaks run clean distclean help FORCE

all: $(TARGET)

$(TARGET): $(OBJS) $(MODE_STAMP)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo "Built $@ ($(MODE))"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)/tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR) $(OBJ_DIR)/tests:
	@mkdir -p $@

$(MODE_STAMP): FORCE
	@mkdir -p $(OBJ_ROOT)
	@echo '$(MODE)' | cmp -s - $@ || echo '$(MODE)' > $@

debug:
	@$(MAKE) --no-print-directory MODE=debug all

asan:
	@$(MAKE) --no-print-directory MODE=asan all

test: $(TEST_BIN)
	./$(TEST_BIN)

smoke: $(TARGET)
	@rm -rf $(SMOKE_DIR)
	./$(TARGET) --fresh --no-color --data $(SMOKE_DIR) < $(TEST_DIR)/demo_script.txt
	@echo "--- reload from disk ---"
	echo "list" | ./$(TARGET) --no-color --no-save --data $(SMOKE_DIR)

valgrind:
	@$(MAKE) --no-print-directory MODE=debug all $(OBJ_ROOT)/debug/test_runner
	$(VALGRIND) ./$(OBJ_ROOT)/debug/test_runner
	@rm -rf $(SMOKE_DIR)
	$(VALGRIND) ./$(TARGET) --fresh --no-color --data $(SMOKE_DIR) < $(TEST_DIR)/demo_script.txt

leaks:
	@$(MAKE) --no-print-directory MODE=debug all $(OBJ_ROOT)/debug/test_runner
	MallocStackLogging=1 leaks --atExit -- ./$(OBJ_ROOT)/debug/test_runner
	@rm -rf $(SMOKE_DIR)
	MallocStackLogging=1 leaks --atExit -- ./$(TARGET) --fresh --no-color --data $(SMOKE_DIR) < $(TEST_DIR)/demo_script.txt

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_ROOT) $(TARGET)

distclean: clean
	rm -rf data

help:
	@sed -n '2,16p' Makefile

FORCE:

-include $(DEPS)
