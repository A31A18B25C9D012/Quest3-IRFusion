NDK         ?= $(HOME)/android/android-ndk-r26b
API         := 33
ABI         := arm64-v8a
TRIPLE      := aarch64-linux-android
HOST_CC     := gcc
PYTHON      := python3

TOOLCHAIN   := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
CC          := $(TOOLCHAIN)/$(TRIPLE)$(API)-clang
AR          := $(TOOLCHAIN)/llvm-ar
RANLIB      := $(TOOLCHAIN)/llvm-ranlib
STRIP       := $(TOOLCHAIN)/llvm-strip

GLSLC       ?= glslc

SYSROOT     := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/sysroot
VK_INCLUDE  := $(NDK)/sources/third_party/vulkan/src/include

OPENXR_INCLUDE ?= $(HOME)/dev/openxr/include
OPENXR_LIB     ?= $(HOME)/dev/openxr/build-android/src/loader/libopenxr_loader.a

LIBUVC_INCLUDE ?= $(HOME)/dev/libuvc/include
LIBUVC_LIB     ?= $(HOME)/dev/libuvc/build-android/libuvc.a

LIBUSB_INCLUDE ?= $(HOME)/dev/libusb/libusb
LIBUSB_LIB     ?= $(HOME)/dev/libusb/build-android/libusb/libusb-1.0.a

KERNEL_DIR  ?= /lib/modules/$(shell uname -r)/build

BUILD           := build
BUILD_ANDROID   := $(BUILD)/android-$(ABI)
BUILD_HOST      := $(BUILD)/host
BUILD_SPV       := $(BUILD)/spv
BUILD_KERN      := $(BUILD)/kernel

CFLAGS := \
    -O2 -Wall -Wextra -std=c11 \
    -march=armv8-a+simd \
    --sysroot=$(SYSROOT) \
    -isystem $(SYSROOT)/usr/include \
    -isystem $(SYSROOT)/usr/include/$(TRIPLE) \
    -I$(VK_INCLUDE) \
    -I$(OPENXR_INCLUDE) \
    -I$(LIBUVC_INCLUDE) \
    -I$(LIBUSB_INCLUDE) \
    -Isrc \
    -D__ANDROID__ \
    -DANDROID \
    -fPIC \
    -fno-exceptions

LDFLAGS := \
    -L$(SYSROOT)/usr/lib/$(TRIPLE)/$(API) \
    -lm \
    -landroid \
    -llog \
    -lpthread \
    -Wl,--strip-all

HOST_CFLAGS := -O2 -Wall -Wextra -std=c11 -Isrc

MATH_SRC := \
    src/math/mat4.c \
    src/math/se3.c \
    src/math/kalman.c \
    src/math/camera_math.c

SYNC_SRC := \
    src/sync/timestamp_align.c

HAL_SRC := \
    src/hal/camera_hal3.c \
    src/hal/uvc_camera.c

CAL_SRC := \
    src/calibration/calibration.c

VULKAN_SRC := \
    src/vulkan/vk_context.c \
    src/vulkan/vk_image.c \
    src/vulkan/vk_pipeline.c \
    src/vulkan/vk_dispatch.c

SERVICE_SRC := \
    src/service/stereo_depth.c \
    src/service/ir_overlay.c \
    src/service/fusion_service.c

INPUT_SRC := \
    src/input/button_input.c

HUD_SRC := \
    src/hud/hud_renderer.c

DISTORTION_SRC := \
    src/distortion/lens_correction.c

OPENXR_SRC := \
    src/openxr/compositor.c

ALL_SRC := \
    $(MATH_SRC) \
    $(SYNC_SRC) \
    $(HAL_SRC) \
    $(CAL_SRC) \
    $(VULKAN_SRC) \
    $(SERVICE_SRC) \
    $(INPUT_SRC) \
    $(HUD_SRC) \
    $(DISTORTION_SRC) \
    $(OPENXR_SRC)

MATH_OBJ        := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(MATH_SRC))
SYNC_OBJ        := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(SYNC_SRC))
HAL_OBJ         := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(HAL_SRC))
CAL_OBJ         := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(CAL_SRC))
VULKAN_OBJ      := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(VULKAN_SRC))
SERVICE_OBJ     := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(SERVICE_SRC))
INPUT_OBJ       := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(INPUT_SRC))
HUD_OBJ         := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(HUD_SRC))
DISTORTION_OBJ  := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(DISTORTION_SRC))
OPENXR_OBJ      := $(patsubst src/%.c,$(BUILD_ANDROID)/%.o,$(OPENXR_SRC))

SPIRV_EMBED_OBJ := \
    $(BUILD_ANDROID)/spirv/ir_warp_spirv.o \
    $(BUILD_ANDROID)/spirv/composite_spirv.o \
    $(BUILD_ANDROID)/spirv/edge_spirv.o \
    $(BUILD_ANDROID)/spirv/hud_spirv.o \
    $(BUILD_ANDROID)/spirv/lens_spirv.o

ALL_OBJ := \
    $(MATH_OBJ) \
    $(SYNC_OBJ) \
    $(HAL_OBJ) \
    $(CAL_OBJ) \
    $(VULKAN_OBJ) \
    $(SERVICE_OBJ) \
    $(INPUT_OBJ) \
    $(HUD_OBJ) \
    $(DISTORTION_OBJ) \
    $(OPENXR_OBJ) \
    $(SPIRV_EMBED_OBJ)

SHADERS := \
    $(BUILD_SPV)/ir_warp.spv \
    $(BUILD_SPV)/composite.spv \
    $(BUILD_SPV)/edge_detect.spv \
    $(BUILD_SPV)/hud.spv \
    $(BUILD_SPV)/lens_correct.spv

HOST_MATH_OBJ := \
    $(BUILD_HOST)/math/mat4.o \
    $(BUILD_HOST)/math/se3.o \
    $(BUILD_HOST)/math/kalman.o \
    $(BUILD_HOST)/math/camera_math.o

HOST_CAL_OBJ := \
    $(BUILD_HOST)/calibration/calibration.o \
    $(BUILD_HOST)/calibration/calibrate.o

.PHONY: all service shaders calibrate kernel_module hud_preview push clean

all: service calibrate shaders kernel_module hud_preview

service: $(BUILD_ANDROID)/ir_fusion_service

shaders: $(SHADERS)

calibrate: $(BUILD_HOST)/calibrate

kernel_module: $(BUILD_KERN)/ir_uvc_bridge.ko

hud_preview: $(BUILD_HOST)/hud_preview

$(BUILD_ANDROID)/ir_fusion_service: $(ALL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ \
	    $(LIBUVC_LIB) \
	    $(LIBUSB_LIB) \
	    $(OPENXR_LIB) \
	    $(LDFLAGS)

$(BUILD_ANDROID)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_ANDROID)/spirv/ir_warp_spirv.o: $(BUILD_SPV)/ir_warp.spv
	@mkdir -p $(dir $@)
	$(PYTHON) tools/embed_spirv.py $< ir_warp_spirv > $(BUILD_ANDROID)/spirv/ir_warp_spirv.c
	$(CC) $(CFLAGS) -c -o $@ $(BUILD_ANDROID)/spirv/ir_warp_spirv.c

$(BUILD_ANDROID)/spirv/composite_spirv.o: $(BUILD_SPV)/composite.spv
	@mkdir -p $(dir $@)
	$(PYTHON) tools/embed_spirv.py $< composite_spirv > $(BUILD_ANDROID)/spirv/composite_spirv.c
	$(CC) $(CFLAGS) -c -o $@ $(BUILD_ANDROID)/spirv/composite_spirv.c

$(BUILD_ANDROID)/spirv/edge_spirv.o: $(BUILD_SPV)/edge_detect.spv
	@mkdir -p $(dir $@)
	$(PYTHON) tools/embed_spirv.py $< edge_spirv > $(BUILD_ANDROID)/spirv/edge_spirv.c
	$(CC) $(CFLAGS) -c -o $@ $(BUILD_ANDROID)/spirv/edge_spirv.c

$(BUILD_ANDROID)/spirv/hud_spirv.o: $(BUILD_SPV)/hud.spv
	@mkdir -p $(dir $@)
	$(PYTHON) tools/embed_spirv.py $< hud_spirv > $(BUILD_ANDROID)/spirv/hud_spirv.c
	$(CC) $(CFLAGS) -c -o $@ $(BUILD_ANDROID)/spirv/hud_spirv.c

$(BUILD_ANDROID)/spirv/lens_spirv.o: $(BUILD_SPV)/lens_correct.spv
	@mkdir -p $(dir $@)
	$(PYTHON) tools/embed_spirv.py $< lens_spirv > $(BUILD_ANDROID)/spirv/lens_spirv.c
	$(CC) $(CFLAGS) -c -o $@ $(BUILD_ANDROID)/spirv/lens_spirv.c

$(BUILD_SPV)/ir_warp.spv: src/vulkan/shaders/ir_warp.comp
	@mkdir -p $(dir $@)
	$(GLSLC) --target-env=vulkan1.1 -O $< -o $@

$(BUILD_SPV)/composite.spv: src/vulkan/shaders/composite.comp
	@mkdir -p $(dir $@)
	$(GLSLC) --target-env=vulkan1.1 -O $< -o $@

$(BUILD_SPV)/edge_detect.spv: src/vulkan/shaders/edge_detect.comp
	@mkdir -p $(dir $@)
	$(GLSLC) --target-env=vulkan1.1 -O $< -o $@

$(BUILD_SPV)/hud.spv: src/vulkan/shaders/hud.comp
	@mkdir -p $(dir $@)
	$(GLSLC) --target-env=vulkan1.1 -O $< -o $@

$(BUILD_SPV)/lens_correct.spv: src/vulkan/shaders/lens_correct.comp
	@mkdir -p $(dir $@)
	$(GLSLC) --target-env=vulkan1.1 -O $< -o $@

$(BUILD_HOST)/calibration/calibrate.o: src/calibration/calibrate.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $(shell pkg-config --cflags opencv4 2>/dev/null || pkg-config --cflags opencv) -c -o $@ $<

$(BUILD_HOST)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c -o $@ $<

$(BUILD_HOST)/tools/hud_preview.o: tools/hud_preview.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -c -o $@ $<

$(BUILD_HOST)/calibrate: $(HOST_MATH_OBJ) $(HOST_CAL_OBJ)
	@mkdir -p $(dir $@)
	$(HOST_CC) -o $@ $^ \
	    $(shell pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv) \
	    -lm -lpthread

$(BUILD_HOST)/hud_preview: $(BUILD_HOST)/tools/hud_preview.o
	@mkdir -p $(dir $@)
	$(HOST_CC) -o $@ $^ -lm

$(BUILD_KERN)/ir_uvc_bridge.ko: src/kernel/ir_uvc_bridge.c src/kernel/Kbuild
	@mkdir -p $(BUILD_KERN)
	cp src/kernel/ir_uvc_bridge.c $(BUILD_KERN)/
	cp src/kernel/Kbuild $(BUILD_KERN)/
	$(MAKE) -C $(KERNEL_DIR) M=$(abspath $(BUILD_KERN)) modules

push: service
	adb push $(BUILD_ANDROID)/ir_fusion_service /data/local/tmp/ir_fusion_service
	adb shell chmod 755 /data/local/tmp/ir_fusion_service

clean:
	rm -rf $(BUILD)