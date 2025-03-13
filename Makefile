# Compiler
CC = gcc

# Common NVML installation paths
NVML_PATHS := /usr/local/cuda /usr /usr/local /opt/nvidia

# Function to find NVML_PATH
find_nvml_path = $(firstword $(foreach dir,$(NVML_PATHS),$(wildcard $(dir)/include/nvml.h)))

# NVML path discovery
NVML_INCLUDE := $(shell dirname $(call find_nvml_path))
NVML_LIB := $(shell echo $(NVML_INCLUDE) | sed 's/include/lib64/')

# Check if NVML_PATH is found
ifneq ($(NVML_INCLUDE),)
  CFLAGS += -I$(NVML_INCLUDE)
  LIBS +=  -lnvidia-ml -lncurses
else
  $(error NVML path not found. Please set NVML_PATH manually.)
endif

# Compiler flags
CFLAGS += -Wall -Wextra -O2 -march=native -mtune=native -flto -funroll-loops
# Target executable
TARGET = gpulite

# Source files
SRCS = ./src/main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Installation directory
INSTALL_DIR = /usr/local/bin

# Default target
all: $(TARGET) clean_obj

# Rule for building the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for cleaning up generated files
clean:
	rm -f $(TARGET) $(OBJS)

clean_obj:
	rm -f $(OBJS)

# Rule for running the program
run: $(TARGET)
	./$(TARGET)

# Install target
install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)

# Uninstall target
uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)

.PHONY: all clean run install uninstall

