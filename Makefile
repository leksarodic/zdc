CMAKE ?= cmake
PYTHON ?= python3
BT_NAME ?= CAR_482
ARM_CC ?= arm-linux-gnueabi-gcc
QEMU ?= qemu-arm
BUILD ?= build
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
XVFB ?= xvfb-run -a
endif

.PHONY: all build test emulator screenshot arm qemu-test qemu-docker image verify-image clean

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
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/volume.ppm --frames 30 --state connected --volume 62
	@$(PYTHON) tools/ppm_to_png.py artifacts/volume.ppm artifacts/volume.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/light.ppm --frames 30 --state connected --light
	@$(PYTHON) tools/ppm_to_png.py artifacts/light.ppm artifacts/light.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/light-settings.ppm --frames 30 --state connected --panel settings --light
	@$(PYTHON) tools/ppm_to_png.py artifacts/light-settings.ppm artifacts/light-settings.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/night-dim.ppm --frames 30 --state connected --dim
	@$(PYTHON) tools/ppm_to_png.py artifacts/night-dim.ppm artifacts/night-dim.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/screensaver.ppm --frames 30 --state connected --saver
	@$(PYTHON) tools/ppm_to_png.py artifacts/screensaver.ppm artifacts/screensaver.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/screensaver-light.ppm --frames 30 --state connected --saver --light
	@$(PYTHON) tools/ppm_to_png.py artifacts/screensaver-light.ppm artifacts/screensaver-light.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/files-music.ppm --frames 30 --state connected --panel files --file media/Music/sunset_drive.wav
	@$(PYTHON) tools/ppm_to_png.py artifacts/files-music.ppm artifacts/files-music.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/files-photo.ppm --frames 30 --state connected --panel files --file media/Photos/sunset.bmp
	@$(PYTHON) tools/ppm_to_png.py artifacts/files-photo.ppm artifacts/files-photo.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/pair-help.ppm --frames 30 --state ready --pair-help
	@$(PYTHON) tools/ppm_to_png.py artifacts/pair-help.ppm artifacts/pair-help.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/settings-reset.ppm --frames 30 --state connected --settings-modal reset
	@$(PYTHON) tools/ppm_to_png.py artifacts/settings-reset.ppm artifacts/settings-reset.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/disclaimer.ppm --frames 30 --state connected --settings-modal disclaimer
	@$(PYTHON) tools/ppm_to_png.py artifacts/disclaimer.ppm artifacts/disclaimer.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/files-sources.ppm --frames 30 --state connected --panel files --files-root media --files-root media-sd
	@$(PYTHON) tools/ppm_to_png.py artifacts/files-sources.ppm artifacts/files-sources.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/files-sd.ppm --frames 30 --state connected --panel files --files-root media --files-root media-sd --storage-index 1
	@$(PYTHON) tools/ppm_to_png.py artifacts/files-sd.ppm artifacts/files-sd.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/call-interrupt.ppm --frames 30 --state connected --interrupt
	@$(PYTHON) tools/ppm_to_png.py artifacts/call-interrupt.ppm artifacts/call-interrupt.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/call-banner.ppm --frames 30 --state incoming --panel radio
	@$(PYTHON) tools/ppm_to_png.py artifacts/call-banner.ppm artifacts/call-banner.png
	@$(XVFB) ./$(BUILD)/strada-ui --screenshot artifacts/radio-muted.ppm --frames 30 --state connected --panel radio --radio-mute
	@$(PYTHON) tools/ppm_to_png.py artifacts/radio-muted.ppm artifacts/radio-muted.png

arm:
	$(CMAKE) -S . -B build-arm \
		-DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabi.cmake \
		-DSTRADA_BUILD_SDL=OFF -DSTRADA_BUILD_FBDEV=ON \
		-DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build build-arm -j

qemu-docker:
	./tools/qemu_test_docker.sh

qemu-test:
	@command -v $(ARM_CC) >/dev/null 2>&1 || (echo "missing $(ARM_CC); install gcc-arm-linux-gnueabi or use 'make qemu-docker'" >&2; exit 1)
	@command -v $(QEMU) >/dev/null 2>&1 || (echo "missing $(QEMU); install qemu-user or use 'make qemu-docker'" >&2; exit 1)
	mkdir -p $(BUILD)
	$(ARM_CC) -static -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft -O2 -Iapp \
		tests/test_bt.c app/bt_sim.c app/bt_uart.c -o $(BUILD)/test_bt_arm
	$(QEMU) ./$(BUILD)/test_bt_arm

image:
	mkdir -p artifacts
	$(PYTHON) tools/patch_bt_name.py firmware/7010b-original.bin \
		-o artifacts/7010b-$(BT_NAME).img --name $(BT_NAME)

verify-image: image
	$(PYTHON) tools/patch_bt_name.py artifacts/7010b-$(BT_NAME).img \
		--verify-against firmware/7010b-original.bin

clean:
	rm -rf $(BUILD) build-arm artifacts
