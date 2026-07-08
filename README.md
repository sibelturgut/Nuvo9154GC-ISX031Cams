# GMSL2 Multi-Camera Hardware Synchronization Skew Report

This repository contains a technical justification report evaluating a measured 60 µs inter-camera frame-capture skew for an autonomous-vehicle perception use case. 

The evaluation targets a mixed surround-view, object-detection, and stereo camera system operating at highway speeds (up to 130 km/h). The report compiles hardware specifications, published production-AV dataset benchmarks, peer-reviewed sensor-fusion timing analysis, and a first-principles physical error-budget calculation to assess system-level tolerance.

### 📄 Document

**[Read the full Technical Justification Report (PDF)](camera-report.pdf)**

---
*Hardware Evaluated: Nuvo 9154GC + 4 Cameras (AC-ISX031 H60 3x, AC-ISX031 H120 1x) with Neousys PCIe-NPL54 frame grabber.*

> [!WARNING]
> The codes only work seamlessly together if there is no dropped frames. The sync.cpp is going to show dropped frames guesses but it will not account for it while counting the frames, meaning the frame numbers will increase as if there was no drop.
