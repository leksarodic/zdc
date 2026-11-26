CMAKE ?= cmake
PYTHON ?= python3
BT_NAME ?= STRADA
BUILD ?= build
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
XVFB ?= xvfb-run -a
endif

.PHONY: all build test emulator screenshot arm image verify-image clean

all: build

build:
	$(CMAKE) -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD) -j

test: build
	ctest --test-dir $(BUILD) --output-on-failure

emulator: build
	$(XVFB) ./$(BUILD)/strada-ui

screenshot: build
	mkdir -p artifacts
	@for st in off ready pairing connected playing incoming active error; do \
		$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/$$st.ppm --frames 30 --state $$st; \
		$(PYTHON) tools/ppm_to_png.py artifacts/$$st.ppm artifacts/$$st.png; \
	done
	@for p in radio files settings; do \
		$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/$$p.ppm --frames 30 --state connected --panel $$p; \
		$(PYTHON) tools/ppm_to_png.py artifacts/$$p.ppm artifacts/$$p.png; \
	done
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/call.ppm --frames 30 --state connected --call
	@$(PYTHON) tools/ppm_to_png.py artifacts/call.ppm artifacts/call.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/files-no-storage.ppm --frames 30 --state connected --panel files --files-root /nonexistent
	@$(PYTHON) tools/ppm_to_png.py artifacts/files-no-storage.ppm artifacts/files-no-storage.png

arm:
	$(CMAKE) -S . -B build-arm \
		-DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabi.cmake \
		-DSTRADA_BUILD_SDL=OFF -DSTRADA_BUILD_FBDEV=ON \
		-DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build build-arm -j

image:
	mkdir -p artifacts
	$(PYTHON) tools/patch_bt_name.py firmware/7010b-original.bin \
		-o artifacts/7010b-$(BT_NAME).img --name $(BT_NAME)

verify-image: image
	$(PYTHON) tools/patch_bt_name.py artifacts/7010b-$(BT_NAME).img \
		--verify-against firmware/7010b-original.bin

clean:
	rm -rf $(BUILD) build-arm artifacts
