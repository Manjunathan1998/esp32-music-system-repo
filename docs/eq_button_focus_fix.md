# EQ Button Focus and Color Fix

## Problem

When clicking on the EQ page buttons (Theater or Car), the following issues occurred:

1. **Focus moved to the other button** instead of staying on the clicked button
2. **Button color didn't change** to indicate which preset is active

## Root Cause

The button click handlers (`CMD_EQ_SET_THEATER` and `CMD_EQ_SET_CAR`) only applied the EQ settings but didn't:

- Maintain focus on the clicked button
- Change button appearance to show active state
- Reset the other button to inactive state

## Solution Implemented

### 1. Keep Focus on Clicked Button

Added `lv_group_focus_obj()` call after processing each command to explicitly set focus back to the clicked button.

### 2. Visual Feedback with Color Change

- **Active button**: Changed to **green** (`0x00FF00`) to indicate it's the currently selected preset
- **Inactive button**: Reset to **default cyan** color (`0xff0292a0`)

### 3. Thread-Safe UI Update

Used `lv_async_call()` to ensure UI updates happen in the correct LVGL task context.

## Code Changes

### CMD_EQ_SET_THEATER Handler

```cpp
case CMD_EQ_SET_THEATER:
    // ... apply EQ settings ...

    // Update button colors and maintain focus
    lv_async_call([](void *unused) {
        // Set Theater button to green (active)
        lv_obj_set_style_bg_color(objects.btn_theater,
                                  lv_color_hex(0x00FF00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        // Reset Car button to default color
        lv_obj_set_style_bg_color(objects.btn_car,
                                  lv_color_hex(0xff0292a0),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        // Keep focus on Theater button
        lv_group_focus_obj(objects.btn_theater);
    }, NULL);
    break;
```

### CMD_EQ_SET_CAR Handler

```cpp
case CMD_EQ_SET_CAR:
    // ... apply EQ settings ...

    // Update button colors and maintain focus
    lv_async_call([](void *unused) {
        // Set Car button to green (active)
        lv_obj_set_style_bg_color(objects.btn_car,
                                  lv_color_hex(0x00FF00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        // Reset Theater button to default color
        lv_obj_set_style_bg_color(objects.btn_theater,
                                  lv_color_hex(0xff0292a0),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        // Keep focus on Car button
        lv_group_focus_obj(objects.btn_car);
    }, NULL);
    break;
```

## Expected Behavior Now

1. **Click on Theater button**:

   - Theater button turns **green**
   - Car button returns to **default cyan**
   - Focus **stays on Theater button**
   - Theater EQ preset is applied

2. **Click on Car button**:
   - Car button turns **green**
   - Theater button returns to **default cyan**
   - Focus **stays on Car button**
   - Car EQ preset is applied

## Color Reference

- **Active (selected) button**: `0x00FF00` (bright green)
- **Inactive (default) button**: `0xff0292a0` (cyan/teal)
- **Focused button**: Retains LVGL default focus indicator (outline/border)

## Testing

Test the fix by:

1. Navigate to the EQ page
2. Click Theater button → should turn green and stay focused
3. Click Car button → should turn green, Theater should turn cyan
4. Click Theater again → should turn green, Car should turn cyan

## Notes

- The fix uses `lv_async_call()` to ensure thread-safety when updating UI from the appTask
- Colors are set using `lv_obj_set_style_bg_color()` which directly modifies the button's background
- Focus is explicitly maintained using `lv_group_focus_obj()` to override LVGL's default behavior of moving to the next item after click
