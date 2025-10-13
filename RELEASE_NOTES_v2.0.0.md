# HealthyPi 5 Firmware v2.0.0 Release Notes

**Release Date**: January 2025  
**Zephyr Version**: 4.2-branch  
**Previous Version**: v1.1.3

---

## 🎉 Major Features

### Complete UI Redesign with Vital Statistics Detail Screens
- **New Home Screen**: Redesigned dashboard with all vital signs at a glance
- **Detail Screens**: Individual trend screens for HR, SpO2, RR, and Temperature
  - Real-time waveforms and trend charts
  - 120-second rolling statistics (min, max, average)
  - Historical data visualization
  - Touch navigation between screens
- **Welcome Screen**: Boot animation with product branding
- **Enhanced Graphics**: Custom fonts (Inter, Orbitron, JetBrains Mono) and icons
- **Modern Color Scheme**: Improved contrast and readability

### Improved Respiration Rate Algorithm
- **Adaptive Peak Detection**: Enhanced bioimpedance signal processing
- **Reliable 15-40 BPM Range**: Optimized for normal breathing patterns
- **Crash Fixes**: Resolved stability issues in RR calculation and display

### Bluetooth Low Energy (BLE) Support
- **Standard GATT Services**:
  - Heart Rate Service (0x180D)
  - Pulse Oximeter Service (0x1822)
  - Health Thermometer Service (0x1809)
- **Custom Services**:
  - ECG/Respiration Service (UUID 00001122-...)
  - PPG Service (UUID cd5c7491-...)
- **Auto-switching**: Seamless transition between BLE and USB CDC modes
- **Kconfig Option**: `CONFIG_HEALTHYPI_BLE_ENABLED` for build-time control

### Unified Data Format (OpenView3 Protocol)
- **Single-packet Serial Format**: Streamlined ECG, BioZ, and PPG data streaming
- **Protocol Buffers**: 
  - ECG/BioZ: 50 bytes (8 ECG + 4 BioZ samples)
  - PPG: 19 bytes (8 IR + 8 Red samples)
- **Multi-output**: USB CDC, BLE GATT, and SD card logging use same format

### Vital Statistics Tracking
- **Rolling Window**: 120-second continuous statistics
- **Per-vital Metrics**: Min, max, and average for HR, SpO2, RR, Temperature
- **Real-time Updates**: Statistics update live on detail screens
- **New Module**: `vital_stats.c` library for consistent calculations

---

## 🔧 Technical Improvements

### Zephyr RTOS Upgrade Path
- **v4.1.0 Migration**: Initial port from Zephyr 3.x to 4.1.0
- **v4.2 Final**: Upgraded to stable v4.2-branch
- **Breaking Changes**: Updated APIs for sensors, display, and Zbus messaging
- **Build System**: Modernized CMake configuration and module dependencies

### Enhanced Sensor Drivers
- **MAX30001 Improvements**:
  - Async API with trigger callbacks
  - Fixed ECG and BioZ streaming reliability
  - Lead-off detection support (hardware DC method)
  - FIFO management optimizations
- **AFE4400 Updates**: 
  - Synchronized PPG sampling at 100 Hz
  - Improved IR/Red LED timing
- **MAX30205**: Stable I2C temperature readings

### Display and LVGL Enhancements
- **LVGL 8.x**: Updated display library with better performance
- **24KB Memory Pool**: Optimized for complex UI with charts
- **Thread Safety**: Strict single-thread access to prevent crashes
- **ILI9488 & ST7796**: Support for both display controller variants

### Memory Optimization
- **Heap Management**: 32KB heap pool with careful allocation tracking
- **Buffer Sharing**: SpO2 algorithm buffers reused by data module (saved 4KB)
- **Feature Gating**: Optional BLE and display modules reduce memory footprint
- **Stack Tuning**: Right-sized thread stacks for 264KB RAM constraint

### Persistent Settings
- **Settings Module**: Zephyr settings subsystem integration
- **LittleFS Storage**: Internal flash for configuration persistence
- **FAT32 SD Card**: CSV logging with automatic file management

---

## 🐛 Bug Fixes

### Critical Stability Fixes
- **Display Crash Issues** (f19bf46, c50528c):
  - Fixed hard faults in LVGL update routines
  - Resolved thread safety violations in screen transitions
  - Added proper mutex protection for shared resources
  
- **RR Screen Crash** (c50528c):
  - Fixed memory access violation in respiration detail screen
  - Corrected chart buffer initialization

- **SpO2 Trend Issue** (0a01343):
  - Fixed SpO2 value not updating on trend screen
  - Resolved Zbus listener registration for SpO2 channel

### Sensor and Streaming Fixes
- **ECG and BioZ Streaming** (336e4ec):
  - Fixed data corruption in multi-channel sampling
  - Corrected SPI transaction timing for MAX30001
  - Resolved FIFO overflow conditions

- **Build Warnings** (47e08c3):
  - Cleaned up type mismatches and implicit conversions
  - Fixed deprecated API usage warnings

### Configuration and Build
- **Debug Logs** (42ddfc0): 
  - Disabled verbose logging for production builds
  - Reduced console spam from high-frequency Zbus listeners

---

## 📊 Performance Metrics

### Resource Usage (RP2040: 264KB RAM, 1MB Flash)
- **Flash**: ~689KB (65.7%) - includes fonts, images, and algorithms
- **RAM**: ~260KB (96.4%) - optimized for real-time operation
  - Heap: 32KB
  - LVGL pool: 24KB
  - Thread stacks: ~48KB total
  - Sensor buffers: ~20KB

### Real-time Performance
- **ECG/BioZ Sampling**: 125 Hz per channel (MAX30001)
- **PPG Sampling**: 100 Hz (AFE4400)
- **Temperature**: 1 Hz polling (MAX30205)
- **Display Updates**: 10 Hz (100ms refresh for live data)
- **USB Throughput**: ~5 KB/s full streaming (ECG+PPG+BioZ)

---

## 🔄 API and Architecture Changes

### Zbus Messaging Enhancements
- **New Channels**: 
  - `hr_chan` (Heart Rate)
  - `spo2_chan` (SpO2 and Perfusion Index)
  - `resp_rate_chan` (Respiration Rate)
  - `temp_chan` (Temperature)
  - `batt_chan` (Battery level)
- **Conditional Observers**: `HPI_OBSERVERS` macro for BLE/USB switching
- **Type Safety**: All messages use structs from `hpi_common_types.h`

### Module Architecture
- **Display Module** (`display_module.c`): 
  - Central screen manager with Zbus listeners
  - Thread-safe LVGL operations
  - Screen navigation state machine
  
- **Data Module** (`data_module.c`):
  - Unified data aggregation hub (918 lines)
  - OpenView3 protocol formatting
  - Multi-output routing (USB/BLE/SD)
  
- **Settings Module** (`settings_module.c`): 
  - New persistent configuration layer
  - Celsius/Fahrenheit preference
  - BLE name and advertising parameters

---

## 📁 File Changes Summary

### New Files Added (67,423 insertions)
- **UI Components**: 
  - `scr_hr.c`, `scr_spo2.c`, `scr_rr.c`, `scr_temp.c` - Detail screens
  - `scr_welcome.c` - Boot animation
  - `vital_stats.c/h` - Statistics library
  
- **Fonts**: 15 custom font files (Inter, Orbitron, JetBrains Mono)
- **Images**: 30+ icons and graphics for vitals display
- **Modules**: 
  - `settings_module.c/h` - Persistent settings
  - `ble_stubs.c` - BLE build-time stubs
- **Documentation**: `CLAUDE.md` - Extended developer guide

### Modified Files (1,600 deletions)
- **Removed Screens**: `scr_ecg.c`, `scr_ppg.c`, `scr_resp.c` (replaced with detail screens)
- **Major Refactors**:
  - `data_module.c` - Complete rewrite for unified format
  - `display_module.c` - Redesigned screen management
  - `spo2_process.c` - Enhanced SpO2 algorithm
  - `resp_process.c` - Improved respiration detection
  - `sampling_module.c` - Async sensor API integration

### Build System
- **New Scripts**: 
  - `make_ili9488.sh`, `make_st7796.sh` - Display-specific builds
  - `flash.sh` - Quick flash utility
  - `merge.sh` - MCUboot bootloader merging
- **Overlays**: Separate display and logger configuration files

---

## 🔬 Algorithm Improvements

### SpO2 Processing (`spo2_process.c`)
- **Maxim Algorithm**: Updated to latest reference implementation
- **Validity Detection**: Enhanced finger-off detection using algorithm flags
- **Buffer Management**: Exposed buffers for data module reuse (memory saving)
- **Perfusion Index**: Improved IR/Red signal quality metric

### Respiration Processing (`resp_process.c`)
- **Adaptive Thresholding**: Dynamic baseline adjustment for bioimpedance
- **Peak Detection**: Improved algorithm for breathing cycle identification
- **Rate Range**: Validated 15-40 BPM normal range
- **Noise Filtering**: Reduced false positives from movement artifacts

---

## ⚙️ Configuration Options

### New Kconfig Flags
```kconfig
CONFIG_HEALTHYPI_DISPLAY_ENABLED      # Enable LVGL display (default: y)
CONFIG_HEALTHYPI_BLE_ENABLED          # Enable Bluetooth (default: n)
CONFIG_HEALTHYPI_USB_CDC_ENABLED      # Enable USB serial (default: y)
CONFIG_HEALTHYPI_SD_CARD_ENABLED      # Enable SD logging (default: y)
```

### Build Configurations
- **Display Variants**: ILI9488 (default) and ST7796 overlays
- **Logger Options**: SD card CSV logging overlay
- **Debug Config**: `app/debug.conf` for verbose logging

---

## 📦 Dependencies

### Zephyr Modules (from `west.yml`)
- **zephyr**: v4.2-branch
- **cmsis-dsp**: DSP algorithms for signal processing
- **hal_rpi_pico**: RP2040 hardware abstraction layer
- **littlefs**: Internal flash filesystem
- **lvgl**: Display graphics library v8.x
- **mcuboot**: Bootloader for firmware updates

---

## 🧪 Testing and Validation

### Validated Features
- ✅ ECG streaming at 125 Hz (8 samples per packet)
- ✅ BioZ respiration at 125 Hz (4 samples per packet)
- ✅ PPG dual-wavelength at 100 Hz (8 IR + 8 Red per packet)
- ✅ SpO2 calculation with finger-off detection
- ✅ Respiration rate 15-40 BPM range
- ✅ Temperature reading in Celsius and Fahrenheit
- ✅ BLE GATT services (HR, SpO2, Temp, ECG, PPG)
- ✅ USB CDC OpenView3 protocol streaming
- ✅ SD card CSV logging
- ✅ 120-second rolling statistics
- ✅ All detail screens with charts and overlays

### Test Utilities
- **Python Script**: `tests/test_bioz_decode.py` - Validates BioZ data decoding
- **Runtime Rates**: Verified timing for all sampling and display updates

---

## 🚀 Upgrade Notes

### From v1.1.x
1. **Full reflash required**: Memory layout and bootloader changed
2. **Settings reset**: Persistent settings format updated
3. **BLE disabled by default**: Enable with overlay if needed
4. **USB CDC changes**: OpenView3 protocol replaces old format
5. **Display differences**: Home screen layout completely redesigned

### Build Commands
```bash
# Standard ILI9488 display build
./make_ili9488.sh

# ST7796 display variant
./make_st7796.sh

# Flash via debugger
west flash

# Create UF2 with bootloader (drag-drop to RP2040)
./merge.sh
```

---

## 🐞 Known Issues

### Memory Constraints
- RAM usage at 96.4% - minimal headroom for additional features
- `scr_all_trends.c` disabled in `app/CMakeLists.txt` to save memory
- Large font files consume significant flash space

### BLE Limitations
- BLE and USB CDC cannot run simultaneously (auto-switching mode)
- BLE reduces available RAM for other features
- Custom service UUIDs not yet standardized

### Display
- ST7796 variant less tested than ILI9488
- LVGL 8.x memory pool tuning may be needed for complex custom screens

---

## 📖 Documentation

### New Documentation Files
- **CLAUDE.md**: Comprehensive developer guide with architecture details
- **README.md**: Updated with v2.0 features and build instructions
- **RELEASE_NOTES_v2.0.0.md**: This file

### Developer Resources
- Zephyr API docs: https://docs.zephyrproject.org/
- LVGL docs: https://docs.lvgl.io/8/
- Project instructions: `.github/copilot-instructions.md`

---

## 🙏 Acknowledgments

This release represents a major architectural overhaul with contributions from:
- Zephyr 4.1/4.2 migration and stabilization
- Complete UI redesign with vital statistics
- Enhanced sensor drivers and algorithms
- BLE integration and protocol implementation
- Extensive testing and bug fixes

---

## 📝 Commit History

Key commits from v1.1.3 to v2.0.0:
- `42ddfc0` - Disable debug logs (production release)
- `e44b7e4` - Enable BLE support
- `c50528c` - Fix RR screen crash issue
- `f19bf46` - Fix display crash issues
- `d70faf6` - Update resp_process.c algorithm
- `2b13eab` - Working RR algorithm implementation
- `336e4ec` - Fix ECG and BioZ streaming
- `7858d5c` - Add Kconfig to disable BLE
- `0a01343` - Fix SpO2 trend issue
- `7793bad` - Fix trend screens
- `07a6ea7` - Add vitals detail screens
- `03636bd` - Remove realtime plot, add vital stats subscreens
- `585e026` - Update Zephyr to v4.2
- `be042a1` - Autoswitch between BLE and USB modes
- `fb29cbc` - Upgrade to Zephyr 4.1.0
- `47e08c3` - Fix build warnings
- `63bd74a` - Upgrade to Zephyr 4.1 branch
- `4209226` - Add welcome screen
- `11ce4d7` - Added unified data format

**Total Changes**: 152 files changed, 67,423 insertions(+), 1,600 deletions(-)

---

**For support and updates**: https://github.com/Protocentral/healthypi5_zephyr
