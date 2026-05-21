BUILD_DIR = build
LOCAL_LIBS_DIR = libs
CC = gcc
UNAME_S := $(shell uname -s)

CFLAGS = $$(pkg-config --cflags raylib sndfile) -I$(LOCAL_LIBS_DIR)/raygui/src
LDFLAGS = $$(pkg-config --libs raylib sndfile)

WEB_BUILD_DIR = $(BUILD_DIR)/web
BUILD_WEB_PATH = $(LOCAL_LIBS_DIR)/raylib-web
# NO LONGER NEEDED: $(LOCAL_LIBS_DIR)/libsndfile/libsndfile.a
BUILD_WEB_FLAGS = $(BUILD_WEB_PATH)/lib/libraylib.a \
	-I$(BUILD_WEB_PATH)/include \
	-I$(LOCAL_LIBS_DIR)/raygui/src \
	$$(pkg-config --cflags-only-I sndfile)

BUILD_WEB_RESOURCES_PATH  ?= $(dir $<)resources@resources
BUILD_WEB_SHELL ?= minshell.html

.PHONY: build debug clean build-web build-web-deploy

build: clean
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -O3 -o $(BUILD_DIR)/spectrogram main.c $(LDFLAGS)

debug: clean
	$(CC) $(CFLAGS) -g -O0 -o $(BUILD_DIR)/spectrogram main.c $(LDFLAGS) -fsanitize=address

clean:
	rm -f $(BUILD_DIR)/spectrogram

build-web:
	mkdir -p $(WEB_BUILD_DIR)
	emcc -o $(WEB_BUILD_DIR)/spectrogram.html main.c -Os -Wall -DPLATFORM_WEB \
		$(BUILD_WEB_FLAGS) -sUSE_GLFW=3 -sASYNCIFY -sFORCE_FILESYSTEM=1 -sMINIFY_HTML=1 \
		-sINITIAL_MEMORY=64MB -sMAXIMUM_MEMORY=1024MB -sALLOW_MEMORY_GROWTH=1 \
		--preload-file $(BUILD_WEB_RESOURCES_PATH) \
		--shell-file $(BUILD_WEB_SHELL)
		
build-web-deploy: build-web
	rm -rf ./docs
	cp -r $(WEB_BUILD_DIR) ./docs
	cp $(WEB_BUILD_DIR)/spectrogram.html ./docs/index.html
	rm -rf ./docs/spectrogram.html