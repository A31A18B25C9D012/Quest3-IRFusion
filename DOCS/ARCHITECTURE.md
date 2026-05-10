# System Architecture

## Overview

The system boots directly into a single-purpose infrared passthrough
runtime. There is no Android launcher or home screen. The user sees the
fused camera view the moment the headset starts. The underlying OS is a
stripped AOSP build kept solely to load Qualcomm hardware drivers. All
visible behavior is produced by the fusion service and the HUD.

---

## Boot and User Interface

On startup the AOSP init system launches the fusion service via init.rc.
The service initializes Vulkan, loads calibration, starts the IR UVC
stream, finds and opens the button input device, then enters the frame
loop. The user sees the fused IR passthrough immediately.

Navigation uses three physical buttons on the headset. The button input
thread writes single-byte commands into a pipe; the frame loop reads
that pipe once per frame and dispatches them through the state machine.

Normal view:

| Button | Action |
|--------|--------|
| Volume up | Cycle to next mode (0->1->2->3->4->0) |
| Volume down | Cycle to previous mode (0->4->3->2->1->0) |
| Power (short press) | Open settings page |
| Power (hold 2s) | Shut down the fusion service |

Settings page:

| Button | Action |
|--------|--------|
| Volume up | Move cursor up |
| Volume down | Move cursor down |
| Power (short press) | Activate selected row |
| Power (hold 2s) | Shut down the fusion service |

The settings page has four rows: MODE (cycles 0-4), RES (cycles
resolution preset and reinitializes the IR camera), HUD (toggles the
overlay), and EXIT (closes the settings page).

Mode changes are also accepted via the IPC socket at
/dev/socket/ir_fusion by writing a single digit 0-4.

---

## HUD State and Communication

HUDState lives on the CPU inside the FusionService struct. It is
updated each frame from three sources:

- IR camera callback: computes mean byte value of the raw IR buffer
  divided by 255, stores it as ir_intensity.
- Button input pipe: the button thread writes single-byte commands.
  Volume events update mode. Power hold sets the shutdown flag.
- Depth map: the center pixel disparity is converted to meters and
  stored as crosshair_dist.

At the end of each frame loop iteration hud_get_push_constants() packs
HUDState into a flat 48-byte HUDPushConstants struct. This is uploaded
to the GPU with a single vkCmdPushConstants call before the HUD shader
dispatch. No descriptor sets or buffer copies are involved.

The HUD shader runs one thread per pixel in 16x16 groups. Each thread
reads its coordinate from the composite image, evaluates whether the
coordinate falls inside any HUD element, and writes the result.

## HUD Element Math

IR intensity bar fill height:

    fill_y = bar_bottom - floor((bar_bottom - bar_top) * ir_intensity)

Caret segment hit test (integer, no division):

    len2  = dx*dx + dy*dy
    t     = clamp(ex*dx + ey*dy, 0, len2)
    nx    = ex*len2 - t*dx
    ny    = ey*len2 - t*dy
    hit   = 4*(nx*nx + ny*ny) <= 9*len2*len2   (threshold: 1.5 px)

Center pixel depth from stereo disparity:

    Z = (focal_length_px * baseline_m) / disparity_px

Pixel font bit extraction for digit d at column c, row r:

    bit = (FONT[d] >> ((5-r)*4 + (3-c))) & 1

Status box border check:

    border = (x == x0 || x == x1 || y == y0 || y == y1)

---

## Data Sources

### Stereo Visible Passthrough

- Provided by the Quest 3 camera HAL as two AHardwareBuffer frames.
- Resolution: 1280x960 per eye.
- Format: YUV420 / NV21 depending on HAL configuration.
- Depth: Reconstructed via SAD stereo block matching (~65mm baseline).

### External IR Frame

- Ingested via UVC driver (libuvc or kernel UVC module).
- Resolution: 160x120 (FLIR Lepton) or up to 1920x1080 (NIR CMOS).
- Frame rate: 8-9 Hz (LWIR), up to 120 Hz (NIR global shutter).
- Timestamp alignment: linear interpolation between IR frames to match
  RGB capture timestamps.
- IR illumination source for NIR: the Quest 3 built-in IR flood
  emitters and dot projector (~850nm). No external LEDs required.

### Spatial Data

- Stereo depth map computed from left/right RGB disparity.
- Used to modulate the IR overlay intensity by surface distance.

---

## Frame Fusion Pipeline

### Stage 1: Timestamp Alignment

IR frames are stored in a 32-slot ring buffer with hardware timestamps
from the UVC capture path. Each RGB frame is matched to the nearest IR
frame. If the gap exceeds half the IR frame period, the service
linearly interpolates between the two bracketing IR frames at the
pixel level.

### Stage 2: IR-to-Headset Extrinsic Transform

The IR sensor pose relative to the headset origin is stored as:

```
T_ir_to_headset = [ R_ir | t_ir ]
                  [  0   |  1   ]
```

Calibrated once with the host calibration tool. Optionally refined at
runtime by a diagonal 6-DOF Kalman filter when environmental feature
correspondences are detected in both streams.

### Stage 3: IR Pixel Projection (Warp Shader)

The inverse of the calibrated homography maps each output pixel
coordinate back to the source position in the IR frame:

```
p_rgb = H_inv * p_ir   (homogeneous coordinates)
```

The warp shader runs in 16x16 thread groups.

### Stage 4: Edge Detection (Edge Shader)

A 3x3 Sobel operator runs on the warped IR luminance and outputs a
scalar gradient magnitude image in a separate R32F image.

### Stage 5: Composite Shader

Per-pixel blend:

```
C_final = a * C_rgb + b * F(I_ir, depth) + g * E(edges)

F(I_ir, depth) = I_ir / (1 + depth * k)
```

Coefficients a, b, g, and k are selected from the active mode preset.

### Stage 6: HUD Shader

Reads the composite image and writes the HUD overlay into a separate
intermediate buffer. Draws the four status boxes in the bottom-left,
the caret reticle at screen center, the distance readout in the
bottom-right, and the IR intensity bar on the right edge. Output is
pure green (BRIGHT or DIM) on top of scene color. State is delivered
via a 48-byte push constant block updated once per frame.

### Stage 7: Lens Correction Shader

Applies a configurable radial warp with coefficients k1 and k2. The
OpenXR compositor handles per-pixel lens distortion for all submitted
layers, so k1 = k2 = 0 is the correct default. The shader is present
as a tunable stage for cases where display geometry correction is needed
outside the compositor path.

---

## Operating Modes

| Mode | a | b | g | Description |
|------|---|---|---|-------------|
| 0 | 1.0 | 0.0 | 0.0 | RGB passthrough only |
| 1 | 0.8 | 0.5 | 0.0 | IR blended over RGB (default) |
| 2 | 0.0 | 1.0 | 0.0 | IR only |
| 3 | 0.9 | 0.0 | 0.5 | IR edges overlaid on RGB |
| 4 | 0.8 | 0.4 | 0.1 | Depth-weighted IR blend |

---

## Latency Budget

Target end-to-end: under 20ms.

| Stage | Target |
|-------|--------|
| RGB capture to HAL buffer | 2-4ms |
| IR USB capture to buffer | 3-8ms |
| Timestamp alignment | 0.5ms |
| IR warp shader | 1-2ms |
| Edge shader | 0.5-1ms |
| Composite shader | 1-2ms |
| HUD shader | 0.5ms |
| Lens correction shader | 0.5ms |
| OpenXR composition | 2-4ms |
| Display scan-out | 2ms |
| Total | 13-24ms |

---

## Process and Privilege Model

| Process | Privilege | Function |
|---------|-----------|----------|
| camera_service (HAL) | system | Owns RGB camera buffers |
| ir_fusion_service | system | IR UVC stream, alignment, GPU dispatch |
| openxr_runtime (Monado) | system | Display composition |

All buffer passing uses AHardwareBuffer file descriptors to avoid copies.
