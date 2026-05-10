# Software Dependencies

## Overview

| Layer | Component | Version | License |
|-------|-----------|---------|---------|
| OS base | AOSP (XR branch) | android-14-release | Apache 2.0 |
| Kernel | Linux (Android Common Kernel) | 5.15 LTS or 6.1 LTS | GPL 2.0 |
| OpenXR runtime | Monado | latest main | BSL-1.0 |
| Compute API | Vulkan | 1.1 (required), 1.3 (preferred) | Proprietary (API spec) |
| Camera library | OpenCV | 4.9.x | Apache 2.0 |
| Linear algebra | Eigen | 3.4.0 | MPL 2.0 |
| Nonlinear optimizer | Ceres Solver | 2.2.0 | BSD 3-Clause |
| USB camera driver | libuvc | 0.0.7 | LGPL 2.1 |
| USB camera driver (alt) | android-uvcvideo (kernel module) | kernel-version matched | GPL 2.0 |
| Build system | CMake | 3.22+ | BSD |
| Compiler | Clang (Android NDK) | 17+ (via NDK r26+) | Apache 2.0 |
| NDK | Android NDK | r26b or r27 | Apache 2.0 |
| ADB / build tools | Android SDK platform-tools | latest | Apache 2.0 |

---

## 1. AOSP XR

### Repository

Source: android.googlesource.com

Manifest repo: https://android.googlesource.com/platform/manifest

Relevant branch: android-14-release (or the XR-specific branch if available under platform/manifest xr)

AOSP XR was announced in 2023. As of 2025, track the main AOSP branch and apply XR-specific vendor overlays separately.

### Disk and Memory Requirements

- AOSP source tree: ~300GB after sync.
- Build output: additional ~200GB.
- RAM for build: 32GB minimum, 64GB preferred.
- OS for build host: Ubuntu 22.04 LTS (recommended by AOSP documentation).

### Key Subsystems

| Subsystem | Path in AOSP | Purpose |
|-----------|--------------|---------|
| Camera HAL3 | hardware/interfaces/camera/provider | HAL interface definition |
| Camera service | frameworks/av/services/camera | Android camera service |
| Gralloc (buffer allocation) | hardware/interfaces/graphics/allocator | AHardwareBuffer backend |
| SurfaceFlinger | frameworks/native/services/surfaceflinger | Display compositor |
| Kernel UVC | drivers/media/usb/uvc/ | USB video class driver |

---

## 2. Android NDK

### Version

Use NDK r26b or r27.

Download: https://developer.android.com/ndk/downloads

### Target ABI

- arm64-v8a (64-bit ARM). Required for Snapdragon XR2 Gen 2.
- Do not target armeabi-v7a. Snapdragon XR2 Gen 2 is 64-bit only in practice.

### CMake Toolchain File

```cmake
set(CMAKE_TOOLCHAIN_FILE "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
set(ANDROID_PLATFORM android-33)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_STL c++_shared)
```

Minimum API level: 33 (Android 13). Required for full AHardwareBuffer and Vulkan 1.1 support.

---

## 3. Monado (OpenXR Runtime)

### Repository

https://gitlab.freedesktop.org/monado/monado

### Build for Android

```bash
git clone https://gitlab.freedesktop.org/monado/monado.git
cd monado
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33 \
  -DXRT_FEATURE_OPENXR=ON \
  -DXRT_BUILD_DRIVER_SURVIVE=OFF \
  -DXRT_BUILD_DRIVER_OPENHMD=OFF
cmake --build build-android
```

### Passthrough Compositor

Monado exposes the `XR_EXT_composition_layer_depth` extension for depth-aware compositing. For passthrough, implement a custom compositor driver that reads from the RGB HAL buffer and applies the IR overlay before submitting to the display backend.

Key source files:
- src/xrt/compositor/ (compositor interface)
- src/xrt/drivers/ (device drivers, add a custom IR fusion driver here)

### Alternative: Meta OpenXR SDK

If the target device remains stock Quest 3 firmware:
- Meta provides the OpenXR SDK as part of the Meta XR Core SDK.
- Passthrough access: XrPassthroughFB and related extensions (XR_FB_passthrough).
- Documentation: https://developer.meta.com/documentation/native/android/
- This path does not require AOSP modifications but limits access to passthrough buffer pixels.

---

## 4. Vulkan

### Version Requirements

- Vulkan 1.1 minimum. Snapdragon XR2 Gen 2 supports Vulkan 1.1 on Adreno 740.
- Required extension: VK_ANDROID_external_memory_android_hardware_buffer
- Required extension: VK_KHR_external_memory
- Optional: VK_KHR_timeline_semaphore (for precise GPU-CPU synchronization)

### Vulkan SDK

LunarG Vulkan SDK: https://vulkan.lunarg.com/sdk/home

For Android, Vulkan headers are included in the Android NDK (android-ndk/toolchains/llvm/prebuilt/).

### Zero-Copy Buffer Path

```c
// Import AHardwareBuffer as VkImage
VkAndroidHardwareBufferPropertiesANDROID props = {
    .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID
};
vkGetAndroidHardwareBufferPropertiesANDROID(device, buffer, &props);

VkExternalMemoryImageCreateInfo extMemInfo = {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
};
// ... create VkImage with extMemInfo in pNext chain
```

---

## 5. OpenCV

### Version

4.9.x (latest stable as of 2025).

### Repository

https://github.com/opencv/opencv

### Build for Android ARM

```bash
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
cd opencv
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33 \
  -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules \
  -DBUILD_ANDROID_EXAMPLES=OFF \
  -DBUILD_TESTS=OFF \
  -DWITH_OPENCL=OFF \
  -DWITH_CUDA=OFF \
  -DENABLE_NEON=ON
cmake --build build-android --parallel
```

### Modules Required

| Module | Use |
|--------|-----|
| core | Matrix operations, data types |
| calib3d | Camera calibration, stereoCalibrate, findHomography |
| imgproc | Sobel, cornerSubPix, undistort, remap |
| features2d | Feature detection for runtime correspondence |
| highgui | Debug visualization only (not in production) |

---

## 6. Eigen

### Version

3.4.0

### Repository

https://gitlab.com/libeigen/eigen

Header-only. No build step. Include the eigen/ directory in the project.

### Use Cases

- SE(3) transform arithmetic (Quaternion, AngleAxis, Transform).
- 4x4 homogeneous matrix multiplication for T_ir_to_headset.
- Jacobian computation for Ceres cost functions.

---

## 7. Ceres Solver

### Version

2.2.0

### Repository

https://github.com/ceres-solver/ceres-solver

### Dependencies

- Eigen 3.4.0 (required)
- glog 0.6.0 (required, or use MINIGLOG if glog is unavailable on Android)
- SuiteSparse (optional, improves sparse solver performance)

### Build for Android

```bash
git clone https://github.com/ceres-solver/ceres-solver.git
cd ceres-solver
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33 \
  -DEIGEN_INCLUDE_DIR=../eigen \
  -DMINIGLOG=ON \
  -DBUILD_TESTING=OFF \
  -DBUILD_EXAMPLES=OFF
cmake --build build-android --parallel
```

### Use in Calibration

Ceres provides the Levenberg-Marquardt solver used to minimize reprojection error:

```
min sum_i || p_rgb_i - project(T_ir_to_headset, P_3D_i) ||^2
```

---

## 8. libuvc

### Version

0.0.7 (latest stable)

### Repository

https://github.com/libuvc/libuvc

### Build for Android

libuvc requires libusb. Use libusb-android:

```bash
git clone https://github.com/libusb/libusb.git
# Build libusb for Android arm64-v8a first

git clone https://github.com/libuvc/libuvc.git
cd libuvc
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DLIBUSB_INCLUDE_DIR=../libusb/libusb \
  -DLIBUSB_LIBRARY=../libusb/android/libs/arm64-v8a/libusb1.0.so
cmake --build build-android
```

### UVC Alternative: Kernel Driver

On rooted or custom AOSP builds, the in-kernel UVC driver (drivers/media/usb/uvc/) is available via /dev/video* device nodes. This provides lower latency than userspace libuvc and integrates with the V4L2 API.

---

## 9. Host Development Environment

Required on the build machine (Ubuntu 22.04 LTS preferred):

| Tool | Version | Install |
|------|---------|---------|
| repo | latest | curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo |
| Git | 2.34+ | apt install git |
| Python | 3.10+ | apt install python3 |
| CMake | 3.22+ | apt install cmake or snap install cmake |
| Ninja | 1.10+ | apt install ninja-build |
| ADB | latest | SDK platform-tools |
| Java (for AOSP) | OpenJDK 11 | apt install openjdk-11-jdk |
| Make | 4.3+ | apt install make |

Windows development with WSL2 (Ubuntu 22.04) is supported for NDK cross-compilation but AOSP builds require native Linux due to case-sensitive filesystem requirements.