EXT_DIR ?= $(HOME)/.local/share/SuperCollider/Extensions/PT2399UGen
BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: build install test clean

build:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(EXT_DIR)"
	$(CMAKE) --build $(BUILD_DIR) -j

install: build
	$(CMAKE) --install $(BUILD_DIR)

test: install
	QT_QPA_PLATFORM=offscreen sclang PT2399_test.scd

clean:
	rm -rf $(BUILD_DIR)
