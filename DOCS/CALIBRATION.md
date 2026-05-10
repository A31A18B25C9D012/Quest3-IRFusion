# Calibration Procedure

This document covers intrinsic calibration for each camera, extrinsic calibration between the IR sensor and the headset camera coordinate frame, and runtime refinement.

---

## Prerequisites

- Quest 3 connected in developer mode via ADB.
- IR camera mounted in final position on the headset frame (no movement after calibration).
- OpenCV 4.8+ installed on development host.
- Python 3.10+ or C++ calibration tool built against OpenCV.
- Calibration target printed or prepared (see section 1).

---

## 1. Calibration Target Selection

### For NIR Cameras

A standard checkerboard pattern printed on matte paper works. NIR cameras under 850nm or 940nm illumination see ink contrast on paper.

- Board size: 9x6 inner corners (10x7 squares).
- Square size: 30mm per square for A4 paper, 40mm for A3.
- Print on matte paper. Glossy paper causes specular reflections under IR illumination.

### For LWIR Thermal Cameras

Standard paper checkerboards are not visible in LWIR. Thermal emissivity contrast is required.

- Option A: Aluminum plate with squares of matte black tape applied. Metal has low emissivity (~0.05); matte black tape has high emissivity (~0.95). The pattern appears clearly in a thermal frame.
- Option B: Heat the board from behind and use thin versus thick material to create temperature differentials.
- Square size: 40mm or larger due to low LWIR resolution (160x120 pixels).

---

## 2. Camera Intrinsic Calibration

Intrinsic calibration determines the focal length, principal point, and lens distortion coefficients for each camera independently.

### 2a. RGB Cameras (Quest 3)

The Quest 3 ships with factory intrinsics stored in the camera HAL characteristics. Read them using the Camera2 API:

```
CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraId);
float[] intrinsics = characteristics.get(CameraCharacteristics.LENS_INTRINSIC_CALIBRATION);
// [fx, fy, cx, cy, s]
int[] distortion = characteristics.get(CameraCharacteristics.LENS_DISTORTION);
```

If these values are unavailable or inaccurate for the passthrough mode, perform manual calibration:

1. Record 20-30 frames of the checkerboard from the passthrough camera feed at varied angles (tilt, rotation, distance).
2. Run `cv2.calibrateCamera()` with the collected image points and object points.
3. Target reprojection error below 0.5 pixels RMS.

### 2b. IR Camera

The IR camera has no factory intrinsics available. Manual calibration is required.

1. Fix the IR camera in position. Do not move it between steps.
2. Capture 20-30 frames of the calibration target from the IR camera.
   - For NIR: illuminate the target with the IR LED array. Use exposure settings that avoid saturation.
   - For LWIR: use the thermal target. Warm the board slightly to increase thermal contrast.
3. Detect corners in each frame:
   - NIR: `cv2.findChessboardCorners()` followed by `cv2.cornerSubPix()`.
   - LWIR: normalize frame to 8-bit, then use `cv2.findChessboardCorners()`.
4. Run `cv2.calibrateCamera()` on the collected IR frames.
5. Target reprojection error below 1.0 pixel RMS (LWIR at 160x120 is more tolerant due to low resolution).

Store results as:
```
K_ir  = [[fx, 0, cx],
          [0, fy, cy],
          [0,  0,  1]]
dist_ir = [k1, k2, p1, p2, k3]
```

---

## 3. Extrinsic Calibration (IR-to-Headset Transform)

Extrinsic calibration determines the rigid transform T_ir_to_headset, which maps 3D points from the IR sensor frame to the headset coordinate frame.

### 3a. Simultaneous Capture

Both the RGB passthrough camera and the IR camera must observe the calibration target at the same time.

- Mount the calibration target 0.5m to 1.5m in front of the headset.
- Trigger synchronized capture: record both streams with timestamps.
- Collect 15-25 stereo pairs at varied poses. Include translations along X, Y, Z and rotations up to 30 degrees.

### 3b. Corner Detection in Both Frames

For each capture pair:
1. Detect checkerboard corners in the RGB frame: `pts_rgb`.
2. Detect checkerboard corners in the IR frame: `pts_ir`.
3. Reject pairs where detection fails in either frame.

### 3c. Stereo Calibration

Run OpenCV stereo calibration using all accepted pairs:

```python
flags = cv2.CALIB_FIX_INTRINSIC  # Use previously computed K_rgb, K_ir
ret, K_rgb, dist_rgb, K_ir, dist_ir, R, T, E, F = cv2.stereoCalibrate(
    object_points,
    image_points_rgb,
    image_points_ir,
    K_rgb, dist_rgb,
    K_ir, dist_ir,
    image_size_rgb,
    flags=flags
)
```

Output:
- R: 3x3 rotation matrix from IR frame to RGB camera frame.
- T: 3x1 translation vector from IR origin to RGB origin, in RGB camera units (meters).

Construct T_ir_to_headset:

```
T_ir_to_headset = [ R | T ]
                  [ 0 | 1 ]
```

Target RMS reprojection error below 1.5 pixels after stereo calibration.

### 3d. Homography Fallback

If full extrinsic calibration is not available, compute a homography for approximate planar alignment:

```python
H, mask = cv2.findHomography(pts_ir, pts_rgb, cv2.RANSAC, ransacReprojThreshold=3.0)
```

H is a 3x3 matrix used to warp the IR frame into the RGB frame plane. This works acceptably for targets at similar distances but fails when scene depth varies significantly. Use full extrinsic calibration for production quality.

---

## 4. Validation

After calibration, verify alignment quality:

1. Position a heat source or IR-reflective object in the headset field of view.
2. Display the warped IR overlay on the RGB passthrough.
3. Check that the IR overlay aligns with visible edges of objects to within 3-5 pixels at the center of the frame.
4. Check alignment at frame corners. Distortion errors accumulate at the periphery.

Reprojection error formula:

```
error = (1/N) * sum( || p_rgb_measured - project(T_ir_to_headset, P_3D) || )
```

Where P_3D are 3D points from the stereo depth map and p_rgb_measured are the corresponding pixel locations in the RGB frame.

---

## 5. Storing Calibration Data

Store calibration results in a JSON file loaded at system startup:

```json
{
  "K_rgb_left": [[fx, 0, cx], [0, fy, cy], [0, 0, 1]],
  "dist_rgb_left": [k1, k2, p1, p2, k3],
  "K_rgb_right": [[fx, 0, cx], [0, fy, cy], [0, 0, 1]],
  "dist_rgb_right": [k1, k2, p1, p2, k3],
  "K_ir": [[fx, 0, cx], [0, fy, cy], [0, 0, 1]],
  "dist_ir": [k1, k2, p1, p2, k3],
  "R_ir_to_headset": [[r00, r01, r02], [r10, r11, r12], [r20, r21, r22]],
  "t_ir_to_headset": [tx, ty, tz],
  "H_ir_to_rgb": [[h00, h01, h02], [h10, h11, h12], [h20, h21, h22]],
  "baseline_mm": 65.0,
  "calibration_date": "2025-01-01"
}
```

---

## 6. Runtime Refinement

After initial calibration, the system optionally refines T_ir_to_headset at runtime using a Kalman filter when environmental correspondences are detected.

State vector:
```
x = [rx, ry, rz, tx, ty, tz]   (rotation vector + translation)
```

Prediction step: propagate state using IMU delta if available, otherwise hold constant.

Update step: when a strong gradient feature appears in both the IR frame and the RGB frame at the same location, compute the residual and update the state.

This handles slow thermal drift or minor physical shifts in the sensor mount over time. It does not replace the initial calibration.

---

## Recalibration Triggers

Recalibrate when:
- The IR sensor is remounted or its position changes.
- Reprojection error during validation exceeds 5 pixels.
- The headset undergoes a firmware update that modifies camera intrinsics.
- Ambient temperature change exceeds 20 degrees Celsius (thermal expansion can shift sensor position).