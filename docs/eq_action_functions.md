# Action Functions Implementation for EQ Buttons

## Overview

Implemented action callback functions for the Theater and Car EQ preset buttons. These functions will be called when the buttons are pressed via LVGL event system.

## Files Modified

### 1. `src/ui/actions.h`

**Function Declarations:**

```cpp
extern void action_theater_press(lv_event_t * e);
extern void action_car_press(lv_event_t * e);
```

### 2. `src/main.cpp`

**External Declarations (lines 19-21):**

```cpp
/// actions start ///
extern void action_theater_press(lv_event_t *e);
extern void action_car_press(lv_event_t *e);
/// actions end ///
```

**Function Implementations (lines 315-324):**

```cpp
// Action function implementations
void action_theater_press(lv_event_t *e)
{
	Serial.println(">>> action_theater_press");
}

void action_car_press(lv_event_t *e)
{
	Serial.println(">>> action_car_press");
}
```

## Function Signatures

Both functions follow the LVGL event callback signature:

- **Parameter:** `lv_event_t *e` - LVGL event structure containing event details
- **Return:** `void`
- **Purpose:** Handle button press events for EQ presets

## Current Behavior

### action_theater_press()

- Prints: `>>> action_theater_press`
- Called when Theater button is pressed

### action_car_press()

- Prints: `>>> action_car_press`
- Called when Car button is pressed

## Usage

These functions are designed to be attached to LVGL button objects as event callbacks. In EEZ Studio or LVGL code, you would attach them like:

```cpp
lv_obj_add_event_cb(objects.btn_theater, action_theater_press, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(objects.btn_car, action_car_press, LV_EVENT_CLICKED, NULL);
```

## Next Steps (Optional Enhancements)

To make these functions fully functional, you could add:

1. **Send commands to appTask:**

   ```cpp
   void action_theater_press(lv_event_t *e)
   {
       Serial.println(">>> action_theater_press");
       AppCommand cmd = CMD_EQ_SET_THEATER;
       xQueueSend(appCommandQueue, &cmd, 0);
   }
   ```

2. **Direct EQ manipulation:**

   ```cpp
   void action_theater_press(lv_event_t *e)
   {
       Serial.println(">>> action_theater_press");
       bassGain = 2.0;
       midGain = 0.8;
       trebleGain = 1.5;
       // Apply EQ settings...
   }
   ```

3. **Update button visual states:**
   ```cpp
   void action_theater_press(lv_event_t *e)
   {
       Serial.println(">>> action_theater_press");
       lv_obj_set_style_bg_color(objects.btn_theater, lv_color_hex(0x00FF00), LV_PART_MAIN);
       lv_obj_set_style_bg_color(objects.btn_car, lv_color_hex(0xff0292a0), LV_PART_MAIN);
   }
   ```

## Testing

To verify these functions work:

1. Build and upload the code
2. Navigate to the EQ page
3. Press the Theater button → Should see `>>> action_theater_press` in Serial Monitor
4. Press the Car button → Should see `>>> action_car_press` in Serial Monitor

## Notes

- Functions are declared with `extern "C"` linkage in `actions.h` to ensure compatibility with C code
- The `lv_event_t *e` parameter can be used to get additional event information:
  - `lv_event_get_code(e)` - Get event type (CLICKED, PRESSED, etc.)
  - `lv_event_get_target(e)` - Get the object that triggered the event
  - `lv_event_get_user_data(e)` - Get custom user data passed during callback registration
