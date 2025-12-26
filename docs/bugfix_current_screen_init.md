# Bug Fix: current_screen Not Initialized

## Problem Identified

From the debug output:

```
DEBUG: current_screen = 0  ❌
DEBUG: objects.main = 3FFC68EC
DEBUG: objects.equalizer_page = 3FFC8AE4
DEBUG: On different screen (not main or EQ)
DEBUG: No command to send (CMD_NONE)
```

**Root Cause:** `current_screen` was `NULL (0)` even though the main screen was displayed.

## Why This Happened

1. `current_screen` is declared in `globals.h` as:

   ```cpp
   static lv_obj_t *current_screen = NULL;
   ```

2. In `setup()`, `ui_init()` is called which loads the main screen automatically (via EEZ Studio generated code)

3. However, **`current_screen` variable was never set** to `objects.main`

4. `switchToScreen()` sets `current_screen`, but it was never called for the initial main screen

5. Result: The encoder button handler checks:
   ```cpp
   if (current_screen == objects.main)  // 0 == 0x3FFC68EC = FALSE!
   ```
   This always failed, so no commands were sent!

## Solution

Added initialization of `current_screen` right after `ui_init()`:

```cpp
ui_init();

// Initialize current_screen to main screen (ui_init loads main screen by default)
current_screen = objects.main;
Serial.print(">>> Initialized current_screen to main: ");
Serial.println((uint32_t)current_screen, HEX);
```

## Expected Behavior After Fix

Now when you click a button on the main screen, the debug output should show:

```
DEBUG: current_screen = 3FFC68EC  ✅
DEBUG: objects.main = 3FFC68EC
DEBUG: On main screen - checking main focus group  ✅
DEBUG: Focused object = [address]
DEBUG: Matched menu_buttons[X]
CMD_SWITCH_TO_SCR_[BT/EQ/etc.]  ✅
DEBUG: Sending command to queue: X  ✅
```

And the screen will actually switch! 🎉

## Location

**File:** `src/main.cpp`
**Function:** `setup()`
**Line:** After `ui_init()` call

## Testing

1. Upload the fixed code
2. Open Serial Monitor
3. Click any button on main screen
4. You should now see:
   - `current_screen` matches `objects.main`
   - "On main screen - checking main focus group"
   - Command sent and executed
   - Screen switches successfully

## Lesson Learned

When using generated UI code (like EEZ Studio), always ensure that any application state variables tracking the UI (like `current_screen`) are properly initialized to match the UI's initial state.

The `ui_init()` function sets up LVGL objects and loads a default screen, but it doesn't know about our custom `current_screen` variable, so we must initialize it ourselves.
