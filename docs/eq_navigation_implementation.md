# Equalizer Page Encoder Navigation Implementation

## Summary

Added encoder navigation support for the equalizer page buttons (`btn_theater` and `btn_car`) to match the main screen navigation behavior.

## Changes Made

### 1. Global Variables (globals.h)

Added:

```cpp
lv_group_t *focus_group_eq;  // Focus group for equalizer page buttons
lv_obj_t *eq_buttons[2];     // Array to store EQ page buttons
```

Added new commands:

```cpp
enum AppCommand {
    // ...existing commands...
    CMD_EQ_SET_THEATER,
    CMD_EQ_SET_CAR
};
```

### 2. Focus Group Setup (main.cpp)

#### New Function: `setupEncoderFocusGroupEQ()`

Creates a separate focus group for the equalizer page buttons:

```cpp
void setupEncoderFocusGroupEQ() {
    focus_group_eq = lv_group_create();
    lv_group_add_obj(focus_group_eq, objects.btn_theater);
    lv_group_add_obj(focus_group_eq, objects.btn_car);
    lv_obj_add_flag(objects.btn_theater, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(objects.btn_car, LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_CLICKABLE);
    lv_group_focus_obj(objects.btn_theater);
}
```

#### New Function: `switchToEQFocusGroup()`

Switches encoder input to the EQ focus group:

```cpp
void switchToEQFocusGroup() {
    lv_indev_set_group(enc_indev, focus_group_eq);
    lv_group_focus_obj(objects.btn_theater);
}
```

#### New Function: `switchToMainFocusGroup()`

Switches encoder input back to the main menu focus group:

```cpp
void switchToMainFocusGroup() {
    lv_indev_set_group(enc_indev, focus_group);
    lv_group_focus_obj(objects.a2dp_bluetooth);
}
```

### 3. Screen Switching Updates

#### Updated CMD_SWITCH_TO_SCR_MAIN

Now switches back to main focus group when returning to main screen:

```cpp
case CMD_SWITCH_TO_SCR_MAIN:
    lv_async_call([](void *unused) {
        switchToScreen(objects.main);
        switchToMainFocusGroup();  // Added
        stopBtSink();
    }, NULL);
    break;
```

#### Updated CMD_SWITCH_TO_SCR_EQ

Now switches to EQ focus group when entering equalizer page:

```cpp
case CMD_SWITCH_TO_SCR_EQ:
    lv_async_call([](void *unused) {
        switchToScreen(menu_screens[2]);
        switchToEQFocusGroup();  // Added
    }, NULL);
    break;
```

### 4. Encoder Button Handling

Updated the single-click handler in `encoderTask()` to check the current screen and use the appropriate focus group:

```cpp
// Check if we're on the main screen
if (current_screen == objects.main) {
    focused = lv_group_get_focused(focus_group);
    // Handle main menu buttons...
}
// Check if we're on the equalizer page
else if (current_screen == objects.equalizer_page) {
    focused = lv_group_get_focused(focus_group_eq);
    // Handle EQ buttons...
}
```

### 5. Setup Initialization

In `setup()`, added:

```cpp
eq_buttons[0] = objects.btn_theater;
eq_buttons[1] = objects.btn_car;

setupEncoderFocusGroupEQ();
Serial.println("EQ focus group ready");
```

### 6. Command Handlers

Added placeholder handlers in `appTask()`:

```cpp
case CMD_EQ_SET_THEATER:
    Serial.println("EQ: Theater preset selected");
    // TODO: Apply theater EQ preset when EQ is implemented
    break;

case CMD_EQ_SET_CAR:
    Serial.println("EQ: Car preset selected");
    // TODO: Apply car EQ preset when EQ is implemented
    break;
```

## User Experience

### Navigation Flow:

1. **Main Screen**: Encoder scrolls through 4 menu buttons (BT, AWS, EQ, Settings)
2. **Enter EQ Screen**: Single click on EQ button → switches to equalizer page
3. **EQ Screen**: Encoder now scrolls through 2 buttons (Theater, Car)
4. **Select Preset**: Single click on Theater or Car → triggers command
5. **Return to Main**: Double-click encoder → returns to main screen

### Button Behavior:

- **Encoder rotation**: Cycles through buttons in current focus group
- **Single click**: Activates the focused button
- **Double click**: Returns to main screen from any screen
- **Long press (2s)**: Triggers shutdown

## Benefits

1. ✅ **Consistent UX**: Same navigation pattern across all screens
2. ✅ **Separate focus groups**: No interference between different screen contexts
3. ✅ **Extensible**: Easy to add more buttons or screens with the same pattern
4. ✅ **Ready for EQ implementation**: Commands and handlers are in place

## Next Steps

When implementing the actual EQ functionality:

1. Replace the TODO comments in `CMD_EQ_SET_THEATER` and `CMD_EQ_SET_CAR` with actual EQ preset functions
2. Optionally add visual feedback (highlight selected preset on UI)
3. Consider adding a "Home" preset button for a third option

## Testing Checklist

- [ ] Main screen: Encoder scrolls through 4 menu buttons
- [ ] Enter EQ screen: Single click on EQ button works
- [ ] EQ screen: Encoder scrolls between Theater and Car buttons
- [ ] Theater button: Single click triggers CMD_EQ_SET_THEATER
- [ ] Car button: Single click triggers CMD_EQ_SET_CAR
- [ ] Double-click: Returns from EQ screen to main screen
- [ ] Focus indicators: Visual feedback shows which button is focused
- [ ] No interference: Main and EQ focus groups work independently

## Files Modified

- `src/globals.h` - Added focus_group_eq, eq_buttons array, and EQ commands
- `src/main.cpp` - Added focus group functions, updated screen switching, encoder handling
