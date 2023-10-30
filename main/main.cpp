#include <stdio.h>
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <rom/rtc.h>

const char *TAG = "TERMOSTAT_EKRAN";
const char *PRG_NAME = "TERMOSTAT";

bool ffs_init(void);
int file_size(const char *name);
bool start_page(void);
bool main_page(void);

uint8_t backlight_value = 20;

#include "panel.cpp"
#include "calibrate.h"

extern "C" void app_main()
{
    uint8_t Reset_Mean = rtc_get_reset_reason(0);
    printf("RESET %d\n",Reset_Mean);
    ffs_init();

    if (Reset_Mean==1) chip_info();
  
    lcd.init();
    lcd.touch();
    lcd.setColorDepth(16);
    lcd.setBrightness(backlight_value);
 
    lcd.startWrite();
    lcd.fillScreen(TFT_BLACK);
    lcd.endWrite();

    if (Reset_Mean==1)  start_page();

    //-----------LVGL INIT -------------------------
    lv_init();
    lv_disp_draw_buf_init( &draw_buf, buf, NULL, PANEL_W * ln );

    //lvgl için sanal bir panel oluşturulup bu panel lvgl_lcd_flash
    //aracılıgı ile lovyanGFX e baglanıyor 
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = PANEL_W;
    disp_drv.ver_res = PANEL_H;
    disp_drv.flush_cb = &lvgl_lcd_flash;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register( &disp_drv );

    //lvgl background rengi degişiyor
    lv_style_t *ice_style;
    ice_style = (lv_style_t *)malloc(sizeof(lv_style_t));
    lv_style_init(ice_style);
    lv_style_set_bg_color(ice_style, LV_COLOR_MAKE(0x00, 0x00, 0x00));
    lv_style_set_text_color(ice_style, LV_COLOR_MAKE(0xff, 0xff, 0xff));
    lv_obj_add_style(lv_scr_act(), ice_style, LV_STATE_DEFAULT);
 
    //Lvgl in touchpad driverı lovyanGFX touchpad ine yönlendiriliyor
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touchpad_read;
    lv_indev_drv_register( &indev_drv );

    //lvgl zamanlaması için periyodik timer oluşturuluyor.
    //lvgl bu timera göre aksiyon üreteceginden ekran hassasiyeti
    //bu timera baglı oluyor.
    esp_timer_create_args_t periodic_timer_args = {};
    periodic_timer_args.callback = &lvgl_tick_task;
    periodic_timer_args.name = "periodic_gui";
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    //main_page();
    touchscreen_cal_create();

    while(true)
    {
        lv_task_handler();
        vTaskDelay(10/portTICK_PERIOD_MS);    
    }
    
}

bool ffs_init(void)
{
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/config",
      .partition_label = "storage",
      .max_files = 8,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
         
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
  return true;

}

int file_size(const char *name)
{
    FILE *fd = fopen(name, "r+");
    if (fd == NULL) {ESP_LOGE(TAG, "%s not open",name); return 0;}
    fseek(fd, 0, SEEK_END);
    int file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    fclose(fd);
    return file_size;
}

bool start_page(void)
{
    //Bu ekran lovyanGFX tarafından kontrol ediliyor. 
  uint8_t *img;
  uint32_t imgSize = file_size("/config/logo.png");
  img = (uint8_t *)malloc(imgSize+10);
  if (img==NULL) {
    ESP_LOGE(TAG,"Hafızada %d byte yer ayrılamadı\n", imgSize);
    return false;
  }
  if (img!=NULL)
  {
     FILE *fd = fopen("/config/logo.png", "r+");
     if (fd == NULL) {ESP_LOGE(TAG,"logo.png dosyası Okunamadı"); free(img); return false;}
     fread(img,1,imgSize,fd);
     fclose(fd);

     lcd.startWrite();
     lcd.drawPng((std::uint8_t*)img, imgSize, 50, 50,380,380);
     
     free(img);
     //--------------------
     lcd.setFont(&fonts::FreeSansBold12pt7b);
     lcd.setTextColor(TFT_LIGHTGRAY);
     lcd.drawString(PRG_NAME, 160, 380);
     lcd.setFont(&fonts::FreeMono12pt7b);
     lcd.drawString(desc->version, 190, 405);

     lcd.endWrite();

     vTaskDelay(2000/portTICK_PERIOD_MS); 
     return true;
  } 
  return false;
}




/*
static void button_event(lv_obj_t * btn, lv_event_t event)
{
    lv_obj_t * label = lv_obj_get_child(btn, NULL);
    char *aa = (char *) lv_obj_get_user_data(btn);
    if(strcmp(aa,"1")==0 && event.code == LV_EVENT_CLICKED) {
        free(a0);
        printf("BASILSI\n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
    
    if(strcmp(aa,"2")==0 && event == LV_EVENT_VALUE_CHANGED) {
        uint8_t kk = lv_btn_get_state(btn);
        if (kk==0) lv_label_set_text(label, "Auto");
        if (kk==3) lv_label_set_text(label, "Manuel");
        printf("DURUM = %d\n",kk);
    }
   
}
*/




static lv_obj_t * meter;

static void set_value(void * indic, int32_t v)
{
    lv_meter_set_indicator_value(meter, (lv_meter_indicator_t *)indic, v);
}

char *a0, *a1, *a2;
uint8_t min = 15, max=56, set = 30;
lv_meter_indicator_t *indic, *indic1, *indic2;
lv_obj_t *lab0;

static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);  
    char *aa = (char*)lv_event_get_user_data(e);
    /*  
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    char *aa = (char *) lv_obj_get_user_data(btn);
    
    */
    //lv_label_set_text_fmt(label, "%"LV_PRIu32, cnt);

    if(strcmp(aa,"1")==0 && code == LV_EVENT_CLICKED) {
        //free(a0);
        printf("BASILDI\n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
    if(strcmp(aa,"2")==0 && code == LV_EVENT_CLICKED) {
        if (set+1<max)
        {
            set+=1;
            lv_meter_set_indicator_start_value(meter, indic, set);
            char *mm = (char*)malloc(5);
            sprintf(mm,"%d",set);
            lv_label_set_text(lab0,mm);
            lv_event_send(meter, LV_EVENT_REFRESH , NULL);
            free(mm);
        }
        
        //free(a1);
        printf("UP \n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
    if(strcmp(aa,"3")==0 && code == LV_EVENT_CLICKED) {
        if (set-1>min+10)
        {
            set-=1;
            lv_meter_set_indicator_start_value(meter, indic, set);
            char *mm = (char*)malloc(5);
            sprintf(mm,"%d",set);
            lv_label_set_text(lab0,mm);
            free(mm);
            lv_event_send(meter, LV_EVENT_REFRESH , NULL);
        }
        printf("DOWN \n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
  
}

LV_IMG_DECLARE(up);
LV_IMG_DECLARE(down);

bool main_page(void)
{

    meter = lv_meter_create(lv_scr_act());
    lv_obj_center(meter);
    lv_obj_set_size(meter, 300, 300);
    
    /*Add a scale first*/
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, scale, min,max,225,135);
    lv_meter_set_scale_ticks(meter, scale, 40, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 10, 4, 15, lv_color_black(), 10);

    /*Add a red arc to the end*/
    indic2 = lv_meter_add_arc(meter, scale, 8, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic2, set);
    lv_meter_set_indicator_end_value(meter, indic2, max);

    /*Make the tick lines red at the end of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), true,
                                     0);
    lv_meter_set_indicator_start_value(meter, indic, set);
    lv_meter_set_indicator_end_value(meter, indic, max);

    /*Add a needle line indicator*/
    indic1 = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), 10);

    //indic1 = lv_meter_add_arc(meter, scale, 10, lv_palette_main(LV_PALETTE_RED), 0);

    lab0 = lv_label_create(meter);
    lv_obj_set_style_text_font(lab0, &lv_font_montserrat_30, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab0,LV_COLOR_MAKE(0xff, 0x00, 0x00),  LV_STATE_DEFAULT);
    //lv_obj_set_pos(lab, 20, 20);
    lv_obj_align(lab0, LV_ALIGN_CENTER, -20, -40);
    char *mm = (char*)malloc(5);
    sprintf(mm,"%d",set);
    lv_label_set_text(lab0,mm);
    free(mm);

    lv_obj_t *lab1 = lv_label_create(meter);
    lv_obj_set_style_text_font(lab1, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab1, LV_COLOR_MAKE(0xff, 0x00, 0x00), LV_STATE_DEFAULT);
    lv_obj_align_to(lab1, lab0, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_label_set_text(lab1,"SET");

    
    lv_obj_t * img2 = lv_img_create(meter);
    lv_img_set_src(img2, &up);
    lv_obj_set_size(img2, 32,32);
    lv_obj_set_style_img_opa(img2,50, LV_STATE_DEFAULT);
    lv_obj_align_to(img2, lab0, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    lv_obj_add_flag(img2, LV_OBJ_FLAG_CLICKABLE);
    a1 = (char *)malloc(5);
    strcpy(a1,"2");
    lv_obj_add_flag(img2, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(img2, event_handler, LV_EVENT_CLICKED, a1);
  

    lv_obj_t * img3 = lv_img_create(meter);
    lv_img_set_src(img3, &down);
    lv_obj_set_size(img3, 32,32);
    lv_obj_set_style_img_opa(img3,50, LV_STATE_DEFAULT);
    lv_obj_align_to(img3, lab0, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_flag(img3, LV_OBJ_FLAG_CLICKABLE);
    a2 = (char *)malloc(5);
    strcpy(a2,"3");
    lv_obj_add_event_cb(img3, event_handler, LV_EVENT_CLICKED, a2);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_value);
    lv_anim_set_var(&a, indic);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 5000);
    lv_anim_set_repeat_delay(&a, 100);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_playback_delay(&a, 100);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    //lv_anim_start(&a);

    lv_obj_t * label;
    lv_obj_t * setup_btn = lv_btn_create(lv_scr_act());   
    lv_obj_set_pos(setup_btn, 10, 10);                            
    lv_obj_set_size(setup_btn, 100, 50);   
    a0 = (char *)malloc(5);
    strcpy(a0,"1");
    lv_obj_add_event_cb(setup_btn, event_handler, LV_EVENT_CLICKED, a0);
    label = lv_label_create(setup_btn);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS " Setup");
  
    return true; 
}