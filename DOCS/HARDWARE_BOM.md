# Hardware Bill of Materials

All prices are approximate as of 2025 and subject to change. Verify
current pricing at the listed vendors before ordering.

---

## 1. Host XR Headset

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| XR Headset | Meta Quest 3 (128GB) | Meta Store, Amazon | $499 |
| XR Headset (alt) | Meta Quest 3 (512GB) | Meta Store, Amazon | $649 |

Notes:
- Quest 3 uses a Snapdragon XR2 Gen 2 SoC with Adreno GPU (Vulkan 1.1).
- USB-C port supports OTG in developer mode.
- Camera HAL access requires a custom AOSP build.
- The Quest 3 front panel has two built-in IR flood illuminators and a
  dot projector that emit ~850nm near-infrared light continuously for
  controller and hand tracking. These serve as the IR light source for
  NIR cameras and require no additional emitters.

---

## 2. Infrared Camera Modules

### Option A: Thermal (Long-Wave IR, 8-14 um)

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| LWIR sensor module | FLIR Lepton 3.5 | GroupGets, Digi-Key | $200 |
| Lepton breakout board | PureThermal 2 (USB) | GroupGets | $59 |
| Thermal USB camera | Seek Thermal Compact Pro | Amazon, Seek Thermal | $150 |

Notes:
- FLIR Lepton 3.5 outputs 160x120 at 8.6 Hz (radiometric) or 9 Hz
  (non-radiometric). Detects emitted heat, not reflected light.
- PureThermal 2 exposes the Lepton as a USB UVC device.
- Seek Compact Pro is 320x200 at 7-9 Hz, USB-C, UVC-compliant.
- LWIR does not use the Quest 3 IR emitters. It detects body heat and
  surface temperature differentials without any active illumination.

### Option B: Near-Infrared (NIR)

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| USB NIR camera (no IR cut filter) | Arducam B0205 (USB, OV9281, global shutter) | Arducam store, Amazon | $55 |
| USB NIR camera (alt) | ELP USB camera, 1080p, no IR cut | Amazon, AliExpress | $30-45 |

Notes:
- NIR cameras are standard CMOS sensors with the IR-cut filter removed.
- The Quest 3 built-in IR emitters provide enough illumination for the
  NIR camera to image the environment without any external LED hardware.
  No LED arrays, driver boards, or power banks are required for NIR.
- Global shutter (OV9281) eliminates motion blur compared to rolling
  shutter sensors.
- Resolution is higher than LWIR (up to 1080p vs 160x120).

### Recommendation

Use LWIR (Seek Compact Pro or Lepton + PureThermal 2) for heat-based
detection and temperature visualization. Use NIR (Arducam OV9281) for
high-resolution low-light passthrough. The Quest 3 IR emitters cover the
NIR operating wavelength, so an NIR build requires only the camera.

---

## 3. USB Interface and OTG Hardware

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| USB-C OTG hub | Anker 7-in-1 USB-C hub (USB-A x3, USB-C PD) | Amazon | $30-45 |
| USB-C OTG hub (alt) | Baseus 6-in-1 USB-C hub | Amazon | $25-35 |

Notes:
- The OTG hub connects to the Quest 3 USB-C port and provides USB-A
  ports for the IR camera.
- Quest 3 USB-C supports USB 3.2 Gen 1 (5 Gbps). Bandwidth is sufficient
  for one LWIR and one NIR stream simultaneously.

---

## 4. Power Supply

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| USB-C power bank | Anker 737 (26800mAh, 140W) | Amazon | $80-100 |
| USB current/voltage meter | UM25C USB power meter | Amazon | $15 |

Notes:
- The power bank is used to keep the Quest 3 charged during extended
  operation. The Quest 3 can draw from USB-C while in developer mode on
  most firmware versions.
- The IR camera is powered directly from the OTG hub USB bus.

---

## 5. IR Camera Mount Hardware

| Item | Vendor | Est. Price |
|------|--------|------------|
| M2/M3 standoffs and screws | Amazon | $8 |
| 3D printing filament (PETG or ABS for bracket) | Amazon | $20 |
| Jumper wires | Amazon, Adafruit | $5 |

Notes:
- The IR camera mounts on the front face of the Quest 3 between the two
  RGB passthrough cameras. The mount must be rigid with no flex to
  maintain calibration after the initial calibration run.

---

## 6. Calibration Hardware

| Item | Model | Vendor | Est. Price |
|------|-------|--------|------------|
| Checkerboard calibration target | A3 or A4 printed checkerboard (9x6, 30mm squares) | Print locally | Free |

Notes:
- Standard paper checkerboards work for NIR + RGB stereo calibration
  under Quest 3 IR illumination.
- LWIR thermal cameras cannot see printed checkerboards. The LWIR
  calibration target must have thermal emissivity contrast, such as
  an aluminum plate with squares of matte black tape applied.

---

## Total Estimated Cost

| Configuration | Est. Total |
|---------------|------------|
| NIR build (Quest 3 + Arducam OV9281 + OTG hub) | $585 |
| LWIR build (Quest 3 + Seek Compact Pro + OTG hub) | $680 |
| Dual-spectrum (Quest 3 + LWIR + NIR + hub) | $735 |

---

## Sourcing Summary

| Vendor | Best For |
|--------|----------|
| GroupGets (groupgets.com) | FLIR Lepton modules and PureThermal boards |
| Digi-Key (digikey.com) | Individual components, connectors |
| Adafruit (adafruit.com) | Breakout boards |
| Amazon | Consumer IR cameras, hubs, power banks |
| AliExpress | Low-cost IR camera modules (longer shipping) |