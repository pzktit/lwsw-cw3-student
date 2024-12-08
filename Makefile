APP_NAME := app

# Source and build directories
SRC_DIR := src
BUILD_DIR := build_dir
INC_DIR := include

ARCH := aarch64-linux-gnu-
CXXOPTS := -I$(INC_DIR)
LDOPTS := -static -Llib

# tools
CXX := $(ARCH)g++
CPP := $(ARCH)g++


# Preprocesor flags
CPPFLAG :=
# Compiler and flags
CXXFLAGS := -g -Wall -Wextra -fdiagnostics-color=always -std=c++20 $(CXXOPTS)
# Linker flags
LDFLAGS := $(LDOPTS) -lgpiodcxx -lgpiod



# List of source files and corresponding object files
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))
DEP_FILES := $(OBJ_FILES:$(BUILD_DIR)/%.o=$(BUILD_DIR)/%.d)

#HDR_FILES := $(wildcard $(INC_DIR)/*.hpp) $(wildcard $(INC_DIR)/*.h)

# Output binary
TARGET := $(BUILD_DIR)/$(APP_NAME)

# Default target
build:  $(BUILD_DIR) $(TARGET)

# Link object files into the final executable
$(TARGET): $(OBJ_FILES) | $(BUILD_DIR)
	$(CXX) $(OBJ_FILES) $(LDFLAGS) -o $(TARGET)
	echo $(DEP_FILES)

# Creates the dependecy rules
%.d: $(SRC_FILES) | $(BUILD_DIR)
	@$(CPP) $(CXXFLAGS) $(CPPFLAGS) $< -MM -MT $(@:%.d=%.o) >$@

# Includes all .h files
-include $(DEP_FILES)

# Compile source files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

depend: $(DEP_FILES)
	@echo "Dependencies regenerated"

# Clean build artifacts
depclean:
	@rm -f $(BUILD_DIR)/*.d

clean: depclean
	@rm -f $(BUILD_DIR)/*
	@rmdir $(BUILD_DIR)

run: build
	scp $(BUILD_DIR)/$(APP_NAME) target:
	ssh target ./$(APP_NAME)

debug: build
	scp $(BUILD_DIR)/$(APP_NAME) target:
	ssh target gdbserver --once :2345 ./$(APP_NAME)
