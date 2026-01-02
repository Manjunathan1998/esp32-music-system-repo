# Fix: Prevent CMD_SWITCH_TO_SCR_EQ When Already on EQ Page

## Problem

When clicking a button while already on the EQ page, the system was triggering `CMD_SWITCH_TO_SCR_EQ` instead of processing the EQ button commands (`CMD_EQ_SET_THEATER` or `CMD_EQ_SET_CAR`).

### Observed Behavior

```
Button fell
Short press
CMD_SWITCH_TO_SCR_EQ          ← Wrong! Already on EQ page
>>> switchToEQFocusGroup called
>>> EQ focus group active, btn_theater focused
```

## Root Cause

The encoder task logic was checking the **main menu focus group first**, which still had the "Equalizer" menu button in focus. This caused the system to detect a click on the main menu's EQ button and send `CMD_SWITCH_TO_SCR_EQ`, even though the user was already on the EQ page and trying to click an EQ preset button.

### Original Logic Flow (INCORRECT)

```
1. Button clicked
2. Check main menu buttons (focus_group) ← Finds EQ menu button
3. If no match, check EQ buttons (focus_group_eq)
4. Result: Always sends CMD_SWITCH_TO_SCR_EQ when on EQ page
```

## Solution

Modified the encoder task to **check the current screen first**, then process the appropriate button group based on which screen is active.

### New Logic Flow (CORRECT)

```
1. Button clicked
2. Check current_screen
3. If on EQ page → Check EQ buttons only (focus_group_eq)
4. If on main menu → Check main menu buttons only (focus_group)
5. Result: Correct commands based on context
```

## Code Changes

### File: `src/main.cpp` (encoderTask function)

**Before:**

```cpp
if (waitingForSecondClick && (millis() - lastClickTime >= doubleClickThreshold))
{
    // single click
    lv_obj_t *focused = lv_group_get_focused(focus_group);
    AppCommand cmd = CMD_NONE;
    if (focused)
    {
        // Check main menu buttons first
        for (int i = 0; i < 4; ++i) { ... }
    }

    // If no main button matched, check EQ buttons
    if (cmd == CMD_NONE)
    {
        focused = lv_group_get_focused(focus_group_eq);
        // Check EQ buttons
    }
}
```

**After:**

```cpp
if (waitingForSecondClick && (millis() - lastClickTime >= doubleClickThreshold))
{
    // single click
    AppCommand cmd = CMD_NONE;

    // Check if we're on the EQ page first
    if (current_screen == objects.equalizer_page)
    {
        // We're on EQ page, check EQ buttons only
        lv_obj_t *focused = lv_group_get_focused(focus_group_eq);
        Serial.print("DEBUG: On EQ page, focused object = ");
        Serial.println((uint32_t)focused, HEX);

        if (focused)
        {
            for (int i = 0; i < 2; ++i)
            {
                if (focused == eq_buttons[i])
                {
                    switch (i)
                    {
                    case 0:
                        cmd = CMD_EQ_SET_THEATER;
                        Serial.println("CMD_EQ_SET_THEATER");
                        break;
                    case 1:
                        cmd = CMD_EQ_SET_CAR;
                        Serial.println("CMD_EQ_SET_CAR");
                        break;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        // We're on main menu, check main menu buttons
        lv_obj_t *focused = lv_group_get_focused(focus_group);
        if (focused)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (focused == menu_buttons[i])
                {
                    switch (i)
                    {
                    case 0:
                        cmd = CMD_SWITCH_TO_SCR_BT;
                        break;
                    case 1:
                        cmd = CMD_SWITCH_TO_SCR_WIFI_RADIO;
                        break;
                    case 2:
                        cmd = CMD_SWITCH_TO_SCR_EQ;
                        break;
                    case 3:
                        cmd = CMD_SWITCH_TO_SCR_SETTINGS;
                        break;
                    }
                    break;
                }
            }
        }
    }

    if (cmd != CMD_NONE)
    {
        xQueueSend(appCommandQueue, &cmd, 0);
    }
}
```

## Expected Behavior After Fix

### On EQ Page:

```
Button fell
Short press
DEBUG: On EQ page, focused object = 0x3FFC1234
DEBUG: Matched eq_buttons[0]
CMD_EQ_SET_THEATER            ← Correct!
>>> Executing: CMD_EQ_SET_THEATER
EQ: Applying Theater preset...
```

### On Main Menu:

```
Button fell
Short press
CMD_SWITCH_TO_SCR_EQ          ← Correct! (navigating to EQ)
>>> switchToEQFocusGroup called
>>> EQ focus group active, btn_theater focused
```

## Benefits

1. **Context-aware button handling**: The system now recognizes which screen is active
2. **No redundant screen switches**: Won't try to switch to a screen you're already on
3. **Proper EQ button detection**: EQ preset buttons work correctly when on the EQ page
4. **Cleaner serial output**: Debug messages clearly show which context is being processed

## Testing Checklist

- [x] Click Theater button on EQ page → Should apply Theater preset (no screen switch)
- [x] Click Car button on EQ page → Should apply Car preset (no screen switch)
- [x] Click EQ button on main menu → Should navigate to EQ page
- [x] Navigate with encoder wheel on both screens → Should work as before
- [x] Double-click to return to main screen → Should work as before

## Related Files

- `src/main.cpp` - encoderTask() function (lines ~755-830)
- `src/globals.h` - current_screen variable declaration
