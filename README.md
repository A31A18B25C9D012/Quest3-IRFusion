GOAL

The goal of this project is to turn a Meta Quest 3 into a standalone
night-vision and heat-vision headset. The Quest 3 has built-in infrared
emitters on its front face that it uses for controller and hand tracking.
By plugging in a near-infrared camera through the USB-C port, those
same emitters illuminate the scene and the camera picks up the light,
letting the user see in complete darkness. If a thermal camera is used
instead, the user can see heat gradients and body heat without any
illumination at all. The two camera streams are fused together so the
user gets a single clear image through the headset lenses.

The system boots directly into the fusion view. There is no Android home
screen, no launcher, nothing else. The user puts the headset on and
immediately sees the infrared passthrough with a HUD on top. Mode
changes and shutdown are controlled by the physical volume and power
buttons on the headset.


WHAT THIS IS

This is a real time sensor fusion system for a Meta Quest 3. It takes
the stereo visible-light passthrough cameras built into the headset and
merges them with a frame stream from an external infrared camera attached
via USB. The result is a composite image that displays in the headset
with the infrared data overlaid on the visible passthrough feed, giving
the effect of IR night vision or heat vision depending on the camera.

The system runs as a privileged service on a stripped AOSP build. It
does not replace the operating system kernel or hardware drivers. Those
must come from Android because the Snapdragon XR2 Gen 2 has no open GPU
or camera drivers. What it does replace is everything the user sees. The
Android framework loads the hardware drivers at boot, then the fusion
service starts and takes over the display. The user never interacts with
Android directly.

The infrared camera connects to the Quest 3 USB-C port via an OTG hub.
For near-infrared cameras, no external lighting is needed. The Quest 3
already emits 850nm infrared light from the emitters built into its
front face. A NIR camera with the IR cut filter removed picks that up
and images the environment. For thermal cameras, no illumination is
needed at all.


TARGET HARDWARE

The primary target is a Meta Quest 3 running a custom AOSP build in
developer mode. The compute SoC is a Snapdragon XR2 Gen 2 with an
Adreno GPU that supports Vulkan 1.1.

Two IR sensor types are supported. The first is a long-wave infrared
thermal camera such as a FLIR Lepton 3.5 on a PureThermal 2 breakout
board or a Seek Thermal Compact Pro. These detect emitted heat and
produce the heat-vision effect. The second is a near-infrared CMOS
camera such as the Arducam OV9281 with the IR cut filter removed. These
produce the night-vision effect using the Quest 3 built-in IR emitters
as the light source. No external LED hardware is required.


HOW THE PIPELINE WORKS

At startup the service loads a calibration file containing the intrinsic
parameters for each camera and the extrinsic transform that describes the
position and orientation of the IR sensor relative to the headset
coordinate frame. This transform is stored as a 4x4 rigid body matrix
in SE(3).

Each time a new RGB frame arrives, the service finds the IR frame whose
timestamp is closest to the RGB capture time. If the IR frame rate is
lower than the RGB frame rate, the service linearly interpolates between
the two bracketing IR frames at the pixel level.

The depth map is computed from the stereo RGB pair using
sum-of-absolute-differences block matching. The resulting pixel disparity
d is converted to depth Z using Z equals focal length times baseline
divided by d.

Five Vulkan compute shaders run in sequence. The first warps the IR
frame into the RGB camera coordinate frame using the inverse of the
calibrated homography. The second runs a Sobel edge detector on the
warped IR luminance. The third blends the RGB passthrough, warped IR,
and edge data into a composite image using the formula:

-    `C_final = alpha * C_rgb + beta * F(I_ir, depth) + gamma * E`

where F applies a depth falloff of 1 divided by (1 + depth * k) to
reduce the IR contribution at far surfaces. The fourth shader draws the
HUD overlay. The fifth applies an optional lens distortion correction
before the frame is submitted to the OpenXR compositor.


OPERATING MODES

Five modes are available. Volume up cycles forward, volume down cycles
backward. The current mode number is displayed in the settings page.

- Mode 0 passes the RGB frame through unmodified.
- Mode 1 blends the IR overlay onto the RGB feed. This is the default.
- Mode 2 displays only the IR frame.
- Mode 3 overlays IR edge features on the RGB feed.
- Mode 4 applies depth-weighted IR blending where the IR contribution
decreases with distance.

Modes are also selectable remotely by writing a digit 0 through 4 to
the IPC socket at /dev/socket/ir_fusion.


SETTINGS PAGE

A brief press of the power button opens the settings page. The display
darkens and a centered 300x200 panel appears with four rows:

-    MODE    current mode number (cycles 0-4 on select)
-    RES     three resolution boxes (320x240, 640x480, 1280x720), the active one filled in
-    HUD     ON or OFF
-    EXIT    close the settings page

Volume up moves the cursor up. Volume down moves the cursor down. A
brief power press activates the highlighted row. Selecting MODE cycles
to the next mode. Selecting RES cycles to the next resolution preset
and reinitializes the IR camera at that resolution without restarting
the service. Selecting HUD toggles the HUD overlay on and off. Selecting
EXIT closes the settings page and resumes normal view.

Holding the power button for two seconds shuts the service down from
any state, including while the settings page is open.


CALIBRATION

Calibration is done once on the host machine using the calibrate tool.
The user prints a 9x6 checkerboard with 30mm squares and holds it in
front of both cameras. The tool captures 15 to 25 stereo pairs, detects
corners using sub-pixel refinement, then runs OpenCV stereoCalibrate to
compute the rotation matrix and translation vector. It also computes a
3x3 homography used by the warp shader.

For LWIR cameras, the checkerboard must use thermal emissivity contrast
such as an aluminum plate with matte black tape squares.

A diagonal Kalman filter with a 6-dimensional state vector optionally
refines the transform at runtime when image correspondences are detected.


HUD

The HUD is a green overlay rendered by the fourth compute shader. It
has four elements.

Bottom-left: four stacked outlined rectangles. From top to bottom they
indicate CAMERAS active, INFRARED on, THERMAL present, and MODE active.
A bright outline means the subsystem is on. A dim outline means it is
off or not available.

Center: a small caret formed by two line segments meeting at the screen
center. The apex marks the optical axis. Volume buttons toggle the
caret on and off. When off, the center of the frame is completely clear.

Bottom-right: a three-digit meter readout showing the distance to
whatever is at screen center, computed from the stereo depth map at the
center pixel using Z = (focal_length * baseline) / disparity. Only
shown when the caret is on.

Right edge: a vertical fill bar whose height is proportional to the
mean luminance of the current IR frame.

HUD state is passed to the GPU as a 48-byte push constant block once
per frame. No descriptor sets or buffer copies are used for HUD data.


HUD TESTING WITHOUT THE HEADSET

The hud_preview host tool renders the HUD to a BMP file so the layout
can be checked without the headset:
```
    make hud_preview
    ./build/host/hud_preview [ir_intensity] [has_temp] [ir_on] \
        [crosshair_on] [crosshair_dist_m] [settings_open] \
        [settings_cursor] [mode] [ir_res_preset] [hud_enabled]
```

All arguments are optional. Defaults: ir_intensity=0.6, has_temp=0,
ir_on=1, crosshair_on=1, crosshair_dist=42, settings_open=0,
settings_cursor=0, mode=1, ir_res_preset=1, hud_enabled=1.

To preview the settings page with the cursor on the RES row:

-    `./build/host/hud_preview 0.6 0 1 1 42 1 1`

Output is hud_preview.bmp in the working directory. Run via
tools/test_hud.sh on Windows to open it automatically. Adjust
constants in src/vulkan/shaders/hud.comp and rebuild until the layout
is correct before pushing to the device.



BUILDING

Prerequisites are the Android NDK r26b or later, glslc from the LunarG
Vulkan SDK, Python 3, and a host GCC installation. The external libraries
libuvc, libusb, and the OpenXR loader must be cross-compiled for
arm64-v8a using the NDK toolchain before building.

Set NDK, LIBUVC_LIB, LIBUSB_LIB, and OPENXR_LIB in the Makefile or as
environment variables.

-    `make shaders`       - compile GLSL shaders to SPIR-V
-    `make service`       - cross-compile the fusion service binary
-    `make calibrate`     - build the host calibration tool
-    `make kernel_module` - build the kernel UVC bridge module
-    `make hud_preview`   - build the host HUD preview tool
-    `make all`           - build all of the above
-    `make push`          - push the service binary to the device via adb

You can also find more infromtation on setting everything up [here](https://github.com/A31A18B25C9D012/Quest3-IRFusion/DOCS/SETUP.md)

RUNNING

Run the host calibration tool first with both cameras visible to the
development machine. Transfer the output calibration.json to the device
at /data/local/tmp/calibration.json.

Start the service on the device:
```
    /data/local/tmp/ir_fusion_service \
        /data/local/tmp/calibration.json \
        <vendor_id> <product_id> <width> <height> <fps>
```

The service finds the button input device automatically, opens the IPC
socket, starts streaming, and runs until the power button is held for
two seconds or the process is killed.