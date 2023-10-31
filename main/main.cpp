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
static void temp_callback(void *arg);

uint8_t backlight_value = 20;

#include "panel.cpp"
#include "dht11.h"
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
    DHT11_init(GPIO_NUM_1);

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

    esp_timer_create_args_t arg = {};
    arg.callback = &temp_callback;
    periodic_timer_args.name = "periodic_temp";
    esp_timer_handle_t temp_timer;
    ESP_ERROR_CHECK(esp_timer_create(&arg, &temp_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(temp_timer, 10000 * 1000));

    main_page();
   // touchscreen_cal_create();


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

char *a0, *a1, *a2, *a3, *a4;
uint8_t min = 15, max=56, set = 27, temp = min;
bool autoflag = true;
lv_meter_indicator_t *indic, *indic1, *indic2, *indic3;
lv_obj_t *lab0, *lab1, *lab2, *lab3, *img_alev, *auto_btn, *grp1, *grp2, *sw, *swlab;

static bool Role_Durumu=false;

static void temp_rate(bool state)
{
    bool relay_stat;
    if (autoflag) {
        if (set>temp) relay_stat=true; else relay_stat=false; 
    } else {
        if (state) relay_stat=true; else relay_stat=false; 
    }

    if (Role_Durumu!=relay_stat) {
        Role_Durumu = relay_stat;
        //Role_Durumunu CPU2 ye gonder
        if (relay_stat )
        {
                lv_obj_clear_flag(img_alev, LV_OBJ_FLAG_HIDDEN);  
                printf("ROLE AKTIF\n");
            } else {
                lv_obj_add_flag(img_alev, LV_OBJ_FLAG_HIDDEN);
                printf("ROLE PASIF\n");
            }
        lv_event_send(img_alev, LV_EVENT_REFRESH , NULL);  
    }
}

static void temp_callback(void *arg)
{
    struct dht11_reading aa = DHT11_read();
    int tmp = (aa.temperature * 0.55) + 3;
    int hum = aa.humidity;
   // int sta = aa.status;
    temp=tmp;
    set_value(indic1,tmp);
    char *mm = (char*)malloc(5);
    sprintf(mm,"%d",tmp);
    lv_label_set_text(lab2,mm);
    free(mm);
    
    printf("%3d %3d\n",tmp,hum);
    if (autoflag) temp_rate(true);

}

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
    if(strcmp(aa,"4")==0 && code == LV_EVENT_VALUE_CHANGED) {
         printf("Event %d %s\n",code, aa);
         lv_obj_t * btn = lv_event_get_target(e);
         lv_state_t bb = lv_obj_get_state(btn);
         if (bb & LV_STATE_CHECKED)
          {
            printf("AUTO\n");
            lv_obj_add_flag(grp1, LV_OBJ_FLAG_HIDDEN);
            autoflag = false;
            lv_obj_add_state(sw, LV_STATE_CHECKED);
            temp_callback(NULL);
            lv_obj_clear_flag(grp2, LV_OBJ_FLAG_HIDDEN);
          } else {
            printf("MANUAL\n");
            lv_obj_add_flag(grp2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(grp1, LV_OBJ_FLAG_HIDDEN);
            temp_callback(NULL);
            autoflag = true;
          }
         printf("State = %d\n", bb);
    }

    if(strcmp(aa,"5")==0 && code == LV_EVENT_VALUE_CHANGED) {
         printf("Event %d %s\n",code, aa);
         lv_obj_t * btn = lv_event_get_target(e);
         lv_state_t bb = lv_obj_get_state(btn);
         if (bb & LV_STATE_CHECKED)
          {
            printf("acık\n");
            temp_rate(true);
            lv_label_set_text(swlab,"OPEN");
          } else {
            printf("kapalı\n");
            temp_rate(false);
            lv_label_set_text(swlab,"CLOSE");
          }
         printf("State = %d\n", bb);
    }

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
            lv_meter_set_indicator_start_value(meter, indic2, set);
            char *mm = (char*)malloc(5);
            sprintf(mm,"%d",set);
            lv_label_set_text(lab0,mm);
            temp_rate(true);
            lv_event_send(meter, LV_EVENT_REFRESH , NULL);
            free(mm);
        }
        
        //free(a1);
        //printf("UP \n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
    if(strcmp(aa,"3")==0 && code == LV_EVENT_CLICKED) {
        if (set-1>min)
        {
            set-=1;
            lv_meter_set_indicator_start_value(meter, indic2, set);
            char *mm = (char*)malloc(5);
            sprintf(mm,"%d",set);
            lv_label_set_text(lab0,mm);
            free(mm);
            temp_rate(true);
            lv_event_send(meter, LV_EVENT_REFRESH , NULL);
        }
        //printf("DOWN \n");
        //free(a1);
        //lv_obj_clean(lv_scr_act());
        //setup_page();
    }
  
}

LV_IMG_DECLARE(up);
LV_IMG_DECLARE(down);
LV_IMG_DECLARE(alev);
LV_IMG_DECLARE(auto_img1);
LV_IMG_DECLARE(smart_img1);
LV_FONT_DECLARE(dejavu_56);

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
    indic2 = lv_meter_add_arc(meter, scale, 6, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic2, set);
    lv_meter_set_indicator_end_value(meter, indic2, max);

    indic1 = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_GREY), 10);

    grp1 = lv_obj_create(meter);
    //lv_obj_set_style_bg_color(grp1,LV_COLOR_MAKE(0x00, 0xff, 0x00),LV_STATE_DEFAULT);
    lv_obj_remove_style_all(grp1);
    lv_obj_set_style_bg_opa(grp1, LV_OPA_TRANSP, LV_STATE_DEFAULT); 
    lv_obj_set_size(grp1, 160,60);  
    lv_obj_align(grp1, LV_ALIGN_CENTER, 0, -50);

    grp2 = lv_obj_create(meter);
    lv_obj_remove_style_all(grp2);
    lv_obj_set_size(grp2, 160,60);  
    lv_obj_align(grp2, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_flag(grp2, LV_OBJ_FLAG_HIDDEN);

    sw = lv_switch_create(grp2);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    a4 = (char *)malloc(5);
    strcpy(a4,"5");
    lv_obj_add_event_cb(sw, event_handler, LV_EVENT_VALUE_CHANGED, a4);
    lv_obj_align(sw, LV_ALIGN_CENTER, 0, -0);

    swlab = lv_label_create(grp2);
    lv_obj_set_style_text_font(swlab, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(swlab, LV_COLOR_MAKE(0x00, 0x00, 0xff), LV_STATE_DEFAULT);
    lv_obj_align_to(swlab, sw, LV_ALIGN_OUT_TOP_MID, -8, 0);
    lv_label_set_text(swlab,"OPEN");

 
 //----------  
    lab0 = lv_label_create(grp1);
    lv_obj_set_style_text_font(lab0, &lv_font_montserrat_30, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab0,LV_COLOR_MAKE(0xff, 0x00, 0x00),  LV_STATE_DEFAULT);
    lv_obj_align(lab0, LV_ALIGN_CENTER, 0, 5);
    char *mm = (char*)malloc(5);
    sprintf(mm,"%d",set);
    lv_label_set_text(lab0,mm);
    free(mm);

    lv_obj_t *lab1 = lv_label_create(grp1);
    lv_obj_set_style_text_font(lab1, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab1, LV_COLOR_MAKE(0xff, 0x00, 0x00), LV_STATE_DEFAULT);
    lv_obj_align_to(lab1, lab0, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_label_set_text(lab1,"SET");

    
    lv_obj_t * img2 = lv_img_create(grp1);
    lv_img_set_src(img2, &up);
    lv_obj_set_size(img2, 32,32);
    lv_obj_set_style_img_opa(img2,50, LV_STATE_DEFAULT);
    lv_obj_align_to(img2, lab0, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    lv_obj_add_flag(img2, LV_OBJ_FLAG_CLICKABLE);
    a1 = (char *)malloc(5);
    strcpy(a1,"2");
    lv_obj_add_flag(img2, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(img2, event_handler, LV_EVENT_CLICKED, a1);
  
    lv_obj_t * img3 = lv_img_create(grp1);
    lv_img_set_src(img3, &down);
    lv_obj_set_size(img3, 32,32);
    lv_obj_set_style_img_opa(img3,50, LV_STATE_DEFAULT);
    lv_obj_align_to(img3, lab0, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_add_flag(img3, LV_OBJ_FLAG_CLICKABLE);
    a2 = (char *)malloc(5);
    strcpy(a2,"3");
    lv_obj_add_event_cb(img3, event_handler, LV_EVENT_CLICKED, a2);
//-----------------------------

    lab2 = lv_label_create(meter);
    lv_obj_set_style_text_font(lab2, &dejavu_56, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab2,LV_COLOR_MAKE(0x00, 0x00, 0xff),  LV_STATE_DEFAULT);
    lv_obj_align(lab2, LV_ALIGN_BOTTOM_MID, 0, -20);
    mm = (char*)malloc(5);
    sprintf(mm,"%d",temp);
    lv_label_set_text(lab2,mm);
    free(mm);

    lab3 = lv_label_create(meter);
    lv_obj_set_style_text_font(lab3, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab3, LV_COLOR_MAKE(0x00, 0x00, 0xff), LV_STATE_DEFAULT);
    lv_obj_align_to(lab3, lab2, LV_ALIGN_OUT_TOP_MID, -10, 0);
    lv_label_set_text(lab3,"TEMP");

    img_alev = lv_img_create(meter);
    lv_img_set_src(img_alev, &alev);
    lv_obj_set_size(img_alev, 32,32);
    //lv_obj_set_style_img_opa(img_alev,50, LV_STATE_DEFAULT);
    lv_obj_align(img_alev, LV_ALIGN_CENTER, 0, 0);
    //lv_obj_add_flag(img_alev, LV_OBJ_FLAG_HIDDEN);
    //lv_obj_clear_flag(img_alev, LV_OBJ_FLAG_HIDDEN);
    temp_rate(false);

    auto_btn = lv_imgbtn_create(meter);
    lv_obj_set_size(auto_btn, 48,48);
    lv_imgbtn_set_src(auto_btn, LV_IMGBTN_STATE_RELEASED, NULL,&auto_img1,NULL);
    lv_imgbtn_set_src(auto_btn, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, &auto_img1, NULL);
    lv_imgbtn_set_src(auto_btn, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, &smart_img1, NULL);
    lv_imgbtn_set_src(auto_btn, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, &smart_img1, NULL);
    lv_obj_add_flag(auto_btn, LV_OBJ_FLAG_CHECKABLE);
    a3 = (char *)malloc(5);
    strcpy(a3,"4");
    lv_obj_add_event_cb(auto_btn, event_handler, LV_EVENT_VALUE_CHANGED, a3);
    lv_obj_align(auto_btn, LV_ALIGN_RIGHT_MID, -20, 30);

    


    lv_obj_t * label;
    lv_obj_t * setup_btn = lv_btn_create(lv_scr_act());   
    lv_obj_set_pos(setup_btn, 10, 10);                            
    lv_obj_set_size(setup_btn, 100, 50);   
    a0 = (char *)malloc(5);
    strcpy(a0,"1");
    lv_obj_add_event_cb(setup_btn, event_handler, LV_EVENT_CLICKED, a0);
    label = lv_label_create(setup_btn);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS " Setup");

    autoflag = true;
    temp_callback(NULL);
  
    return true; 
}