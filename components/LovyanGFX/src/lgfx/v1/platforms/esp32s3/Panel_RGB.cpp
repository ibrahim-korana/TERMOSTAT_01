/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)
/----------------------------------------------------------------------------*/

#if defined (ESP_PLATFORM)
#include <sdkconfig.h>
#if defined (CONFIG_IDF_TARGET_ESP32S3)

#include "Panel_RGB.hpp"
#include "../../Bus.hpp"
#include "../common.hpp"

#include "Bus_RGB.hpp"

#include <soc/gpio_reg.h>
#include <soc/gpio_periph.h>

namespace lgfx
{
 inline namespace v1
 {
//----------------------------------------------------------------------------

  Panel_RGB::Panel_RGB(void)
  {
    _write_depth = color_depth_t::rgb565_2Byte;
    _read_depth = color_depth_t::rgb565_2Byte;
    // _write_depth = color_depth_t::rgb332_1Byte;
    // _read_depth = color_depth_t::rgb332_1Byte;
  }

  Panel_RGB::~Panel_RGB(void)
  {
    deinitFrameBuffer();
  }

  bool Panel_RGB::init(bool use_reset)
  {
    if (!Panel_FrameBufferBase::init(use_reset)) { return false; }

    auto h = _cfg.panel_height;
    auto frame_buffer_ = _bus->getDMABuffer(0);
    size_t lineArray_size = h * sizeof(void*);
    uint8_t** lineArray = (uint8_t**)heap_alloc_dma(lineArray_size);

    if (lineArray)
    {
      _lines_buffer = lineArray;
      memset(lineArray, 0, lineArray_size);

      uint8_t bits = _write_bits;
      int w = (_cfg.panel_width + 3) & ~3;
      if (frame_buffer_) {
        auto fb = frame_buffer_;
        for (int i = 0; i < h; ++i) {
          lineArray[i] = fb;
          fb += w * bits >> 3;
        }

        int32_t pin_cs = _config_detail.pin_cs;
        if (pin_cs >= 0) {
          lgfx::gpio_hi(pin_cs);
          lgfx::pinMode(pin_cs, pin_mode_t::output);
        }

        return true;
      }
      heap_free(lineArray);
    }

    return false;
  }
/*
  static inline uint8_t* sub_heap_alloc(bool flg_psram, size_t size)
  {
    uint8_t* res = nullptr;
    if (flg_psram) { res = (uint8_t*)heap_alloc_psram(size); }
    if (res == nullptr)
    {
      res = (uint8_t*)heap_alloc_dma(size);
    }
    if (res) { memset(res, 0, size); }
    return (uint8_t*)res;
  }
*/

  bool Panel_RGB::initFrameBuffer(uint_fast16_t w, uint_fast16_t h, color_depth_t depth, uint8_t chunk_lines, uint8_t use_psram)
  {
    size_t lineArray_size = h * sizeof(void*);
 //printf("height:%d\n", h);
    uint8_t** lineArray = (uint8_t**)heap_alloc_dma(lineArray_size);
    if (lineArray)
    {
      memset(lineArray, 0, lineArray_size);

      uint8_t bits = (depth & color_depth_t::bit_mask);
      w = (w + 3) & ~3;
// 暫定実装。画面全体のバッファを一括で確保する。
// ToDo : 分割確保
      _frame_buffer = (uint8_t*)heap_alloc_psram((w * bits >> 3) * h);
      if (_frame_buffer) {
        _lines_buffer = lineArray;
        auto fb = _frame_buffer;
        for (int i = 0; i < h; ++i) {
          lineArray[i] = fb;
          fb += w * bits >> 3;
        }
        return true;
      }
      heap_free(lineArray);
    }
    return false;
  }

  void Panel_RGB::deinitFrameBuffer(void)
  {
    if (_frame_buffer)
    {
      heap_free(_frame_buffer);
      _frame_buffer = nullptr;
    }

    if (_lines_buffer)
    {
      heap_free(_lines_buffer);
      _lines_buffer = nullptr;
    }
  }

  static void _write_swspi(uint32_t data, uint8_t bits, uint8_t pin_sclk, uint8_t pin_mosi)
  {
    uint_fast8_t mask = 1 << (bits - 1);
    do
    {
      gpio_lo(pin_sclk);

      if (data & mask) { 
        gpio_hi(pin_mosi);
        //printf("1"); 
        } else { 
          gpio_lo(pin_mosi);
          //printf("0");
         }
      gpio_hi(pin_sclk);
    } while (mask >>= 1);
    //printf(" ");
  }

  class _pin_backup_t
  {
  public:
    _pin_backup_t(gpio_num_t pin_num)
      : _io_mux_gpio_reg   { *reinterpret_cast<uint32_t*>(GPIO_PIN_MUX_REG[pin_num]) }
      , _gpio_func_out_reg { *reinterpret_cast<uint32_t*>(GPIO_FUNC0_OUT_SEL_CFG_REG + (pin_num * 4)) }
      , _pin_num           { pin_num }
    {}

    void restore(void) const
    {
      if ((uint32_t)_pin_num < GPIO_NUM_MAX) {
        *reinterpret_cast<uint32_t*>(GPIO_PIN_MUX_REG[_pin_num]) = _io_mux_gpio_reg;
        *reinterpret_cast<uint32_t*>(GPIO_FUNC0_OUT_SEL_CFG_REG + (_pin_num * 4)) = _gpio_func_out_reg;
      }
    }

  private:
    uint32_t _io_mux_gpio_reg;
    uint32_t _gpio_func_out_reg;
    gpio_num_t _pin_num;
  };

  void Panel_RGB::writeCommand(uint32_t data, uint_fast8_t len)
  {
    do
    {
 //printf("CMD: %02x ", data & 0xFF);
      _write_swspi(data & 0xFF, 9, _config_detail.pin_sclk, _config_detail.pin_mosi);
      data >>= 8;
    } while (--len);
   // printf("\n");
  }

  void Panel_RGB::writeData(uint32_t data, uint_fast8_t len)
  {
    do
    {
 //printf("DAT: %02x ", data & 0xFF);
      _write_swspi(data | 0x100, 9, _config_detail.pin_sclk, _config_detail.pin_mosi);
      data >>= 8;
    } while (--len);
   // printf("\n");
  }

//----------------------------------------------------------------------------

  const uint8_t* Panel_ST7701::getInitCommands(uint8_t listno) const
  {
    //printf("7701 init command %d\n", listno);
    static constexpr const uint8_t list0[] =
    {
          
      0xFF, 5, 0x77,0x01,0x00, 0x00, 0x13,  //Command Table 3
0xEF, 1, 0x08,

      0xFF, 5, 0x77,0x01,0x00, 0x00, 0x10,  //Command Table 0
      //0xC0, 2, 0x3B,0x00, //Line display setting
      //0xC1, 2, 0x0E,0x0C, //vsync porch control
      0xC2, 2, 0x07,0x0A, //0x0A, //Inversion selection & Frame Rate Control
0xCC, 1, 0x30,

      //Positive Voltage Gamma Control
      0xB0,16,0x40,0x07,0x53,0x0E,0x12,0x07,0x0A,0x09,
              0x09,0x26,0x05,0x10,0x0D,0x6E,0x3B,0xD6,
      //Negative Voltage Gamma Control              
      0xB1,16,0x40,0x17,0x5C,0x0D,0x11,0x06,0x08,0x08,
              0x08,0x22,0x03,0x12,0x11,0x65,0x28,0xE8,

      //Command Table 1
      0xFF, 5, 0x77,0x01,0x00, 0x00, 0x11,  
      0xB0, 1, 0x4D, //Vop Amplitude setting
      0xB1, 1, 0x4C, //VCOM amplitude setting
      0xB2, 1, 0x81, //VGH Voltage setting
      0xB3, 1, 0x80, //TEST Command Setting
      0xB5, 1, 0x4C, //VGL Voltage setting
      0xB7, 1, 0x85, //Power Control 1
      0xB8, 1, 0x33, //Power Control 2
      0xC1, 1, 0x78,  //Source pre_drive timing set1
      0xC2, 1, 0x78, //Source EQ2 Setting
      0xD0, 1, 0x88, //MIPI Setting 1
0xE0, 3, 0x00,0x00,0x02,
0xE1,11, 0x05,0x30,0x00,0x00,0x06,
         0x30,0x00,0x00,0x0E,0x30,0x30,
0xE2,12, 0x10,0x10,0x30,0x30,0xF4,0x00,
         0x00,0x00,0xF4,0x00,0x00,0x00,
0xE3,04, 0x00,0x00,0x11,0x11,
0xE4,02, 0x44,0x44,
0xE5,16, 0x0A,0xF4,0x30,0xF0,0x0C,0xF6,0x30,0xF0,
         0x06,0xF0,0x30,0xF0,0x08,0xF2,0x30,0xF0,
0xE6,04, 0x00,0x00,0x11,0x11,
0xE7,02, 0x44,0x44,
0xE8,16, 0x0B,0xF5,0x30,0xF0,0x0D,0xF7,0x30,0xF0,
         0x07,0xF1,0x30,0xF0,0x09,0xF3,0x30,0xF0,
0xE9,02, 0x36,0x01,
0xEB,07, 0x00,0x01,0xE4,0xE4,0x44,0x88,0x33,
0xED,16, 0x20,0xFA,0xB7,0x76,0x65,0x54,0x4F,0xFF,
         0xFF,0xF4,0x45,0x56,0x67,0x7B,0xAF,0x02,
0xEF,06, 0x10,0x0D,0x04,0x08,0x3F,0x1F,

      //Command Table 0
      0xFF, 5, 0x77,0x01,0x00, 0x00, 0x10,
      0x11, CMD_INIT_DELAY, 120, //Sleep Out
      0x3A, 1, 0x55, //Interface Pixel Format
      0x29, CMD_INIT_DELAY, 50, //Display ON
0xFF, 0xFF,

/*
      // Command2 BK0 SEL
      0xFF,  5, 0x77, 0x01, 0x00, 0x00, 0x10,

      //0xC1,  2, 0x0D, 0x02,
      0xC1,  2, 0x0E, 0x0C,
      0xC2,  2, 0x31, 0x05,
      0xCD,  1, 0x08,

      // Positive Voltage Gamma Control
      0xB0, 16, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
                0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,

      // Negative Voltage Gamma Control
      0xB1, 16, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
                0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,

      // Command2 BK1 SEL
      0xFF,  5, 0x77, 0x01, 0x00, 0x00, 0x11,

      0xB0,  1, 0x60, // Vop=4.7375v
      0xB1,  1, 0x32, // VCOM=32
      0xB2,  1, 0x07, // VGH=15v
      0xB3,  1, 0x80,
      0xB5,  1, 0x49, // VGL=-10.17v
      0xB7,  1, 0x85,
      0xB8,  1, 0x21, // AVDD=6.6 & AVCL=-4.6
      0xC1,  1, 0x78,
      0xC2,  1, 0x78,

      0xE0,  3, 0x00, 0x1B, 0x02,

      0xE1, 11, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44,
      0xE2, 12, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00,

      0xE3,  4, 0x00, 0x00, 0x11, 0x11,
      0xE4,  2, 0x44, 0x44,

      0xE5, 16, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
                0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0,

      0xE6,  4, 0x00, 0x00, 0x11, 0x11,

      0xE7,  2, 0x44, 0x44,

      0xE8, 16, 0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
                0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0,

      0xEB,  7, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,
      0xEC,  2, 0x3C, 0x00,
      0xED, 16, 0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
                0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA,

      //-----------VAP & VAN---------------
      // Command2 BK3 SEL
      0xFF,  5, 0x77, 0x01, 0x00, 0x00, 0x13,

      0xE5,  1, 0xE4,

      // Command2 BK0 SEL
      0xFF,  5, 0x77, 0x01, 0x00, 0x00, 0x00,

      0x21,  0,  // 0x20 normal, 0x21 IPS
      0x3A,  1, 0x55, // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565

      0x11, CMD_INIT_DELAY, 120, // Sleep Out

      0x29, 0, // Display On

      0xFF, 0xFF,
    */  
    };
    
    switch (listno)
    {
    case 0: return list0;
    default: return nullptr;
    }
  }

  bool Panel_ST7701::init(bool use_reset)
  {
    if (!Panel_RGB::init(use_reset))
    {
      return false;
    }

    int32_t pin_mosi = _config_detail.pin_mosi;
    int32_t pin_sclk = _config_detail.pin_sclk;

    //printf("PIN ST7701 SDA(Mosi)=%d SCL (sck)=%d\n",pin_mosi,pin_sclk);

    if (pin_mosi >= 0 && pin_sclk >= 0)
    {
      _pin_backup_t backup_pins[] = { (gpio_num_t)pin_mosi, (gpio_num_t)pin_sclk };
      lgfx::gpio_lo(pin_mosi);
      lgfx::pinMode(pin_mosi, pin_mode_t::output);
      lgfx::gpio_lo(pin_sclk);
      lgfx::pinMode(pin_sclk, pin_mode_t::output);


      int32_t pin_cs = _config_detail.pin_cs;
      lgfx::gpio_lo(pin_cs);

      writeCommand(0x01, 1);
      vTaskDelay(10/portTICK_PERIOD_MS);
      
     
      writeCommand(0xFF, 1);
      writeData(0x77, 1);
      writeData(0x01, 1);
      writeData(0x00, 1);
      writeData(0x00, 1);
      writeData(0x10, 1);

      // 0xC0 : LNSET : Display Line Setting
      
      writeCommand(0xC0, 1);
      uint32_t line1 = (_cfg.panel_height >> 3) + 1;
      uint32_t line2 = (_cfg.panel_height >> 1) & 3;

      //printf("panel %d line1 %02X line2 %02X\n",_cfg.panel_height,line1 + (line2 ? 0x80 : 0x00),line2);

      writeData(line1 + (line2 ? 0x80 : 0x00) , 1);
      writeData(line2, 1);
    
      auto cfg = ((Bus_RGB*)_bus)->config();

      //Porch control
      writeCommand(0xC1, 1);
      writeData(cfg.vsync_back_porch, 1);
      writeData(cfg.vsync_front_porch, 1);

      // 0xC3 : RGBCTRL
      writeCommand(0xC3, 1);
      uint32_t rgbctrl = 0;
      if ( cfg.de_idle_high  ) rgbctrl += 0x01;
      if ( cfg.pclk_idle_high) rgbctrl += 0x02;
      if (!cfg.hsync_polarity) rgbctrl += 0x04;
      if (!cfg.vsync_polarity) rgbctrl += 0x08;

      //rgbctrl += 0x80; //HV Mode
      //printf("RGBCTRL %02X\n",rgbctrl);
      writeData(rgbctrl, 1);
      writeData(0x10, 1); //Vsync Back poch 0x10
      writeData(0x08, 1); //Hsync Back Poch 0x08
      

      for (uint8_t i = 0; auto cmds = getInitCommands(i); i++)
      {
        command_list(cmds);
      }

      lgfx::gpio_hi(pin_cs);
      for (auto &bup : backup_pins) { bup.restore(); }
    }

    return true;
  }

//----------------------------------------------------------------------------
const uint8_t* Panel_NV3052::getInitCommands(uint8_t listno) const
  {
    static constexpr const uint8_t list0[] =
    {
      
      0xFF, 3, 0x30, 0x52, 0x01,  //Select Page 1
        
      0xE3, 1, 0x00,
      0x0A, 1, 0x01, //WRMADC_EN 0x00
      //0x23, 1, 0xA0, // RGB interface control
      0x23, 1, 0xA0, //
      0x24, 1, 0x0F,
      0x25, 1, 0x14,
      0x26, 1, 0x2E,
      0x27, 1, 0x2E,
      0x29, 1, 0x02,
      0x2A, 1, 0xCF,
      0x32, 1, 0x34,
      0x38, 1, 0x9C,
      0x39, 1, 0xA7,
      0x3A, 1, 0x4F,
      0x3B, 1, 0x94,
      0x40, 1, 0x07,
      0x42, 1, 0x6D,
      0x43, 1, 0x83,
      0x81, 1, 0x00,
      0x91, 1, 0x57,
      0x92, 1, 0x57,
      0xA0, 1, 0x52,
      0xA1, 1, 0x50,
      0xA4, 1, 0x9C,
      0xA7, 1, 0x02,
      0xA8, 1, 0x02,
      0xA9, 1, 0x02,
      0xAA, 1, 0xA8,
      0xAB, 1, 0x28,
      0xAE, 1, 0xD2,
      0xAF, 1, 0x02,
      0xB0, 1, 0xD2,
      0xB2, 1, 0x26,
      0xB3, 1, 0x26,

      0xFF, 3, 0x30, 0x52, 0x02,  //Select Page 2
      0xB0, 1, 0x02,
      0xB1, 1, 0x31,
      0xB2, 1, 0x24,
      0xB3, 1, 0x30,
      0xB4, 1, 0x38,
      0xB5, 1, 0x3E,
      0xB6, 1, 0x26,
      0xB7, 1, 0x3E,
      0xB8, 1, 0x0A,
      0xB9, 1, 0x00,
      0xBA, 1, 0x11,
      0xBB, 1, 0x11,
      0xBC, 1, 0x13,
      0xBD, 1, 0x14,
      0xBE, 1, 0x18,
      0xBF, 1, 0x11,
      0xC0, 1, 0x16,
      0xC1, 1, 0x00,
      0xD0, 1, 0x05,
      0xD1, 1, 0x30,
      0xD2, 1, 0x25,
      0xD3, 1, 0x35,
      0xD4, 1, 0x34,
      0xD5, 1, 0x3B,
      0xD6, 1, 0x26,
      0xD7, 1, 0x3D,
      0xD8, 1, 0x0A,
      0xD9, 1, 0x00,
      0xDA, 1, 0x12,
      0xDB, 1, 0x10,
      0xDC, 1, 0x12,
      0xDD, 1, 0x14,
      0xDE, 1, 0x18,
      0xDF, 1, 0x11,
      0xE0, 1, 0x15,
      0xE1, 1, 0x00,
      0xFF, 3, 0x30, 0x52, 0x03,  //Select Page 3

      0x00, 1, 0x00, 
      0x01, 1, 0x00,
      0x02, 1, 0x00,
      0x03, 1, 0x00,
      0x08, 1, 0x0D,
      0x09, 1, 0x0E,
      0x0A, 1, 0x0F,
      0x0B, 1, 0x10,
      0x20, 1, 0x00,
      0x21, 1, 0x00,
      0x22, 1, 0x00,
      0x23, 1, 0x00,
      0x28, 1, 0x22,
      0x2A, 1, 0xE9,
      0x2B, 1, 0xE9,
      0x30, 1, 0x00,
      0x31, 1, 0x00,
      0x32, 1, 0x00,
      0x33, 1, 0x00,
      0x34, 1, 0x01,
      0x35, 1, 0x00,
      0x36, 1, 0x00,
      0x37, 1, 0x03,
      0x40, 1, 0x0A,
      0x41, 1, 0x0B,
      0x42, 1, 0x0C,
      0x43, 1, 0x0D,
      0x44, 1, 0x22,
      0x45, 1, 0xE4,
      0x46, 1, 0xE5,
      0x47, 1, 0x22,
      0x48, 1, 0xE6,
      0x49, 1, 0xE7,
      0x50, 1, 0x0E,
      0x51, 1, 0x0F,
      0x52, 1, 0x10,
      0x53, 1, 0x11,
      0x54, 1, 0x22,
      0x55, 1, 0xE8,
      0x56, 1, 0xE9, 
      0x57, 1, 0x22,
      0x58, 1, 0xEA, 
      0x59, 1, 0xEB,
      0x60, 1, 0x05,
      0x61, 1, 0x05,
      0x65, 1, 0x0A,
      0x66, 1, 0x0A,
      0x80, 1, 0x05,
      0x81, 1, 0x00,
      0x82, 1, 0x02,
      0x83, 1, 0x04, 
      0x84, 1, 0x00,
      0x85, 1, 0x00, 
      0x86, 1, 0x1F, 
      0x87, 1, 0x1F,
      0x88, 1, 0x0A, 
      0x89, 1, 0x0C,
      0x8A, 1, 0x0E,
      0x8B, 1, 0x10,
      0x96, 1, 0x05,
      0x97, 1, 0x00, 
      0x98, 1, 0x01, 
      0x99, 1, 0x03, 
      0x9A, 1, 0x00, 
      0x9B, 1, 0x00,
      0x9C, 1, 0x1F,
      0x9D, 1, 0x1F,
      0x9E, 1, 0x09,
      0x9F, 1, 0x0B, 
      0xA0, 1, 0x0D, 
      0xA1, 1, 0x0F, 
      0xB0, 1, 0x05, 
      0xB1, 1, 0x1F, 
      0xB2, 1, 0x03, 
      0xB3, 1, 0x01, 
      0xB4, 1, 0x00, 
      0xB5, 1, 0x00, 
      0xB6, 1, 0x1F, 
      0xB7, 1, 0x00,
      0xB8, 1, 0x0F, 
      0xB9, 1, 0x0D,
      0xBA, 1, 0x0B, 
      0xBB, 1, 0x09, 
      0xC6, 1, 0x05,
      0xC7, 1, 0x1F, 
      0xC8, 1, 0x04,
      0xC9, 1, 0x02, 
      0xCA, 1, 0x00, 
      0xCB, 1, 0x00, 
      0xCC, 1, 0x1F, 
      0xCD, 1, 0x00, 
      0xCE, 1, 0x10,
      0xCF, 1, 0x0E, 
      0xD0, 1, 0x0C,
      0xD1, 1, 0x0A,

        0xFF, 3, 0x30, 0x52, 0x00,  //Select Page 0
        //3A komutu RGB nin bit sayısını ayarlar 565 16 bit, 666 18 bit veya 888 24 bit
        //Normalde parametre 5 6 gibi olmalı ancak 55 66 gibi yazıldığında renkler daha
        //parlak oluyor 
        //0x3A, 1, 0x66, //RGB 666
        0x3A, 1, 0x55, //RGB 565
        //36 komutu Renklerde RGB/BGR ayrımı ve Hflip, VFlip işlemleri için kullanılıyor
        //detay için datasheet   
        0x36, 1, 0x02,
        //11 komutu sleep out için kullanılır. Bu komuttan sonra en az 5ms beklenmelidir
        0x11, CMD_INIT_DELAY, 200, 
        //29 komutu display on için kullanılır
        0x29,  CMD_INIT_DELAY, 200, 
        //35 komutu TE enable
        0x35, 1, 0x01,
        0xFF, 0xFF
    };

    switch (listno)
    {
      case 0: return list0;
      default: return nullptr;
    }
  }

bool Panel_NV3052::init(bool use_reset)
  {
    if (!Panel_RGB::init(use_reset))
    {
      return false;
    }

    int32_t pin_mosi = _config_detail.pin_mosi;
    int32_t pin_sclk = _config_detail.pin_sclk;
    
    //printf("PIN Panel_NV3052 %d %d %d\n",pin_mosi,pin_sclk, _config_detail.pin_cs);


    if (pin_mosi >= 0 && pin_sclk >= 0)
    {
      _pin_backup_t backup_pins[] = { (gpio_num_t)pin_mosi, (gpio_num_t)pin_sclk };
      lgfx::gpio_lo(pin_mosi);
      lgfx::pinMode(pin_mosi, pin_mode_t::output);
      lgfx::gpio_lo(pin_sclk);
      lgfx::pinMode(pin_sclk, pin_mode_t::output);

      int32_t pin_cs = _config_detail.pin_cs;
      lgfx::gpio_lo(pin_cs);

      for (uint8_t i = 0; auto cmds = getInitCommands(i); i++)
      {
        command_list(cmds);
      }

      lgfx::gpio_hi(pin_cs);
      for (auto &bup : backup_pins) { bup.restore(); }
    }

    return true;
  }

//----------------------------------------------------------------------------

  const uint8_t* Panel_GC9503::getInitCommands(uint8_t listno) const
  {
    static constexpr const uint8_t list0[] =
    {
      0xF0,  5, 0x55, 0xAA, 0x52, 0x08, 0x00,
      0xF6,  2, 0x5A, 0x87,
      0xC1,  1, 0x3F,
      0xC2,  1, 0x0E,
      0xC6,  1, 0xF8,
      0xC9,  1, 0x10,
      0xCD,  1, 0x25,
      0xF8,  1, 0x8A,
      0xAC,  1, 0x45,
      0xA0,  1, 0xDD,
      0xA7,  1, 0x47,

      0xFA,  4, 0x00, 0x00, 0x00, 0x04,
      0xA3,  1, 0xEE,

      0xFD,  3, 0x28, 0x28, 0x00,

      0x71,  1, 0x48,
      0x72,  1, 0x48,
      0x73,  2, 0x00, 0x44,
      0x97,  1, 0xEE,
      0x83,  1, 0x93,
      0x9A,  1, 0x72,
      0x9B,  1, 0x5a,
      0x82,  2, 0x2c, 0x2c,
      0xB1,  1, 0x10,

      0x6D, 32, 0x00, 0x1F, 0x19, 0x1A, 0x10, 0x0e, 0x0c, 0x0a,
                0x02, 0x07, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
                0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x08, 0x01,
                0x09, 0x0b, 0x0D, 0x0F, 0x1a, 0x19, 0x1f, 0x00,

      0x64, 16, 0x38, 0x05, 0x01, 0xdb, 0x03, 0x03, 0x38, 0x04,
                0x01, 0xdc, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A,

      0x65, 16, 0x38, 0x03, 0x01, 0xdd, 0x03, 0x03, 0x38, 0x02,
                0x01, 0xde, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A,

      0x66, 16, 0x38, 0x01, 0x01, 0xdf, 0x03, 0x03, 0x38, 0x00,
                0x01, 0xe0, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A,

      0x67, 16, 0x30, 0x01, 0x01, 0xe1, 0x03, 0x03, 0x30, 0x02,
                0x01, 0xe2, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A,

      0x68, 13, 0x00, 0x08, 0x15, 0x08, 0x15, 0x7A, 0x7A, 0x08,
                0x15, 0x08, 0x15, 0x7A, 0x7A,

      0x60,  8, 0x38, 0x08, 0x7A, 0x7A, 0x38, 0x09, 0x7A, 0x7A,

      0x63,  8, 0x31, 0xe4, 0x7A, 0x7A, 0x31, 0xe5, 0x7A, 0x7A,

      0x6B,  1, 0x07,

      0x7A,  2, 0x08, 0x13,

      0x7B,  2, 0x08, 0x13,

      0xD1, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0xD2, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0xD3, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0xD4, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0xD5, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0xD6, 52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x00, 0x18,
                0x00, 0x21, 0x00, 0x2a, 0x00, 0x35, 0x00, 0x47,
                0x00, 0x56, 0x00, 0x90, 0x00, 0xe5, 0x01, 0x68,
                0x01, 0xd5, 0x01, 0xd7, 0x02, 0x36, 0x02, 0xa6,
                0x02, 0xee, 0x03, 0x48, 0x03, 0xa0, 0x03, 0xba,
                0x03, 0xc5, 0x03, 0xd0, 0x03, 0xE0, 0x03, 0xea,
                0x03, 0xFa, 0x03, 0xFF,

      0x3a,  1, 0x66,

      0x11, CMD_INIT_DELAY, 200, // Sleep Out

      0x29,  0,
      0xFF, 0xFF
    };
    switch (listno)
    {
    case 0: return list0;
    default: return nullptr;
    }
  }

  bool Panel_GC9503::init(bool use_reset)
  {
    if (!Panel_RGB::init(use_reset))
    {
      return false;
    }

    int32_t pin_mosi = _config_detail.pin_mosi;
    int32_t pin_sclk = _config_detail.pin_sclk;
    
    printf("PIN GC9503 %d %d\n",pin_mosi,pin_sclk);


    if (pin_mosi >= 0 && pin_sclk >= 0)
    {
      _pin_backup_t backup_pins[] = { (gpio_num_t)pin_mosi, (gpio_num_t)pin_sclk };
      lgfx::gpio_lo(pin_mosi);
      lgfx::pinMode(pin_mosi, pin_mode_t::output);
      lgfx::gpio_lo(pin_sclk);
      lgfx::pinMode(pin_sclk, pin_mode_t::output);

      int32_t pin_cs = _config_detail.pin_cs;
      lgfx::gpio_lo(pin_cs);

      for (uint8_t i = 0; auto cmds = getInitCommands(i); i++)
      {
        command_list(cmds);
      }

      lgfx::gpio_hi(pin_cs);
      for (auto &bup : backup_pins) { bup.restore(); }
    }

    return true;
  }

 }
}
#endif
#endif