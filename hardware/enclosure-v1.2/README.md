# CM5 Media Dongle — Enclosure v1.2 (fit-check)

## Files
- `cm5_dongle_v1_2.scad`: editable parametric OpenSCAD design. Set `part` to `bottom`, `top`, `assembly`, `board_template`, or `clearance_gauge`.
- `bottom.stl`, `top.stl`: prototype enclosure shells.
- `clearance_gauge.stl`: inexpensive 55.6 × 41.6 mm opening gauge to check the carrier-board footprint before printing the complete case.

## Nominal envelope
90 × 55 × 25 mm; 2 mm walls; 2.2 mm top/floor; nominal board footprint 55 × 41 mm; board underside 5.5 mm above the base. PETG recommended for initial prints; 0.2 mm layers, 3–4 perimeters. Print shells open-side-up and test supports/bridges in your slicer. Four nominal M2 fasteners, subject to hardware fit.

## Verified vs unverified
Waveshare publishes a 55 × 41 mm CM5-NANO-B board outline. Its product page lists **DC 5 V / 5 A** power input, mini HDMI, USB-C programming/power, USB-A, and RJ45. The earlier 5 V / 3 A assumption must not be used without measuring actual power draw and confirming manufacturer guidance.

**NOT verified:** board mounting-hole coordinates, assembled CM5 + carrier height, connectors' 3D coordinates, heatsink dimensions, connector orientation, cable plug strain relief, radio antenna keep-out, case thermal performance. Consequently the end openings are **provisional cable-access slots**, NOT validated mating connector cutouts. The carrier board includes RJ45 and USB-A which may require a larger housing or a redesigned carrier board. The 90 × 55 × 25 mm envelope may need to grow. Do not force the electronics into this shell.

## Next physical validation
1. Obtain board 3D model from https://www.waveshare.com/wiki/CM5-NANO-B under Resources → CM5-NANO-B 3D.
2. Check the actual CM5 + carrier + heatsink stack and RJ45/USB-A overhangs in CAD.
3. Print the clearance gauge; measure board and any mounting-hole coordinates with calipers.
4. Mark the exact HDMI and USB-C connector positions and revise OpenSCAD geometry.
5. Check wireless performance, temperatures and voltage under sustained streaming and gaming before closing the housing.

## References
- https://www.waveshare.com/product/cm5-nano-b.htm
- https://www.waveshare.com/wiki/CM5-NANO-B

This is an engineering concept for physical fit-check, not a certified or production-ready product.
