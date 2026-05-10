# Setup Guide

Step-by-step instructions for setting up the development environment,
building the software stack, and configuring hardware for the infrared
passthrough system.

---

## Prerequisites

- Ubuntu 22.04 LTS (native install preferred; WSL2 works for NDK builds
  but not full AOSP builds).
- 64GB RAM minimum for AOSP builds, 32GB minimum for NDK-only builds.
- 500GB free disk space.
- A Meta Quest 3 headset in developer mode.
- USB-C OTG hub.
- One IR camera from the hardware BOM.

---

## Phase 1: Host Machine Setup

### 1.1 Install System Packages

```bash
sudo apt update
sudo apt install -y \
  git python3 python3-pip curl wget unzip \
  cmake ninja-build make \
  openjdk-11-jdk \
  build-essential libssl-dev libffi-dev \
  pkg-config libusb-1.0-0-dev \
  adb fastboot
```

### 1.2 Install Android NDK

```bash
wget https://dl.google.com/android/repository/android-ndk-r26b-linux.zip
unzip android-ndk-r26b-linux.zip -d ~/android
export ANDROID_NDK=~/android/android-ndk-r26b
echo 'export ANDROID_NDK=~/android/android-ndk-r26b' >> ~/.bashrc
```

### 1.3 Install Android SDK Platform Tools

```bash
wget https://dl.google.com/android/repository/platform-tools-latest-linux.zip
unzip platform-tools-latest-linux.zip -d ~/android
export PATH=$PATH:~/android/platform-tools
echo 'export PATH=$PATH:~/android/platform-tools' >> ~/.bashrc
```

### 1.4 Install repo Tool

```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=$PATH:~/bin
```

---

## Phase 2: Quest 3 Developer Mode

### 2.1 Enable Developer Mode

1. Create a Meta developer account at developer.meta.com.
2. Open the Meta Quest mobile app on a paired phone.
3. Navigate to Menu -> Devices -> [your headset] -> Developer Mode.
4. Toggle Developer Mode on.
5. Restart the headset.

### 2.2 Enable USB Debugging

1. Connect the headset to the host machine via USB-C.
2. Put on the headset. A dialog asks to allow USB debugging.
3. Select Allow and check Always allow from this computer.

### 2.3 Verify ADB Connection

```bash
adb devices
# Expected: <serial>  device
```

### 2.4 Check USB OTG Support

```bash
adb shell cat /proc/config.gz | gunzip | grep CONFIG_USB_OTG
# Should output: CONFIG_USB_OTG=y or CONFIG_USB_GADGET=y
```

Connect the OTG hub, then:

```bash
adb shell lsusb
# Should list USB devices connected to the hub
```

---

## Phase 3: Build Dependencies

### 3.1 Build libusb for Android

```bash
cd ~/dev
git clone https://github.com/libusb/libusb.git
cd libusb
./autogen.sh
mkdir build-android && cd build-android
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33
make -j$(nproc)
export LIBUSB_DIR=~/dev/libusb/build-android
```

### 3.2 Build libuvc for Android

```bash
cd ~/dev
git clone https://github.com/libuvc/libuvc.git
cd libuvc
mkdir build-android && cd build-android
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DLIBUSB_INCLUDE_DIR=${LIBUSB_DIR}/../libusb \
  -DLIBUSB_LIBRARY=${LIBUSB_DIR}/libusb/libusb-1.0.so
make -j$(nproc)
export LIBUVC_DIR=~/dev/libuvc/build-android
```

### 3.3 Build OpenCV for the host calibration tool

```bash
sudo apt install -y libopencv-dev
```

Or build from source:

```bash
cd ~/dev
git clone https://github.com/opencv/opencv.git
cd opencv && git checkout 4.9.0
mkdir build-host && cd build-host
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
make -j$(nproc)
sudo make install
```

### 3.4 Build Monado (OpenXR Runtime)

```bash
cd ~/dev
git clone https://gitlab.freedesktop.org/monado/monado.git
cd monado
mkdir build-android && cd build-android
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33 \
  -DXRT_FEATURE_OPENXR=ON \
  -DXRT_BUILD_DRIVER_SURVIVE=OFF \
  -DXRT_BUILD_DRIVER_OPENHMD=OFF \
  -DXRT_BUILD_DRIVER_HYDRA=OFF \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
export OPENXR_DIR=~/dev/monado/build-android
```

---

## Phase 4: AOSP Build (Required for Camera HAL Access)

Skip if using the Meta OpenXR SDK path (app-level access only). This
phase is required for direct camera HAL buffer access.

### 4.1 Configure Git for AOSP

```bash
git config --global user.email "you@example.com"
git config --global user.name "Your Name"
git config --global color.ui false
```

### 4.2 Sync AOSP Source

```bash
mkdir ~/aosp && cd ~/aosp
repo init -u https://android.googlesource.com/platform/manifest \
     -b android-14.0.0_r1
repo sync -c -j8 --no-tags --no-clone-bundle
```

This downloads ~150GB and takes 2-6 hours.

### 4.3 Set Up Build Environment

```bash
cd ~/aosp
source build/envsetup.sh
lunch aosp_arm64-userdebug
```

### 4.4 Build AOSP

```bash
make -j$(nproc)
```

Build time: 2-6 hours.

### 4.5 Add Service Auto-start to init.rc

Edit the device init.rc to add:

```
service ir_fusion /data/local/tmp/ir_fusion_service \
    /data/local/tmp/calibration.json 0x289d 0x0010 320 200 9
    class main
    user root
    group root
    oneshot
```

Adjust the vendor ID, product ID, and resolution arguments to match
your IR camera.

### 4.6 Flash to Headset

```bash
adb reboot bootloader
fastboot flashall -w
```

Note: Flashing a custom AOSP build removes access to the Meta store
and proprietary OpenXR runtime. Keep a separate Quest 3 unit for
production testing if needed.

---

## Phase 5: Hardware Assembly

### 5.1 Mount IR Camera

1. Position the IR camera on the front face of the Quest 3, centered
   between the two RGB passthrough cameras.
2. Secure with M2 standoffs or a 3D-printed bracket. The mount must be
   rigid with no flex.
3. Route the USB-A cable to the OTG hub.
4. Record the physical offset in millimeters from the IR sensor center
   to the headset IMU origin. This is used as the initial estimate for
   t_ir_to_headset before calibration.

### 5.2 Connect OTG Hub

1. Connect the OTG hub to the Quest 3 USB-C port.
2. Connect the IR camera USB-A cable to the hub.
3. Connect the hub USB-C power input to the power bank.
4. Verify with adb shell lsusb that the IR camera appears.

No IR LED wiring is required. The Quest 3 built-in IR emitters on the
front panel illuminate the scene for NIR cameras. For LWIR thermal
cameras, no illumination is needed at all.

---

## Phase 6: Calibration

See CALIBRATION.md for the full procedure. After hardware assembly:

1. Run the intrinsic calibration for the IR camera.
2. Run stereo extrinsic calibration between IR and RGB.
3. Store the output JSON at /data/local/tmp/calibration.json on device.

---

## Phase 7: Build and Deploy Fusion Service

### 7.1 Build Shaders and Service

```bash
cd ~/dev/ir_fusion
NDK=~/android/android-ndk-r26b \
LIBUVC_LIB=~/dev/libuvc/build-android/libuvc.a \
LIBUSB_LIB=~/dev/libusb/build-android/libusb/libusb-1.0.a \
OPENXR_LIB=~/dev/monado/build-android/src/loader/libopenxr_loader.a \
make all
```

### 7.2 Preview HUD on Host

Build and run the HUD preview tool before deploying to verify HUD layout:

```bash
make hud_preview
./build/host/hud_preview [ir_intensity] [has_temp] [ir_on] \
    [crosshair_on] [crosshair_dist_m] [settings_open] \
    [settings_cursor] [mode] [ir_res_preset] [hud_enabled]
```

Output is written to hud_preview.bmp in the working directory. On
Linux open it with an image viewer. On Windows run tools/test_hud.sh
which builds and opens it automatically.

All arguments are optional. Defaults: ir_intensity=0.6, has_temp=0,
ir_on=1, crosshair_on=1, crosshair_dist=42, settings_open=0,
settings_cursor=0, mode=1, ir_res_preset=1, hud_enabled=1.

To preview the settings page with the cursor on the RES row:

```bash
./build/host/hud_preview 0.6 0 1 1 42 1 1
```

Adjust position constants in src/vulkan/shaders/hud.comp and rebuild
until the layout is correct before pushing shaders to the device.

### 7.3 Push and Start Service

```bash
make push
adb push build/host/calibration.json /data/local/tmp/calibration.json
adb shell /data/local/tmp/ir_fusion_service \
    /data/local/tmp/calibration.json 0x289d 0x0010 320 200 9
```

Adjust vendor/product IDs and resolution for your IR camera model:
- Seek Thermal Compact Pro: 0x289d:0x0010, 320x200, 9fps
- FLIR PureThermal 2: 0x1e4e:0x0100, 160x120, 8fps
- Arducam OV9281: 0x0bda:0x5830, 1280x720, 60fps (or as configured)

### 7.4 Verify Operation

```bash
adb logcat -s IRFusionService
```

Log output shows frame timestamps, IR frame rate, button device path,
and GPU shader dispatch times.

---

## Phase 8: HUD and Settings Verification

After the service is running on the device:

1. Observe the HUD overlay in the headset. Four outlined rectangles
   should appear in the bottom-left. The caret should appear at the
   center of the frame. The IR intensity bar should appear on the right
   edge and move as you point the headset at warm or bright surfaces.
   The distance readout should appear in the bottom-right and change as
   you move closer to or further from objects.
2. Press the power button briefly to open the settings page. The screen
   should darken and a centered panel with four rows (MODE, RES, HUD,
   EXIT) should appear with a `>` cursor on the first row. Volume up
   and down move the cursor. A brief power press activates the selected
   row. Verify that MODE cycles through 0-4, RES cycles through the
   three resolution presets and the IR stream reinitializes, HUD toggles
   the overlay, and EXIT returns to the normal view.
3. Cycle through modes outside the settings page using the volume
   buttons to verify the normal mode navigation still works. The third
   status box (THERMAL) should switch as thermal data becomes available
   or is lost.
4. If any HUD element is misaligned or the wrong size, edit the pixel
   position constants in src/vulkan/shaders/hud.comp, rebuild shaders
   with make shaders, re-embed with make service, and push with make
   push. Use the host hud_preview tool first to confirm the layout
   before pushing.
5. If the display geometry is distorted, adjust the k1 and k2 values
   in lens_coeffs_init_quest3() in src/distortion/lens_correction.c.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| adb devices shows no device | USB debugging not enabled | Re-check developer mode |
| lsusb does not show IR camera | OTG not active | Confirm developer mode; reboot headset |
| IR overlay misaligned | Calibration error | Rerun stereo calibration with more frames |
| Frame rate below 60 fps | GPU shader too slow | Reduce IR overlay resolution |
| USB IR camera disconnects | Power budget exceeded | Use a powered OTG hub |
| libuvc open fails | Camera claimed by kernel UVC driver | Check dmesg for uvcvideo errors |
| Button input not detected | Wrong event device | Run adb shell getevent -l to find button device |
| Volume buttons change system volume | Android consuming input | Add PROHIBITED_KEYS to custom AOSP build |
| Status boxes not visible | HUD shader push constant mismatch | Verify HUDPushConstants struct layout matches hud.comp PC block |
| Distance readout shows 000 | Center pixel disparity is zero | Check stereo calibration; minimum detectable depth depends on baseline and focal length |
| IR bar does not move | ir_intensity always zero | Confirm IR frame buffer pointer and byte count passed to hud_compute_ir_intensity |
| Caret not visible | crosshair_on flag not set | Call hud_state_set_crosshair with on=1 before first frame |
| Settings page does not open | Power short press not detected | Check button_input thread is running; verify power key event in adb shell getevent |
| Settings page cursor does not move | Volume events routed to Android | Add PROHIBITED_KEYS override in AOSP build; verify button pipe is open |
| Resolution change hangs | Camera reinit fails | Check dmesg for UVC errors; verify the new width/height are supported by the camera |
| HUD toggle in settings has no effect | hud_enabled flag not propagating | Confirm hud_get_push_constants copies hud_enabled to HUDPushConstants |