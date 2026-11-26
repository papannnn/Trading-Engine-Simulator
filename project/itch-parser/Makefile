BUILD_DIR := build
DEBUG_DIR := build-debug
BUILD_TYPE ?= Release

.PHONY: all build run build-debug run-debug clean

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR)

# Runs from inside the build dir so "../data/sample.itch" resolves to data/sample.itch
run: build
	cd $(BUILD_DIR) && ./itch-parser

build-debug:
	cmake -S . -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(DEBUG_DIR)

run-debug: build-debug
	cd $(DEBUG_DIR) && ./itch-parser

clean:
	rm -rf $(BUILD_DIR) $(DEBUG_DIR)
