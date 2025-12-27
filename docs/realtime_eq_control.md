# Real-Time EQ Control via Serial

## Overview
You can now adjust the equalizer settings in real-time via the Serial Monitor while audio is playing!

## Serial Commands

### Set Bass (Low Frequencies)
```
bass <value>
```
Examples:
- `bass 6` - Boost bass by +6 dB
- `bass 0` - Flat bass (no boost/cut)
- `bass -3` - Cut bass by -3 dB

Alternative: `low <value>`

### Set Mid (Mid Frequencies)
```
mid <value>
```
Examples:
- `mid 2` - Boost mids by +2 dB (vocal clarity)
- `mid 0` - Flat mids
- `mid -6` - Cut mids by -6 dB (reduce vocals)

### Set Treble (High Frequencies)
```
high <value>
```
Examples:
- `high 4` - Boost treble by +4 dB (bright)
- `high 0` - Flat treble
- `high -3` - Cut treble by -3 dB (less harsh)

Alternative: `treble <value>`

### Show Current Settings
```
eq
```
Displays current EQ values and command help.

## Usage Example

### Step 1: Open Serial Monitor
```powershell
pio device monitor
```
Set baud rate to **115200**

### Step 2: Start Playing Audio
- Connect via Bluetooth and play music, OR
- Let MP3 startup sounds play

### Step 3: Adjust EQ in Real-Time
```
> eq
===== Current EQ Settings =====
  Bass:   -8.0 dB
  Mid:    -12.0 dB
  Treble: -12.0 dB
===============================

> bass 0
Bass set to: 0.0 dB
EQ updated!

> mid 0
Mid set to: 0.0 dB
EQ updated!

> high 0
Treble set to: 0.0 dB
EQ updated!

> eq
===== Current EQ Settings =====
  Bass:   0.0 dB
  Mid:    0.0 dB
  Treble: 0.0 dB
===============================
```

### Step 4: Test Different Presets

#### Theater Preset (Cinematic)
```
> bass 6
> mid -2
> high 4
```
Result: Deep bass, reduced mids, bright highs

#### Car Preset (Balanced)
```
> bass 3
> mid 2
> high 1
```
Result: Moderate bass, vocal clarity, subtle treble

#### Flat/Neutral
```
> bass 0
> mid 0
> high 0
```
Result: Natural, uncolored sound

#### Bass Boost
```
> bass 8
> mid 0
> high 2
```
Result: Heavy bass, neutral mids, slight treble

#### Vocal Focus
```
> bass -2
> mid 4
> high 1
```
Result: Reduced bass, enhanced vocals, clarity

## Frequency Bands

- **Bass (Low)**: 20 Hz - 500 Hz
  - Kick drums, bass guitar, low rumble
  
- **Mid**: 500 Hz - 3 kHz
  - Vocals, guitars, pianos, most instruments
  
- **Treble (High)**: 3 kHz - 20 kHz
  - Cymbals, high hats, clarity, "air"

## Recommended Ranges

- **Safe range**: -12 dB to +12 dB
- **Typical use**: -6 dB to +6 dB
- **Extreme values** may cause:
  - Distortion if too high
  - Inaudible if too low

## Testing Workflow

1. **Start with flat** (all 0 dB)
2. **Play music** with good bass, vocals, and highs
3. **Adjust one band at a time**:
   - Increase slowly: +2, +4, +6
   - Decrease slowly: -2, -4, -6
4. **Listen for changes**:
   - Bass: Do you feel more thump?
   - Mid: Are vocals louder/quieter?
   - Treble: Is it brighter/duller?
5. **Find your preferred sound**

## Quick Test Commands

### Test if Bass Control Works
```
bass -20
[wait 2 seconds - should sound muffled]
bass 10
[wait 2 seconds - should sound boomy]
bass 0
[back to normal]
```

### Test if Mid Control Works
```
mid -20
[vocals should disappear]
mid 10
[vocals should be very loud]
mid 0
[back to normal]
```

### Test if Treble Control Works
```
high -20
[should sound very dull, no clarity]
high 10
[should sound very bright, crisp]
high 0
[back to normal]
```

## Advantages of Real-Time Control

✅ **Instant feedback** - Hear changes immediately  
✅ **No recompile** - Adjust without uploading new code  
✅ **Fine-tuning** - Find perfect values for your speakers  
✅ **A/B testing** - Compare different settings quickly  
✅ **Learning** - Understand what each band affects  

## Implementation Details

The serial parser:
1. Reads command from Serial Monitor
2. Parses `<command> <value>` format
3. Updates global variables (`bassGain`, `midGain`, `trebleGain`)
4. Rebuilds EQ config with new values
5. Calls `equalizer->begin(eq_cfg)` to apply immediately
6. Works during playback without interruption

## Example Session

```
Device starting up...
Equalizer initialized with TEST settings:
  Bass:   -8.0 dB
  Mid:    -12.0 dB
  Treble: -12.0 dB

[User plays Bluetooth music]

> eq
===== Current EQ Settings =====
  Bass:   -8.0 dB
  Mid:    -12.0 dB
  Treble: -12.0 dB
===============================

> bass 6
Bass set to: 6.0 dB
EQ updated!

[Bass is now punchy and loud]

> mid 0
Mid set to: 0.0 dB
EQ updated!

[Vocals are now clear]

> high 4
Treble set to: 4.0 dB
EQ updated!

[Sound is now bright and clear]

> eq
===== Current EQ Settings =====
  Bass:   6.0 dB
  Mid:    0.0 dB
  Treble: 4.0 dB
===============================

Perfect! This is my Theater preset!
```

## Troubleshooting

### Commands not working
- Check Serial Monitor is connected (115200 baud)
- Make sure to press Enter after typing command
- Check for typos in command name

### No sound change
- Verify "EQ updated!" message appears
- Try extreme values (-20 or +20) to confirm it's working
- Check that audio is actually playing

### Distorted sound
- Reduce gain values (try +3 instead of +10)
- Don't boost all bands at once
- Consider reducing volume first

## Summary

Yes, adding the serial parser is **more than enough**! You can now:

1. ✅ Adjust bass, mid, and treble in real-time
2. ✅ Test different values without recompiling
3. ✅ Find the perfect EQ settings for your speakers
4. ✅ Experiment and learn what each band does

**Just type commands in the Serial Monitor and hear the changes instantly!** 🎵
