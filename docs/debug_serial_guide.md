# Debug Serial Output Guide

## Debug Messages Added

Comprehensive debug serial messages have been added throughout the encoder navigation and screen switching code to diagnose the button click issue on the main screen.

---

## Debug Message Categories

### 1. **Button Press Detection**

```
=== Button Released (rose) ===
DEBUG: Double-click detected!
  OR
DEBUG: First click - waiting for potential double-click
```

### 2. **Single-Click Timeout (600ms)**

```
DEBUG: Single-click timeout reached (600ms)
DEBUG: current_screen = [hex address]
DEBUG: objects.main = [hex address]
DEBUG: objects.equalizer_page = [hex address]
```

### 3. **Main Screen Focus Detection**

```
DEBUG: On main screen - checking main focus group
DEBUG: Focused object = [hex address]
DEBUG: Matched menu_buttons[0/1/2/3]
CMD_SWITCH_TO_SCR_BT / WIFI / EQ / SETTINGS
```

**Warning Messages:**

```
DEBUG: WARNING - Focused object didn't match any menu button!
DEBUG: WARNING - No focused object in main focus group!
```

### 4. **EQ Screen Focus Detection**

```
DEBUG: On EQ page - checking EQ focus group
DEBUG: Focused EQ object = [hex address]
DEBUG: Matched eq_buttons[0/1]
CMD_EQ_SET_THEATER / CMD_EQ_SET_CAR
```

**Warning Messages:**

```
DEBUG: WARNING - Focused object didn't match any EQ button!
DEBUG: WARNING - No focused object in EQ focus group!
```

### 5. **Command Queue**

```
DEBUG: Sending command to queue: [command number]
  OR
DEBUG: No command to send (CMD_NONE)
DEBUG: Single-click handling complete
```

### 6. **AppTask Command Execution**

```
>>> appTask: Received command: [command number]
>>> Executing: CMD_SWITCH_TO_SCR_MAIN / BT / EQ / etc.
>>> lv_async_call: Switching to [screen name] screen
```

### 7. **Screen Switching**

```
>>> switchToScreen called with target: [hex address]
>>> Current screen before switch: [hex address]
>>> Current screen after switch: [hex address]
>>> switchToScreen complete
```

### 8. **Focus Group Switching**

```
>>> switchToEQFocusGroup called
>>> EQ focus group active, btn_theater focused
  OR
>>> switchToMainFocusGroup called
>>> Main focus group active, a2dp_bluetooth focused
```

---

## Expected Output Sequences

### **Scenario 1: Single Click on Main Screen Button (Bluetooth)**

**Expected sequence:**

```
=== Button Released (rose) ===
DEBUG: First click - waiting for potential double-click

[600ms delay]

DEBUG: Single-click timeout reached (600ms)
DEBUG: current_screen = [0x3FFC1234]  <-- Should match objects.main
DEBUG: objects.main = [0x3FFC1234]
DEBUG: objects.equalizer_page = [0x3FFC5678]
DEBUG: On main screen - checking main focus group
DEBUG: Focused object = [0x3FFC9ABC]
DEBUG: Matched menu_buttons[0]
CMD_SWITCH_TO_SCR_BT
DEBUG: Sending command to queue: 2
DEBUG: Single-click handling complete

>>> appTask: Received command: 2
>>> Executing: CMD_SWITCH_TO_SCR_BT
[BT pairing sound and initialization]
>>> lv_async_call: Switching to BT screen
>>> switchToScreen called with target: [0x3FFC5678]
>>> Current screen before switch: [0x3FFC1234]
>>> Current screen after switch: [0x3FFC5678]
>>> switchToScreen complete
```

### **Scenario 2: Single Click on EQ Button (Theater)**

**Expected sequence:**

```
=== Button Released (rose) ===
DEBUG: First click - waiting for potential double-click

[600ms delay]

DEBUG: Single-click timeout reached (600ms)
DEBUG: current_screen = [0x3FFC5678]  <-- Should match objects.equalizer_page
DEBUG: objects.main = [0x3FFC1234]
DEBUG: objects.equalizer_page = [0x3FFC5678]
DEBUG: On EQ page - checking EQ focus group
DEBUG: Focused EQ object = [0x3FFCDEF0]
DEBUG: Matched eq_buttons[0]
CMD_EQ_SET_THEATER
DEBUG: Sending command to queue: 10
DEBUG: Single-click handling complete

>>> appTask: Received command: 10
>>> Executing: CMD_EQ_SET_THEATER
EQ: Theater preset selected
```

### **Scenario 3: Double-Click (from any screen)**

**Expected sequence:**

```
=== Button Released (rose) ===
DEBUG: First click - waiting for potential double-click

[Within 600ms]

=== Button Released (rose) ===
DEBUG: Double-click detected!

>>> appTask: Received command: 1
>>> Executing: CMD_SWITCH_TO_SCR_MAIN
>>> lv_async_call: Switching to main screen
>>> switchToScreen called with target: [0x3FFC1234]
>>> Current screen before switch: [0x3FFC5678]
>>> switchToMainFocusGroup called
>>> Main focus group active, a2dp_bluetooth focused
>>> Current screen after switch: [0x3FFC1234]
>>> switchToScreen complete
```

---

## Diagnostic Checklist

Use this checklist to diagnose issues from the serial output:

### **Problem: Nothing happens on button click**

Check for:

- [ ] Is `DEBUG: Single-click timeout reached` appearing after 600ms?
- [ ] Does `current_screen` match `objects.main`?
- [ ] Is there a focused object? (not NULL)
- [ ] Does the focused object match any `menu_buttons[]`?
- [ ] Is a command being sent to the queue?
- [ ] Is `appTask` receiving the command?
- [ ] Is `switchToScreen` being called?
- [ ] Is `current_screen` updating after switch?

### **Problem: Wrong screen shown**

Check for:

- [ ] Which command number is being sent?
- [ ] Does the command match the expected action?
- [ ] Is the target screen address correct?
- [ ] Is `current_screen` updating correctly?

### **Problem: EQ buttons not working**

Check for:

- [ ] Is `current_screen == objects.equalizer_page`?
- [ ] Is focus_group_eq returning a focused object?
- [ ] Do the focused objects match `eq_buttons[]`?
- [ ] Are EQ commands being sent to the queue?

---

## Command Number Reference

```cpp
CMD_NONE = 0
CMD_SWITCH_TO_SCR_MAIN = 1
CMD_SWITCH_TO_SCR_BT = 2
CMD_SWITCH_TO_SCR_WIFI_RADIO = 3
CMD_SWITCH_TO_SCR_EQ = 4
CMD_SWITCH_TO_SCR_SETTINGS = 5
CMD_BT_RESTART = 6
CMD_BT_STOP = 7
CMD_BAT_UPDATE = 8
CMD_SHUT_DOWN = 9
CMD_EQ_SET_THEATER = 10
CMD_EQ_SET_CAR = 11
```

---

## How to Use

1. **Upload the code** with debug messages
2. **Open Serial Monitor** at 115200 baud
3. **Perform the action** you want to test
4. **Copy the serial output**
5. **Compare with expected sequences** above
6. **Identify where the flow breaks**

This will pinpoint exactly where the button navigation is failing! 🔍
