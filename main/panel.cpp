

#include <lvgl.h>
#include "esp_ota_ops.h"

#define LGFX_USE_V1
#define PANEL_W 480 
#define PANEL_H 480

#define  LV_TICK_PERIOD_MS 50

#include <LovyanGFX.hpp>

#define D_W PANEL_W
#define D_H PANEL_H


#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#include <driver/i2c.h>

//static const uint16_t screenWidth  = PANEL_W ;
//static const uint16_t screenHeight = PANEL_H ;
static const uint8_t ln = 10; 
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ PANEL_W * ln ];

class LGFX : public lgfx::LGFX_Device
{
public:

  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_ST7701 _panel_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT6x36 _touch_instance;

  LGFX(void)
  {
    {
      auto cfg = _panel_instance.config();

      cfg.memory_width  = PANEL_W;
      cfg.memory_height = PANEL_H;
      cfg.panel_width  = PANEL_W;
      cfg.panel_height = PANEL_H;

    
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.pin_cs = GPIO_NUM_38;  //ok   
      cfg.pin_rst = GPIO_NUM_42; //ok
      cfg.invert = true;

    
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config_detail();

      cfg.use_psram = 1;
      cfg.pin_cs = GPIO_NUM_38;
      cfg.pin_mosi = GPIO_NUM_40; //ok
      cfg.pin_sclk = GPIO_NUM_39; //ok

      _panel_instance.config_detail(cfg);
    }

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      cfg.pin_d0  = GPIO_NUM_14;  // B0
      cfg.pin_d1  = GPIO_NUM_13;  // B1
      cfg.pin_d2  = GPIO_NUM_12; // B2
      cfg.pin_d3  = GPIO_NUM_11;  // B3
      cfg.pin_d4  = GPIO_NUM_10;  // B4
      cfg.pin_d5  = GPIO_NUM_9;  // G0
      cfg.pin_d6  = GPIO_NUM_46;  // G1
      cfg.pin_d7  = GPIO_NUM_3;  // G2
      cfg.pin_d8  = GPIO_NUM_20; // G3
      cfg.pin_d9  = GPIO_NUM_19; // G4
      cfg.pin_d10 = GPIO_NUM_8;  // G5
      cfg.pin_d11 = GPIO_NUM_18; // R0
      cfg.pin_d12 = GPIO_NUM_17; // R1
      cfg.pin_d13 = GPIO_NUM_16; // R2
      cfg.pin_d14 = GPIO_NUM_15; // R3
      cfg.pin_d15 = GPIO_NUM_7; // R4

      cfg.pin_henable = GPIO_NUM_48;
      cfg.pin_vsync   = GPIO_NUM_47;
      cfg.pin_hsync   = GPIO_NUM_21;
      cfg.pin_pclk    = GPIO_NUM_45; 
      cfg.freq_write  = 9 * 1000 * 1000;

       cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 18;
      cfg.hsync_pulse_width = 8;
      cfg.hsync_back_porch  = 50;

      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 0x0C;//10;
      cfg.vsync_pulse_width = 8;
      cfg.vsync_back_porch  = 0x0E;//20;

      cfg.pclk_idle_high    = 0;
      cfg.de_idle_high      = 1;
      
      _bus_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_0;
      _light_instance.config(cfg);
    }
    _panel_instance.light(&_light_instance);

    
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.y_min      = 0;
      cfg.x_max      = PANEL_W;
      cfg.y_max      = PANEL_H;
      cfg.bus_shared = false;
      cfg.offset_rotation = 3;
      
      cfg.i2c_port   = I2C_NUM_1;
      cfg.pin_sda    = GPIO_NUM_5;
      cfg.pin_scl    = GPIO_NUM_4;
      cfg.pin_int    = GPIO_NUM_6;
      cfg.pin_rst    = GPIO_NUM_NC;
      
      cfg.freq       = 100000;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
const esp_app_desc_t *desc = esp_ota_get_app_description();

// Display flushing 
static void lvgl_lcd_flash( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
    //uint32_t w = ( area->x2 - area->x1 + 1 );
    //uint32_t h = ( area->y2 - area->y1 + 1 );

    if (lcd.getStartCount() == 0)
    {   
        lcd.startWrite();
    }
    lcd.pushImageDMA( area->x1
                    , area->y1
                    , area->x2 - area->x1 + 1
                    , area->y2 - area->y1 + 1
                    , ( lgfx::rgb565_t* )&color_p->full);
    lv_disp_flush_ready( disp );

    /*
    lcd.startWrite();
    lcd.setAddrWindow( area->x1, area->y1, w, h );
    
    uint16_t c;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
        c = color_p->full;
        lcd.writeColor(c, 1);
        color_p++;
        }
    }    
    lv_disp_flush_ready( disp );
    */
}

void lvgl_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
    uint16_t touchX=0, touchY=0;
    bool touched = lcd.getTouch( &touchY, &touchX);
    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    } else
        {
            data->state = LV_INDEV_STATE_PR;  
            data->point.x = touchX;
            data->point.y = touchY;
            //printf( "Satir %d Colon %d\n", touchX, touchY );        
        }
    //return false;
}

static void lvgl_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}


void chip_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("chip_info:  features=0x%08X, cores=%u, revision=%u\n",
        chip_info.features, chip_info.cores, chip_info.revision);
    // particular features
    printf("EMB_FLASH...%s\n", (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Y" : "n");
        // Chip has embedded flash memory
    printf("WIFI_BGN....%s\n", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "Y" : "n");
        // Chip has 2.4GHz WiFi
    printf("BLE.........%s\n", (chip_info.features & CHIP_FEATURE_BLE) ? "Y" : "n");
        // Chip has Bluetooth LE
    printf("BT..........%s\n", (chip_info.features & CHIP_FEATURE_BT) ? "Y" : "n");
        // Chip has Bluetooth Classic
    printf("IEEE802154..%s\n", (chip_info.features & CHIP_FEATURE_IEEE802154) ? "Y" : "n");
        // Chip has IEEE 802.15.4,  low-rate wireless personal area network (LR-WPAN)
    printf("EMB_PSRAM...%s\n", (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "Y" : "n");
        // Chip has embedded psram

    // show model name
    printf("chip model=%u  ", (unsigned char)chip_info.model);
    if (chip_info.model == CHIP_ESP32) printf("ESP32\n");            // 1
    else if (chip_info.model == CHIP_ESP32S2) printf("ESP32-S2\n");  // 2
    else if (chip_info.model == CHIP_ESP32S3) printf("ESP32-S3\n");  // 9
    else if (chip_info.model == CHIP_ESP32C3) printf("ESP32-C3\n");  // 5
    else if (chip_info.model == CHIP_ESP32H2) printf("ESP32-H2\n");  // 6
    else printf("unknown\n");

    // show flash size
    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    // show size of PSRAM
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    long unsigned int sizePSRAM;
    sizePSRAM = (long unsigned int)(info.total_free_bytes + info.total_allocated_bytes);
    printf("PSRAM size = %lu\n", sizePSRAM);
}