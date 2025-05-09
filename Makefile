CC = gcc
CFLAGS = -Wall -Wextra -g -I$(SRC_DIR)
LDFLAGS =
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests

# Main executable name
TARGET = $(BIN_DIR)/gc

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%.o, $(TEST_SRCS))
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c, $(BIN_DIR)/test_%, $(TEST_SRCS))

# Heap checker test targets
HEAP_CHECK_TARGET = $(BIN_DIR)/test_gc_with_checker

# Default target
all: directories $(TARGET) tests

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Build main executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables
tests: $(TEST_TARGETS)

$(BIN_DIR)/test_%: $(BUILD_DIR)/%.o $(filter-out $(BUILD_DIR)/main.o, $(OBJS))
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Heap checker test
heap_check: directories $(BUILD_DIR)/test_gc_with_checker.o $(filter-out $(BUILD_DIR)/main.o, $(OBJS))
	$(CC) $(LDFLAGS) -o $(HEAP_CHECK_TARGET) $(BUILD_DIR)/test_gc_with_checker.o $(filter-out $(BUILD_DIR)/main.o, $(OBJS))

# Run main program
run: $(TARGET)
	./$(TARGET)

# Run heap checker test
run_heap_check: heap_check
	./$(HEAP_CHECK_TARGET)

# Run all tests
test: tests
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test"; \
		./$$test; \
		echo ""; \
	done

# Run memory check with Valgrind
memcheck: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all directories tests run test memcheck clean heap_check run_heap_check 