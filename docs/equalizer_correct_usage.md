# Correct Usage of Equalizer3Bands with AudioTools Library

## Common Mistakes and Solutions

### Issue 1: Config Passed by Value (Most Critical!)
**Wrong:**
```cpp
auto eqcfg = eq.defaultConfig();  // Creates temporary copy
eqcfg.gain_high = .9;
eq.begin(eqcfg);  // Equalizer stores reference to temporary!
// eqcfg is destroyed here, equalizer has dangling reference
```

**Correct:**
```cpp
auto &eqcfg = eq.defaultConfig();  // Reference to internal config
eqcfg.gain_high = .9;
eq.begin(eqcfg);  // Equalizer uses its own internal config
```

**Or store as global:**
```cpp
Equilizer3BandsConfig eqcfg;  // Global variable

void setup() {
    eqcfg = eq.defaultConfig();
    eqcfg.gain_high = .9;
    eq.begin(eqcfg);
}
```

### Issue 2: Inconsistent Channel Count
Make sure ALL audio objects use the same number of channels.

### Issue 3: Inefficient Architecture
The equalizer should output directly to the I2S stream, not copied back.

---

## Corrected Minimal Sketch

```cpp
#include "AudioTools.h"
#include "AudioKitHAL.h"

// Audio objects
AudioKitStream kit;              // I2S hardware interface
Equilizer3Bands eq(kit);         // EQ wraps kit (outputs to kit)
StreamCopy copier(eq, kit);      // Copy FROM kit TO eq (input flows through EQ)

// Audio configuration constants
const int sample_rate = 44100;
const int channels = 2;          // MUST match kit configuration!
const int bits_per_sample = 16;

// Overall volume
float volumeControl = 0.98;

// Store EQ config globally to prevent dangling reference
Equilizer3BandsConfig eqcfg;

void setup() {
    Serial.begin(115200);
    delay(800);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);

    // Configure AudioKit (I2S)
    auto cfg = kit.defaultConfig(RXTX_MODE);
    cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;           // Set channels!
    cfg.bits_per_sample = bits_per_sample;
    
    // Minimize lag
    cfg.buffer_count = 2;
    cfg.buffer_size = 256;
    
    kit.begin(cfg);
    kit.setVolume(volumeControl);
    es8388_set_mic_gain(MIC_GAIN_12DB);

    // Configure Equalizer - CRITICAL: Use reference or global!
    eqcfg = eq.defaultConfig();
    eqcfg.sample_rate = sample_rate;
    eqcfg.channels = channels;           // MUST match kit!
    eqcfg.bits_per_sample = bits_per_sample;
    eqcfg.gain_low = 0.01;      // Bass
    eqcfg.gain_medium = 0.01;   // Mids
    eqcfg.gain_high = 0.9;      // Treble
    eqcfg.freq_low = 500;       // Bass/Mid crossover (Hz)
    eqcfg.freq_high = 3000;     // Mid/Treble crossover (Hz)
    
    // Initialize equalizer with stored config
    eq.begin(eqcfg);
    
    Serial.println("Setup complete - EQ active");
}

void loop() {
    copier.copy();           // Copy audio through EQ chain
    kit.processActions();    // Handle AudioKit button/actions
}
```

---

## Alternative: Use Reference (No Global Variable)

```cpp
void setup() {
    // ... kit setup ...
    
    // Get reference to internal config (stays valid)
    auto &eqcfg = eq.defaultConfig();
    eqcfg.sample_rate = sample_rate;
    eqcfg.channels = channels;
    eqcfg.bits_per_sample = bits_per_sample;
    eqcfg.gain_low = 0.01;
    eqcfg.gain_medium = 0.01;
    eqcfg.gain_high = 0.9;
    eqcfg.freq_low = 500;
    eqcfg.freq_high = 3000;
    
    eq.begin(eqcfg);  // Uses internal config, no dangling reference
}
```

---

## For Dynamic EQ Changes (During Playback)

```cpp
// Store config globally for runtime changes
Equilizer3BandsConfig eqcfg;

void setup() {
    // ... setup as above ...
}

void loop() {
    copier.copy();
    
    // Change EQ on the fly (safe because eqcfg is global)
    if (someCondition) {
        eqcfg.gain_low = 2.0;   // Boost bass
        eq.begin(eqcfg);         // Apply changes
    }
}
```

---

## Summary of Rules

1. **Use reference (`auto &`) OR store config globally** - Never use temporary local config
2. **Keep channels consistent** across all audio objects
3. **Set ALL parameters**: sample_rate, channels, bits_per_sample, gain values, crossover frequencies
4. **Architecture**: Input → Equalizer → Output (equalizer wraps the output stream)

---

## Error "Only 16 bits supported: [garbage]" means:
- Config was passed by value (temporary) and destroyed
- Equalizer holds dangling reference to destroyed config
- Accessing destroyed memory returns garbage values

**Fix:** Use `auto &eqcfg` or store config globally!
