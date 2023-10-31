/**
 * @file lv_imgbtn.h
 *
 */

#ifndef LV_ICEIMGBTN_H
#define LV_ICEIMGBTN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../../lvgl.h"

#define LV_USE_ICEIMGBTN 1

#if LV_USE_ICEIMGBTN != 0

/*********************
 *      DEFINES
 *********************/
typedef enum {
    LV_ICEBTN_STATE_RELEASED,
    LV_ICEBTN_STATE_PRESSED,
    LV_ICEBTN_STATE_DISABLED,
    LV_ICEBTN_STATE_CHECKED_RELEASED,
    LV_ICEBTN_STATE_CHECKED_PRESSED,
    LV_ICEBTN_STATE_CHECKED_DISABLED,
    _LV_ICEBTN_STATE_NUM,
} lv_icebtn_state_t;

/**********************
 *      TYPEDEFS
 **********************/
/*Data of image button*/
typedef struct {
    lv_obj_t obj;
    const void * img_src_mid[_LV_ICEBTN_STATE_NUM];   /*Store center images to each state*/
    const void * img_src_left[_LV_ICEBTN_STATE_NUM];  /*Store left side images to each state*/
    const void * img_src_right[_LV_ICEBTN_STATE_NUM]; /*Store right side images to each state*/
    lv_img_cf_t act_cf; /*Color format of the currently active image*/
    const char *text[_LV_ICEBTN_STATE_NUM];
} lv_icebtn_t;

extern const lv_obj_class_t lv_icebtn_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an image button object
 * @param parent pointer to an object, it will be the parent of the new image button
 * @return pointer to the created image button
 */
lv_obj_t * lv_imgbtn_create(lv_obj_t * parent);

/*======================
 * Add/remove functions
 *=====================*/

/*=====================
 * Setter functions
 *====================*/

/**
 * Set images for a state of the image button
 * @param imgbtn pointer to an image button object
 * @param state for which state set the new image
 * @param src_left pointer to an image source for the left side of the button (a C array or path to
 * a file)
 * @param src_mid pointer to an image source for the middle of the button (ideally 1px wide) (a C
 * array or path to a file)
 * @param src_right pointer to an image source for the right side of the button (a C array or path
 * to a file)
 */
void lv_icebtn_set_src(lv_obj_t * imgbtn, lv_icebtn_state_t state, const void * src_left, const void * src_mid,
                       const void * src_right);

void lv_icebtn_set_label(lv_obj_t * imgbtn, lv_icebtn_state_t state, char *txt);

/**
 * Use this function instead of `lv_obj_add/clear_state` to set a state manually
 * @param imgbtn pointer to an image button object
 * @param state  the new state
 */
void lv_icebtn_set_state(lv_obj_t * imgbtn, lv_icebtn_state_t state);

/*=====================
 * Getter functions
 *====================*/

/**
 * Get the left image in a given state
 * @param imgbtn pointer to an image button object
 * @param state the state where to get the image (from `lv_btn_state_t`) `
 * @return pointer to the left image source (a C array or path to a file)
 */
const void * lv_icebtn_get_src_left(lv_obj_t * imgbtn, lv_icebtn_state_t state);

/**
 * Get the middle image in a given state
 * @param imgbtn pointer to an image button object
 * @param state the state where to get the image (from `lv_btn_state_t`) `
 * @return pointer to the middle image source (a C array or path to a file)
 */
const void * lv_icebtn_get_src_middle(lv_obj_t * imgbtn, lv_icebtn_state_t state);

/**
 * Get the right image in a given state
 * @param imgbtn pointer to an image button object
 * @param state the state where to get the image (from `lv_btn_state_t`) `
 * @return pointer to the left image source (a C array or path to a file)
 */
const void * lv_icebtn_get_src_right(lv_obj_t * imgbtn, lv_icebtn_state_t state);


/*=====================
 * Other functions
 *====================*/

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_IMGBTN*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_IMGBTN_H*/
