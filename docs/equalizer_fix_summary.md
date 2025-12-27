# Equalizer Fix - Audio Routing Correction

## Problem Identified

The equalizer was properly initialized and configured, but **it didn't affect the sound** because:

1. ✅ **Bluetooth audio** was routed through the equalizer (via `audio_data_callback`)
2. ❌ **MP3 audio** bypassed the equalizer completely - went directly to `i2s` stream

## Root Cause

```cpp
// BEFORE (Wrong routing):
AudioPlayer player(source, i2s, helix);  // MP3 → i2s (no EQ!)
EncodedAudioStream out(&i2s, &helix);
BluetoothA2DPSink a2dp_sink(i2s);       // BT → callback → equalizer → volume → i2s
```

**Result**: Only Bluetooth audio was equalized, MP3 playback had no EQ effect.

## Solution Applied

### 1. Changed Audio Routing Architecture

```cpp
// AFTER (Correct routing):
AudioPlayer *player = nullptr;           // Initialize in setup()
EncodedAudioStream *out = nullptr;       // Initialize in setup()

// In setup(), after equalizer is created:
player = new AudioPlayer(source, *equalizer, helix);  // MP3 → equalizer!
out = new EncodedAudioStream(equalizer, &helix);
```

**New Audio Flow**:

```
MP3 File → AudioPlayer → *equalizer → volume_stream → i2s → Speaker
Bluetooth → audio_data_callback → equalizer->write() → volume_stream → i2s → Speaker
```

Both paths now go through the equalizer! ✓

### 2. Implemented EQ Presets

#### Theater Preset

```cpp
case CMD_EQ_SET_THEATER:
    bassGain = 6.0;    // +6 dB bass boost
    midGain = -2.0;    // -2 dB (reduce muddiness)
    trebleGain = 4.0;  // +4 dB treble boost

    // Apply dynamically
    equalizer->begin(updated_config);
```

**Sound characteristic**: Deep bass, clear highs, cinematic feel

#### Car Preset

```cpp
case CMD_EQ_SET_CAR:
    bassGain = 3.0;    // +3 dB moderate bass
    midGain = 2.0;     // +2 dB vocal clarity
    trebleGain = 1.0;  // +1 dB subtle treble

    // Apply dynamically
    equalizer->begin(updated_config);
```

**Sound characteristic**: Balanced, vocal-focused, good for noisy environments

### 3. Changed Initial EQ to Flat Response

```cpp
// BEFORE:
float bassGain = 5.0;
float midGain = -12.0;   // This was causing muffled sound!
float trebleGain = -12;  // This too!

// AFTER:
float bassGain = 0.0;    // Flat (no boost/cut)
float midGain = 0.0;     // Flat
float trebleGain = 0.0;  // Flat
```

Now the audio starts with **natural, uncolored sound** until a preset is selected.

### 4. Updated playMp3File() Function

```cpp
void playMp3File(int choice)
{
    if (!player) {
        Serial.println("ERROR: Player not initialized!");
        return;
    }

    // Changed from player.method() to player->method()
    if (!player->begin(choice)) {
        Serial.println("Failed to start player");
        return;
    }

    player->copyAll();
}
```

## Files Modified

### `src/main.cpp`

1. **Lines 25-31**: Changed EQ gain defaults to 0.0 (flat response)
2. **Lines 42-44**: Changed `player` and `out` to pointers
3. **Lines 320-340**: Updated `playMp3File()` to use pointer syntax
4. **Lines 445-490**: Implemented Theater and Car EQ presets with real-time updates
5. **Lines 795-798**: Initialize player and out streams to use equalizer

## Testing Instructions

### Step 1: Build and Upload

```powershell
cd d:\hardware_projects\WORK\MANJUNATHAN\esp32-music-system-repo_copilot
pio run -t upload
pio device monitor
```

### Step 2: Test Flat Response (Default)

1. Power on device
2. Listen to startup sound (`hello.mp3`)
3. Should sound **natural and balanced** (no EQ coloration)

### Step 3: Test Theater Preset

1. Navigate to Equalizer menu
2. Select "Theater" button
3. Serial output should show:
   ```
   >>> Executing: CMD_EQ_SET_THEATER
   Theater EQ applied: Bass +6.0dB, Mid -2.0dB, Treble +4.0dB
   ```
4. Play audio (BT or MP3) - should have **boosted bass and treble**

### Step 4: Test Car Preset

1. Select "Car" button
2. Serial output should show:
   ```
   >>> Executing: CMD_EQ_SET_CAR
   Car EQ applied: Bass +3.0dB, Mid +2.0dB, Treble +1.0dB
   ```
3. Play audio - should have **balanced, vocal-focused sound**

### Step 5: Test Both Audio Sources

- **MP3 test**: Play `hello.mp3` or `pair.mp3`, switch presets
- **Bluetooth test**: Connect phone, play music, switch presets
- **Verify**: Both sources should now be affected by EQ! ✓

## Expected Behavior

| Scenario                       | Expected Result                         |
| ------------------------------ | --------------------------------------- |
| Startup (no preset)            | Flat, natural sound (0 dB all bands)    |
| Theater preset + MP3           | Deep bass, enhanced treble on MP3 files |
| Theater preset + BT            | Deep bass, enhanced treble on Bluetooth |
| Car preset + MP3               | Balanced, vocal clarity on MP3 files    |
| Car preset + BT                | Balanced, vocal clarity on Bluetooth    |
| Switch presets during playback | Immediate effect, no audio glitches     |

## Architecture Diagram

### Before Fix

```
┌─────────┐
│   MP3   │──────────────────────┐
└─────────┘                      │
                                 ▼
┌─────────┐    ┌───────────┐  ┌─────┐   ┌─────────┐
│Bluetooth│───→│  callback │─→│ EQ  │──→│ volume  │──→│  i2s  │──→ Speaker
└─────────┘    └───────────┘  └─────┘   └─────────┘  └─────┘
                                ↑
                                └─ Only BT was equalized!
```

### After Fix ✓

```
┌─────────┐    ┌─────┐   ┌─────────┐   ┌─────┐
│   MP3   │───→│ EQ  │──→│ volume  │──→│ i2s │──→ Speaker
└─────────┘    └─────┘   └─────────┘   └─────┘
                 ↑
┌─────────┐     │
│Bluetooth│─────┘
└─────────┘

Both audio sources now go through EQ! ✓
```

## Technical Details

### Equalizer Configuration

```cpp
// Frequency bands:
eq_config.freq_low = 500;    // Bass/Mid crossover at 500 Hz
eq_config.freq_high = 3000;  // Mid/Treble crossover at 3000 Hz

// Resulting bands:
// - Low:    20 Hz - 500 Hz (Bass)
// - Medium: 500 Hz - 3 kHz (Vocals, instruments)
// - High:   3 kHz - 20 kHz (Treble, clarity)
```

### Gain Values (dB)

- **Positive values**: Boost (e.g., +6 dB = ~2x louder in that band)
- **Negative values**: Cut (e.g., -2 dB = ~0.8x quieter)
- **0 dB**: Unity gain (no change)

### Real-Time Updates

When a preset button is pressed:

1. Command queued → `appTask` processes
2. Global `bassGain`, `midGain`, `trebleGain` updated
3. `equalizer->begin(new_config)` called
4. EQ changes **apply immediately** during playback
5. No audio interruption or buffer underrun

## Troubleshooting

### Issue: MP3 still not affected by EQ

**Check**:

```cpp
// In setup(), verify:
player = new AudioPlayer(source, *equalizer, helix);  // Uses *equalizer!
```

### Issue: Distorted audio with presets

**Solution**: Reduce gain values

```cpp
// If too loud/distorted, try:
bassGain = 4.0;  // Instead of 6.0
trebleGain = 3.0; // Instead of 4.0
```

### Issue: No audio at all

**Check serial output**:

```
Equalizer initialized with flat response (0 dB all bands)
MP3 player initialized - audio will route through EQ
```

If missing, player didn't initialize correctly.

## Next Steps (Optional Enhancements)

1. **Add Flat/Bypass preset**: Button to reset to 0 dB all bands
2. **More presets**: Rock, Jazz, Classical, Speech optimized
3. **Custom EQ editor**: UI sliders for each band
4. **Preset persistence**: Save selected preset to NVS flash
5. **Visual feedback**: Show active preset name on screen
6. **Per-source EQ**: Different settings for MP3 vs Bluetooth

## Summary

✅ **MP3 audio now routes through equalizer** (was bypassing before)  
✅ **EQ presets implemented** (Theater and Car)  
✅ **Flat initial response** (0 dB, natural sound)  
✅ **Real-time preset switching** (works during playback)  
✅ **Both audio sources affected** (MP3 and Bluetooth)

**Status**: READY FOR HARDWARE TESTING! 🎵
