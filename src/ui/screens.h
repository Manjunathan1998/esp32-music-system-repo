#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *bt_screen;
    lv_obj_t *wifi_radio_screen;
    lv_obj_t *equalizer_screen;
    lv_obj_t *settings_screen;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *bluetooth;
    lv_obj_t *l_bluetooth;
    lv_obj_t *inet_radio;
    lv_obj_t *l_inet_radio;
    lv_obj_t *equalizer;
    lv_obj_t *l_qualizer;
    lv_obj_t *settings;
    lv_obj_t *l_settings;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *obj17;
    lv_obj_t *obj18;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *obj21;
    lv_obj_t *obj22;
    lv_obj_t *obj23;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_BT_SCREEN = 2,
    SCREEN_ID_WIFI_RADIO_SCREEN = 3,
    SCREEN_ID_EQUALIZER_SCREEN = 4,
    SCREEN_ID_SETTINGS_SCREEN = 5,
};

void create_screen_main();
void tick_screen_main();

void create_screen_bt_screen();
void tick_screen_bt_screen();

void create_screen_wifi_radio_screen();
void tick_screen_wifi_radio_screen();

void create_screen_equalizer_screen();
void tick_screen_equalizer_screen();

void create_screen_settings_screen();
void tick_screen_settings_screen();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/