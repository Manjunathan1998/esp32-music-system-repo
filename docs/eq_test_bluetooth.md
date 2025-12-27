# Quick EQ Test - Bluetooth Audio

## What Changed (Minimal)

### 1. Set EQ to Obvious Test Values

```cpp
// In main.cpp, lines 28-30:
float bassGain = 8.0;   // Heavy bass boost
float midGain = -6.0;   // Mid cut (vocals reduced)
float trebleGain = 6.0; // Treble boost (crisp highs)
```

These extreme values will make it **very obvious** if the EQ is working.

### 2. Added Serial Debug Output

When the device starts, you'll see:

```
===================================
Equalizer initialized with TEST settings:
  Bass:   8.0 dB (boosted)
  Mid:    -6.0 dB (cut)
  Treble: 6.0 dB (boosted)
Expected sound: Heavy bass, reduced mids, bright highs
===================================
```

## How to Test

### Step 1: Upload Code

```powershell
pio run -t upload
pio device monitor
```

### Step 2: Test Bluetooth Audio

1. Power on the ESP32
2. Navigate to Bluetooth menu
3. Connect your phone
4. Play music (any song with vocals and bass)

### Step 3: Listen for These Effects

**If EQ is working, you should hear**:

- ✓ **Heavy, punchy bass** (+8 dB boost)
- ✓ **Reduced/muffled vocals** (-6 dB mid cut)
- ✓ **Bright, crisp highs** (+6 dB treble boost)
- ✓ Sound like a "V-shaped" frequency response

**If EQ is NOT working**:

- ✗ Normal, balanced sound
- ✗ Clear vocals (mids not cut)
- ✗ No excessive bass

### Step 4: Check Serial Output

Look for this in the serial monitor at startup:

```
Equalizer initialized with TEST settings:
  Bass:   8.0 dB (boosted)
  Mid:    -6.0 dB (cut)
  Treble: 6.0 dB (boosted)
```

## Current Architecture

**Bluetooth Audio Flow**:

```
Phone → Bluetooth → audio_data_callback → equalizer->write() → volume_stream → i2s → Speaker
                                              ↑
                                        EQ APPLIED HERE
```

**MP3 Audio Flow**:

```
SPIFFS → AudioPlayer → *equalizer → volume_stream → i2s → Speaker
                           ↑
                     EQ APPLIED HERE
```

Both paths go through the equalizer ✓

## If EQ is NOT Working

### Check Serial Monitor

Look for:

```
Equalizer initialized with TEST settings:
```

If missing, equalizer didn't initialize.

### Verify Bluetooth Callback

The callback should be set in `startBtSink()`:

```cpp
a2dp_sink.set_stream_reader(audio_data_callback, false);
```

### Check Equalizer Pointer

In `audio_data_callback()`:

```cpp
void audio_data_callback(const uint8_t *data, uint32_t len)
{
    equalizer->write(data, len);  // This should be called
}
```

Add debug print:

```cpp
void audio_data_callback(const uint8_t *data, uint32_t len)
{
    static int count = 0;
    if (count++ % 1000 == 0) {
        Serial.println("BT audio callback active");
    }
    equalizer->write(data, len);
}
```

## If EQ IS Working

You can then adjust to more reasonable values:

```cpp
// Theater preset:
float bassGain = 6.0;
float midGain = -2.0;
float trebleGain = 4.0;

// Car preset:
float bassGain = 3.0;
float midGain = 2.0;
float trebleGain = 1.0;

// Flat (neutral):
float bassGain = 0.0;
float midGain = 0.0;
float trebleGain = 0.0;
```

## Summary

✅ **Minimal changes made**  
✅ **Extreme EQ values** (+8, -6, +6 dB) for obvious audible effect  
✅ **Serial debug output** to confirm initialization  
✅ **Bluetooth audio routes through EQ** via callback  
✅ **MP3 audio routes through EQ** via AudioPlayer

**Test Result Expected**: Heavy bass, reduced mids, bright highs 🎵
