# ESP32 Music System - Startup Sound Issue Analysis & Fix

## Problem Summary

The MP3 startup file (hello.mp3) was not audible on system startup, even though all other audio functionality (Bluetooth A2DP, other MP3 files) was working correctly.

## Root Causes Identified

### 1. Missing I2S Pin Configuration in setup()

**Issue:** In `setup()`, the I2S was initialized WITHOUT pin configurations:

```cpp
auto cfg = i2s.defaultConfig(TX_MODE);
cfg.copyFrom(info);  // Only copied sample rate, channels, bits
cfg.buffer_count = 4;
cfg.buffer_size = 64;
i2s.begin(cfg);  // Pins were NEVER set!
```

**Correct approach** (as done in `startBtSink()`):

```cpp
auto cfg = i2s.defaultConfig(TX_MODE);
cfg.pin_bck = I2S_BCK;      // Required
cfg.pin_ws = I2S_WS;        // Required
cfg.pin_data = I2S_DATA;    // Required
cfg.copyFrom(info);
i2s.begin(cfg);
```

### 2. No Startup Sound Playback Call

**Issue:** The code never actually called `playMp3File(1)` to play hello.mp3 on startup. The audio player and source were initialized globally, but never used during boot.

### 3. SPIFFS Mounting Order

**Issue:** SPIFFS was mounted AFTER I2S initialization. While the global `AudioSourceSPIFFS` object was created before, it's better to mount SPIFFS first to ensure filesystem is ready.

## Solutions Implemented

### Fix 1: Added Pin Configuration to I2S Setup

Modified `setup()` to include pin assignments:

```cpp
auto cfg = i2s.defaultConfig(TX_MODE);
cfg.pin_bck = I2S_BCK;
cfg.pin_ws = I2S_WS;
cfg.pin_data = I2S_DATA;
cfg.copyFrom(info);
cfg.buffer_count = 4;
cfg.buffer_size = 64;
i2s.begin(cfg);
```

### Fix 2: Moved SPIFFS Mount Before I2S Init

Ensures filesystem is ready before audio initialization.

### Fix 3: Added Startup Sound Playback

After all tasks are created and UI is initialized, added:

```cpp
Serial.println("Playing startup sound...");
vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay to ensure I2S is fully ready
playMp3File(1); // Play hello.mp3
Serial.println("Startup sound playback completed");
```

## Why This Works

1. **Pin configuration** allows I2S to actually output audio to the hardware DAC/amplifier
2. **SPIFFS mounted first** ensures audio files are accessible
3. **Startup sound plays** after all initialization is complete, with a small delay to ensure I2S is fully stabilized
4. **I2S object reuse** is preserved - it's initialized once and reused across different audio sources (startup sounds, BT A2DP), which is the correct approach for stability

## Important Note on I2S Management

✅ **CORRECT:** Initialize I2S once in setup() with proper pin config, then reuse the same object

- Startup sounds use: `player` → `i2s`
- BT audio uses: `a2dp_sink(i2s)` → same `i2s` object
- When switching modes: `i2s.end()` then `startBtSink()` reinits with same object

❌ **WRONG:** Creating multiple I2S objects or reinitializing from scratch repeatedly causes instability

## Audio Crackling Issue - Second Fix

### Problem

After the initial fix, the startup sound was audible but had crackling/distortion artifacts.

### Root Causes of Crackling

1. **Insufficient I2S Buffer Size**

   - `buffer_count = 4` and `buffer_size = 64` were too small
   - This caused buffer underruns during audio playback
   - At 44.1kHz stereo 16-bit, buffers were only ~1.5ms of audio data

2. **Task Interference**

   - RTOS tasks (uiTask with lv_timer_handler, encoderTask) were created BEFORE audio playback
   - These tasks competed for CPU cycles during startup sound playback
   - LVGL's UI rendering interrupted audio streaming

3. **Insufficient Stabilization Time**
   - Only 100ms delay before playback wasn't enough for I2S hardware to fully stabilize

### Solutions for Crackling

#### Fix 1: Increased I2S Buffer Sizes

```cpp
cfg.buffer_count = 8;      // Increased from 4 (100% increase)
cfg.buffer_size = 512;     // Increased from 64 (8x increase)
```

**Benefits:**

- More audio buffering prevents underruns during task switching
- ~46ms of audio buffer (8 buffers × 512 bytes / 4 bytes per sample / 44100 Hz)
- Smoother playback even with task scheduling delays

#### Fix 2: Task Creation Order Optimization

Changed from:

```cpp
// OLD: Tasks created first
xTaskCreatePinnedToCore(uiTask, ...);
xTaskCreatePinnedToCore(encoderTask, ...);
playMp3File(1);  // Sound plays with tasks running
```

To:

```cpp
// NEW: Sound plays first, then tasks created
playMp3File(1);  // Sound plays without task interference
xTaskCreatePinnedToCore(uiTask, ...);
xTaskCreatePinnedToCore(encoderTask, ...);
```

**Benefits:**

- No task interference during critical startup sound playback
- setup() has full CPU resources for audio streaming
- Clean, uninterrupted audio buffer filling

#### Fix 3: Extended Stabilization Delays

```cpp
vTaskDelay(200 / portTICK_PERIOD_MS); // Before playback (was 100ms)
playMp3File(1);
vTaskDelay(100 / portTICK_PERIOD_MS); // After playback (new)
```

**Benefits:**

- I2S hardware fully stabilizes before first audio samples
- Clean transition after audio completes before task creation

### Buffer Size Considerations

| Configuration   | Buffer Time | Use Case                        |
| --------------- | ----------- | ------------------------------- |
| 4 × 64 bytes    | ~1.5ms      | ❌ Too small - causes crackling |
| 8 × 512 bytes   | ~46ms       | ✅ Good for startup sounds      |
| 16 × 1024 bytes | ~186ms      | ⚠️ Overkill - wastes RAM        |

**Chosen:** 8 × 512 bytes balances smooth playback with reasonable memory usage

## Testing Checklist (Updated)

- [ ] Startup sound (hello.mp3) plays on boot **without crackling**
- [ ] Audio quality is clean and smooth
- [ ] BT pairing sound (pair.mp3) plays when entering BT mode
- [ ] BT audio streaming works correctly
- [ ] Volume control works in BT mode
- [ ] System remains stable (no crashes/reboots)
- [ ] Shutdown sound (bye.mp3) plays before deep sleep
- [ ] No audio artifacts or distortion

## Performance Notes

- **Memory usage:** I2S buffers now use ~4KB RAM (8 × 512 bytes per buffer × 2 for DMA)
- **Latency:** ~46ms audio latency is acceptable for system sounds
- **CPU load:** Reduced during startup sound by deferring task creation
- **Stability:** Maintained - I2S still initialized once and reused

## Files Modified

- `src/main.cpp` - Fixed I2S initialization, buffer sizes, task ordering, and added startup sound playback

---

## Next Feature: Audio Equalizer Implementation

### Overview Created

Comprehensive documentation for implementing a real-time audio equalizer with presets (Home, Car, Theater) has been created in:

- `docs/equalizer_implementation.md` - Full technical specification
- `docs/equalizer_quickstart.md` - Quick implementation guide

### Implementation Approach

**Audio Pipeline Modification:**

```
CURRENT:  Source → I2S → Output
NEW:      Source → FilteredStream (EQ) → I2S → Output
```

**Key Components:**

1. **FilteredStream** from audio-tools library
2. **Biquad IIR filters** for frequency shaping
3. **3-band or 5-band EQ** (Bass, Mid, Treble or more detailed)
4. **Preset system** with pre-calculated coefficients

**Presets Defined:**

- **Flat**: No adjustment (0dB all bands)
- **Home**: +3dB bass, 0dB mid, +2dB treble - Warm and balanced
- **Car**: +6dB bass, -3dB mid, +5dB treble - V-shape for noisy environments
- **Theater**: +6dB bass, +2dB mid, +3dB treble - Cinema-like experience

### Integration Points

1. **Modified Objects (main.cpp):**

   - `FilteredStream<int16_t, float> filteredOutput(i2s)` - Wrap I2S
   - `AudioPlayer player(source, filteredOutput, helix)` - Use filtered output
   - `BluetoothA2DPSink a2dp_sink(filteredOutput)` - Use filtered output

2. **New Functions:**

   - `setupEqualizer()` - Initialize filters and default preset
   - `applyEQPreset(preset)` - Switch between presets
   - `setBiquadPeakingEQ(...)` - Calculate filter coefficients

3. **Global State (globals.h):**

   - `enum EQPreset` - Preset identifiers
   - `EQPreset currentEQPreset` - Active preset
   - `Filter<int16_t, float> *eqFilters[]` - Filter array

4. **UI Integration:**
   - Use existing `equalizer_page` screen
   - Add buttons/encoder selection for presets
   - Display current preset name

### Performance Impact

**CPU Usage:**

- 3-band EQ: ~3% additional CPU
- 5-band EQ: ~5% additional CPU

**Memory:**

- 3-band: ~600 bytes RAM
- 5-band: ~1KB RAM

**Latency:**

- <10ms additional latency (negligible for music playback)

### Implementation Steps

1. Add FilteredStream wrapper around I2S output
2. Create filter initialization function
3. Implement preset switching logic
4. Calculate biquad coefficients for each preset
5. Integrate with existing BT and MP3 audio paths
6. Add UI controls on equalizer page
7. Test and tune preset values

### Challenges to Address

**BluetoothA2DPSink Integration:**

- Current: `BluetoothA2DPSink a2dp_sink(i2s)` expects direct I2S
- Solution 1: Check if library supports FilteredStream
- Solution 2: Use custom data callback to redirect through FilteredStream
- Solution 3: Manual buffer processing with separate task

**Real-time Performance:**

- Must maintain 44.1kHz sample rate without dropouts
- I2S buffers may need slight increase
- Monitor CPU usage during EQ processing

### Testing Strategy

1. **Baseline Test**: FLAT preset should sound identical to no EQ
2. **Preset Comparison**: A/B test each preset
3. **Switching Test**: Change presets during playback (no clicks/pops)
4. **Source Test**: Verify EQ works on both BT and MP3
5. **Stability Test**: Long-duration playback with EQ active

---

## Documentation Structure

- `context.md` - This file: Project overview and changelog
- `docs/overview.md` - Original project overview
- `docs/equalizer_implementation.md` - Detailed EQ technical spec
- `docs/equalizer_quickstart.md` - Fast implementation guide
