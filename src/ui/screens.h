#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *bt_screen;
    lv_obj_t *aws_connect;
    lv_obj_t *settings_page;
    lv_obj_t *main_list_holder;
    lv_obj_t *a2dp_bluetooth;
    lv_obj_t *l_a2dp;
    lv_obj_t *aws_sync;
    lv_obj_t *l_aws;
    lv_obj_t *equalizer_button;
    lv_obj_t *equalizer;
    lv_obj_t *l_eq;
    lv_obj_t *settings;
    lv_obj_t *l_settings;
    lv_obj_t *battery_skeleton;
    lv_obj_t *ions_left_20;
    lv_obj_t *ions_left_60;
    lv_obj_t *ions_left_60_above;
    lv_obj_t *obj0;
    lv_obj_t *bluetooth_panel;
    lv_obj_t *artist_name;
    lv_obj_t *track_name_1;
    lv_obj_t *status_;
    lv_obj_t *track_name_2;
    lv_obj_t *battery_skeleton_1;
    lv_obj_t *ions_left_21;
    lv_obj_t *ions_left_61;
    lv_obj_t *ions_left_62;
    lv_obj_t *eqpanel;
    lv_obj_t *bluetooth_panel_1;
    lv_obj_t *battery_skeleton_2;
    lv_obj_t *ions_left_22;
    lv_obj_t *ions_left_63;
    lv_obj_t *ions_left_64;
    lv_obj_t *eq_panel_container;
    lv_obj_t *eq_panel;
    lv_obj_t *battery_skeleton_3;
    lv_obj_t *ions_left_23;
    lv_obj_t *ions_left_65;
    lv_obj_t *ions_left_66;
    lv_obj_t *scan;
    lv_obj_t *settings_container;
    lv_obj_t *settings_panel;
    lv_obj_t *battery_skeleton_4;
    lv_obj_t *ions_left_24;
    lv_obj_t *ions_left_67;
    lv_obj_t *ions_left_68;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_BT_SCREEN = 2,
    SCREEN_ID_AWS_CONNECT = 3,
    SCREEN_ID_EQUALIZER = 4,
    SCREEN_ID_SETTINGS_PAGE = 5,
};

void create_screen_main();
void tick_screen_main();

void create_screen_bt_screen();
void tick_screen_bt_screen();

void create_screen_aws_connect();
void tick_screen_aws_connect();

void create_screen_equalizer();
void tick_screen_equalizer();

void create_screen_settings_page();
void tick_screen_settings_page();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/