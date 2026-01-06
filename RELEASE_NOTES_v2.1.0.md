# HealthyPi 5 Firmware v2.1.0 Release Notes

**Release Date**: January 2025
**Zephyr Version**: 4.2-branch
**Previous Version**: v2.0.4

---

## Overview

This release focuses on fixing critical signal quality issues that caused spikes and dropped samples in ECG and respiration waveforms during USB streaming. The MAX30001 driver has been significantly improved for more reliable FIFO handling.

---

## Bug Fixes

### Fixed: ECG and Respiration Waveform Spikes

**Issue**: ECG and BioZ (respiration) waveforms displayed periodic sharp spikes and dropped samples, causing a "square wave" appearance during USB streaming.

**Root Causes Identified and Fixed**:

1. **FIFO Timing Mismatch**: The MNGR_INT register was configured with EFIT=1 (interrupt after 2 samples, ~15.6ms at 128 SPS) while the polling interval was 7ms. This caused many polls to find no data ready, resulting in zero-value samples being queued.

2. **PPG-Only Sample Queuing**: When no ECG data was available in a polling cycle, the code was queuing samples with `ecg_sample=0` and `bioz_sample=0`, creating the characteristic zero spikes in the waveform.

3. **Incorrect Sample Count Calculation**: The driver was reading the MNGR_INT configuration register (which stores the threshold setting) instead of reading enough samples and validating via ETAG/BTAG markers.

**Changes Made**:

- **MAX30001 MNGR_INT Configuration** (`drivers/sensor/max30001/max30001.c`):
  - Changed from `0x080000` (EFIT=1) to `0x000000` (EFIT=0, BFIT=0)
  - EINT now triggers after just 1 sample (~7.8ms), better matching the 7ms polling interval

- **FIFO Read Strategy** (`drivers/sensor/max30001/max30001_async.c`):
  - Read up to 4 ECG samples per poll (covers ~31ms at 128 SPS)
  - Read up to 2 BioZ samples per poll (covers ~31ms at 64 SPS)
  - ETAG/BTAG validation determines actual valid sample count
  - Added handling for invalid/unexpected tag values

- **Sample Queuing Logic** (`app/src/sampling_module.c`):
  - Removed PPG-only sample queuing that caused zero spikes
  - Only queue samples when ECG data is available
  - PPG values are retained and included with next ECG sample

### Fixed: USB Streaming Disconnect

**Issue**: USB streaming would stop after approximately 5 minutes of operation due to FIFO overflows.

**Fix**:
- Added FIFO overflow detection and automatic recovery
- Increased read buffer sizes to provide headroom for system delays:
  - ECG: 8 samples per read (up from 4)
  - BioZ: 4 samples per read (up from 2)
- The driver now monitors EOVF/BOVF status flags and resets the FIFO when overflow conditions are detected

---

## Technical Details

### MAX30001 Driver Improvements

The async sample fetch function now:
- Checks EINT/BINT status flags before attempting FIFO reads
- Reads larger buffers (8 ECG, 4 BioZ samples) to handle system delays
- Validates each sample using ETAG (ECG) and BTAG (BioZ) markers
- Properly handles FIFO empty (tag=0x06) and overflow (tag=0x07) conditions
- Stops reading on invalid tags to prevent garbage data

### BioZ Sample Interleaving

ECG runs at 128 SPS while BioZ runs at 64 SPS (2:1 ratio). The sampling module now properly distributes BioZ samples across ECG samples using proportional index calculation:

```c
bioz_idx = (i * n_samples_bioz) / n_samples_ecg;
```

This ensures smooth respiration waveforms without duplicate or missing samples.

### Code Cleanup

- Removed debug logging and diagnostic counters
- Simplified work handler and thread initialization
- Cleaned up commented-out code blocks
- Reduced unnecessary variable declarations

---

## Performance

### Resource Usage (RP2040: 264KB RAM, 1MB Flash)
- **Flash**: ~743KB (70.9%)
- **RAM**: ~261KB (96.6%)

### Sampling Rates
- **ECG**: 128 SPS (MAX30001)
- **BioZ/Respiration**: 64 SPS (MAX30001)
- **PPG**: ~50 SPS (AFE4400, read every 2nd cycle)
- **Polling Interval**: 7ms (143 Hz timer)

---

## Files Changed

### Modified
- `drivers/sensor/max30001/max30001.c` - MNGR_INT configuration
- `drivers/sensor/max30001/max30001_async.c` - FIFO read logic and validation
- `app/src/sampling_module.c` - Sample queuing and BioZ interleaving
- `app/VERSION` - Version bump to 2.1.0

---

## Upgrade Notes

### From v2.0.x
- Direct upgrade via UF2 flash
- No configuration changes required
- Settings are preserved

### Build Commands
```bash
# Standard ILI9488 display build
./make_ili9488.sh

# ST7796 display variant
./make_st7796.sh

# Flash via USB (drag-drop zephyr.uf2 to RPI-RP2 drive)
```

---

## Known Issues

- RAM usage remains high at 96.6% - minimal headroom for additional features
- BLE and USB CDC cannot run simultaneously (auto-switching mode)

---

**For support and updates**: https://github.com/Protocentral/healthypi5_zephyr
