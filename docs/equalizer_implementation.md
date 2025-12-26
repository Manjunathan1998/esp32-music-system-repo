# ESP32 Audio Equalizer Implementation Overview

## Goal

Implement a real-time audio equalizer with presets (Home, Car, Theater) for both Bluetooth A2DP and MP3 playback using the audio-tools library.

---

## Architecture Overview

### Current Audio Pipeline

```
Bluetooth A2DP:
  a2dp_sink(i2s) → I2S → DAC/Amplifier

MP3 Playback:
  AudioPlayer(source, i2s, helix) → I2S → DAC/Amplifier
```

### New Audio Pipeline with Equalizer

```
Bluetooth A2DP:
  a2dp_sink → FilteredStream<EQ> → I2S → DAC/Amplifier

MP3 Playback:
  AudioPlayer → FilteredStream<EQ> → I2S → DAC/Amplifier
```

---

## Implementation Strategy

### Option 1: Using audio-tools FilteredStream (Recommended)

The audio-tools library provides `FilteredStream` for applying audio effects in the pipeline.

**Advantages:**

- ✅ Built-in support for filter chains
- ✅ Works with both BT and MP3 sources
- ✅ Can use IIR/FIR filters
- ✅ Real-time processing with minimal latency

**Components Needed:**

1. `FilteredStream` - Audio stream wrapper with filtering
2. Biquad IIR filters or FIR filters for each band
3. Filter coefficients for each EQ preset

### Option 2: Using ESP-DSP Library

ESP32 has a hardware-accelerated DSP library with optimized filters.

**Advantages:**

- ✅ Hardware acceleration on ESP32
- ✅ Lower CPU usage
- ✅ Optimized for audio processing

**Disadvantages:**

- ⚠️ More complex integration
- ⚠️ Requires manual audio buffer processing

---

## Recommended Approach: audio-tools FilteredStream

### Step 1: Audio Pipeline Modifications

#### 1.1 Global Objects (add to main.cpp)

```cpp
// Audio objects
AudioInfo info(44100, 2, 16);
I2SStream i2s;

// Equalizer setup
FilteredStream<int16_t, float> filteredOutput(i2s);  // Filtered output to I2S

// Biquad filters for 5-band EQ (common bands)
// 60Hz, 250Hz, 1kHz, 4kHz, 16kHz
Filter<int16_t, float> *eqFilters[5];

// mp3 startup sound setup
MP3DecoderHelix helix;
AudioSourceSPIFFS source("/", ".mp3");
AudioPlayer player(source, filteredOutput, helix);  // Output to filtered stream
EncodedAudioStream out(&filteredOutput, &helix);

// BT A2DP - will need custom output wrapper
BluetoothA2DPSink a2dp_sink;  // Don't pass i2s here anymore
```

#### 1.2 Filter Initialization (in setup())

```cpp
void setupEqualizer() {
    // Initialize 5-band parametric EQ
    // Bass: 60Hz, Low-Mid: 250Hz, Mid: 1kHz, High-Mid: 4kHz, Treble: 16kHz

    for (int i = 0; i < 5; i++) {
        eqFilters[i] = new FilterBiquad<int16_t, float>();
    }

    // Add filters to the filtered stream
    for (int i = 0; i < 5; i++) {
        filteredOutput.addFilter(eqFilters[i]);
    }

    // Apply default preset (Home)
    applyEQPreset(EQ_PRESET_HOME);
}
```

### Step 2: EQ Presets Definition

#### 2.1 Preset Enum (add to globals.h)

```cpp
enum EQPreset {
    EQ_PRESET_FLAT,      // No adjustment (bypass)
    EQ_PRESET_HOME,      // Balanced, slight bass boost
    EQ_PRESET_CAR,       // Enhanced bass and treble for road noise
    EQ_PRESET_THEATER,   // Cinema-like: deep bass, clear dialog
    EQ_PRESET_CUSTOM     // User-defined
};

// Global EQ state
EQPreset currentEQPreset = EQ_PRESET_HOME;
float eqGains[5] = {0, 0, 0, 0, 0};  // Current gains in dB
```

#### 2.2 Preset Configurations

```cpp
// EQ band frequencies (Hz)
const float EQ_FREQUENCIES[5] = {60, 250, 1000, 4000, 16000};

// Preset gain values in dB for each band
// [60Hz, 250Hz, 1kHz, 4kHz, 16kHz]
const float EQ_PRESET_VALUES[5][5] = {
    // FLAT
    {0.0, 0.0, 0.0, 0.0, 0.0},

    // HOME - Balanced with slight warmth
    {+2.0, +1.0, 0.0, +0.5, +1.0},

    // CAR - V-shape for noisy environment
    {+4.0, +1.0, -2.0, +1.0, +4.0},

    // THEATER - Deep bass, clear mids for dialog, sparkle on top
    {+5.0, +2.0, +1.0, -1.0, +3.0},

    // CUSTOM - User adjustable (initialize as flat)
    {0.0, 0.0, 0.0, 0.0, 0.0}
};
```

### Step 3: Filter Coefficient Calculation

#### 3.1 Biquad Filter Function

```cpp
void applyEQPreset(EQPreset preset) {
    currentEQPreset = preset;

    // Copy preset values
    for (int i = 0; i < 5; i++) {
        eqGains[i] = EQ_PRESET_VALUES[preset][i];
    }

    // Calculate and apply filter coefficients
    for (int i = 0; i < 5; i++) {
        float freq = EQ_FREQUENCIES[i];
        float gain = eqGains[i];
        float Q = 1.0;  // Bandwidth (1.0 = moderate)

        // Calculate biquad coefficients for peaking EQ
        setBiquadPeakingEQ(eqFilters[i], freq, gain, Q, 44100);
    }

    Serial.printf("EQ Preset applied: %s\n", getPresetName(preset));
}

void setBiquadPeakingEQ(Filter<int16_t, float>* filter,
                         float freq, float gain, float Q, float sampleRate) {
    // Peaking EQ filter coefficients
    float A = pow(10.0, gain / 40.0);  // Convert dB to linear
    float omega = 2.0 * PI * freq / sampleRate;
    float sn = sin(omega);
    float cs = cos(omega);
    float alpha = sn / (2.0 * Q);

    float b0 = 1.0 + alpha * A;
    float b1 = -2.0 * cs;
    float b2 = 1.0 - alpha * A;
    float a0 = 1.0 + alpha / A;
    float a1 = -2.0 * cs;
    float a2 = 1.0 - alpha / A;

    // Normalize
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;

    // Set filter coefficients
    filter->setCoefficients(b0, b1, b2, a1, a2);
}
```

### Step 4: Bluetooth A2DP Integration

The challenge with BT A2DP is that `BluetoothA2DPSink` expects direct I2S access.

#### Solution: Custom A2DP Data Callback

```cpp
// A2DP data callback - process through EQ
void a2dp_data_callback(const uint8_t *data, uint32_t length) {
    // Write data through filtered stream instead of directly to I2S
    filteredOutput.write(data, length);
}

void startBtSink() {
    Serial.println("Initializing Bluetooth sink...");

    // Configure A2DP to use our callback instead of direct I2S
    a2dp_sink.set_stream_reader(a2dp_data_callback, false);

    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playback_status_changed);
    a2dp_sink.set_auto_reconnect(true, 4);
    a2dp_sink.start("ESP32 Music");
    btSinkActive = true;

    Serial.println("Bluetooth sink with EQ active");
}
```

### Step 5: UI Integration

#### 5.1 Add Command for EQ Change (globals.h)

```cpp
enum AppCommand {
    // ...existing commands...
    CMD_EQ_SET_HOME,
    CMD_EQ_SET_CAR,
    CMD_EQ_SET_THEATER,
    CMD_EQ_TOGGLE
};
```

#### 5.2 EQ Screen Handler (main.cpp)

```cpp
// In appTask() switch statement
case CMD_EQ_SET_HOME:
    applyEQPreset(EQ_PRESET_HOME);
    break;

case CMD_EQ_SET_CAR:
    applyEQPreset(EQ_PRESET_CAR);
    break;

case CMD_EQ_SET_THEATER:
    applyEQPreset(EQ_PRESET_THEATER);
    break;

case CMD_EQ_TOGGLE:
    // Cycle through presets
    currentEQPreset = (EQPreset)((currentEQPreset + 1) % 4);
    applyEQPreset(currentEQPreset);
    break;
```

#### 5.3 Update EQ UI (LVGL Labels)

Add label objects in your EEZ Studio UI for:

- Current preset name
- Visual indicators for each preset button
- Optional: Band level visualization

---

## Performance Considerations

### CPU Usage

- **5-band biquad EQ**: ~2-5% CPU @ 44.1kHz stereo
- **Stereo processing**: Filters applied to both channels
- **Real-time latency**: <10ms additional latency

### Memory Usage

- **5 filters × 2 channels**: ~200 bytes per filter = ~1KB total
- **Filter state**: Minimal (5 float coefficients + 2 state variables per filter)

### Optimization Tips

1. Use `FilteredStream` with stereo mode enabled
2. Pre-calculate filter coefficients (don't recalculate in real-time)
3. Consider reducing to 3-band EQ if CPU is constrained
4. Use fixed-point math if floating-point is too slow

---

## Alternative: Simpler 3-Band EQ

If 5-band is too CPU-intensive:

```cpp
// 3-band EQ: Bass (100Hz), Mid (1kHz), Treble (10kHz)
const float EQ_FREQUENCIES[3] = {100, 1000, 10000};

const float EQ_PRESET_VALUES[4][3] = {
    {0.0, 0.0, 0.0},      // FLAT
    {+3.0, 0.0, +2.0},    // HOME
    {+6.0, -3.0, +5.0},   // CAR
    {+6.0, +2.0, +3.0}    // THEATER
};
```

---

## Testing Procedure

1. **Verify bypass mode** (FLAT preset) - No audio degradation
2. **Test each preset** - Listen for frequency changes
3. **Switch presets during playback** - Should be seamless, no clicks/pops
4. **CPU monitoring** - Ensure heap/CPU stays healthy
5. **BT and MP3** - Both sources should be equalized

---

## Files to Modify

1. **`src/globals.h`** - Add EQ enums, presets, global state
2. **`src/main.cpp`** - Add filter setup, preset functions, BT integration
3. **`src/ui/actions.h`** - Add EQ preset change actions
4. **EEZ Studio UI** - Add preset buttons to equalizer_page

---

## Next Steps

1. ✅ Add EQ globals and preset definitions
2. ✅ Implement filter initialization in setup()
3. ✅ Create preset switching functions
4. ✅ Integrate with BT A2DP stream
5. ✅ Update UI with preset buttons
6. ✅ Test and tune preset values

---

## References

- **audio-tools FilteredStream**: [GitHub - pschatzmann/arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
- **Biquad Filter Design**: Robert Bristow-Johnson's Audio EQ Cookbook
- **ESP32 Audio DSP**: ESP-DSP library documentation

---

## Notes

⚠️ **Important**: The current `BluetoothA2DPSink` implementation may need modification. Check if the library supports:

- Custom stream callbacks
- Output stream redirection
- Or use a buffer-based approach with manual I2S writing

💡 **Alternative**: If direct FilteredStream integration is complex, implement EQ as a separate processing task that reads from a buffer, processes, and writes to I2S.
