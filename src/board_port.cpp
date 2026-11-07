#include "board_port.hpp"
#include "font_renderer.hpp"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "lcd_spi.pio.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>

// PicoCalc v2 display: 320x320 RGB565, ST7365P-compatible command set.
// LCD signals use a small PIO SPI transmitter on GP10/GP11.
namespace board {
namespace {
constexpr int LCD_SCK=10, LCD_MOSI=11, LCD_CS=13, LCD_DC=14, LCD_RST=15;
constexpr int KEY_SDA=6, KEY_SCL=7;
constexpr uint8_t KEYBOARD_ADDR=0x1F, REG_ID_FIF=0x09;
constexpr int WIDTH=320, HEIGHT=320;
constexpr int LCD_Y_OFFSET=0; // Keep configurable until verified against each panel revision.
constexpr uint32_t LCD_SPI_HZ=12'500'000;
constexpr PIO LCD_PIO=pio0;
constexpr uint LCD_SM=0;
uint16_t framebuffer[WIDTH*HEIGHT];
gfx::Canvas canvas(framebuffer, WIDTH, HEIGHT);
bool lcd_ready=false;

inline void cs(bool active){gpio_put(LCD_CS,active?0:1);}
inline void dc(bool data){gpio_put(LCD_DC,data?1:0);}
void wait_tx(){while(!pio_sm_is_tx_stalled(LCD_PIO,LCD_SM)) tight_loop_contents();}
void writeByte(uint8_t v){pio_sm_put_blocking(LCD_PIO,LCD_SM,static_cast<uint32_t>(v)<<24);}
void command(uint8_t c){cs(true);dc(false);writeByte(c);wait_tx();cs(false);}
void commandData(uint8_t c,const uint8_t* data,size_t n){cs(true);dc(false);writeByte(c);wait_tx();if(n){dc(true);for(size_t i=0;i<n;++i)writeByte(data[i]);wait_tx();}cs(false);}
void setWindow(int x0,int y0,int x1,int y1){
    uint16_t xa=static_cast<uint16_t>(x0), xb=static_cast<uint16_t>(x1);
    uint16_t ya=static_cast<uint16_t>(y0+LCD_Y_OFFSET), yb=static_cast<uint16_t>(y1+LCD_Y_OFFSET);
    uint8_t d[4];
    d[0]=xa>>8;d[1]=xa&255;d[2]=xb>>8;d[3]=xb&255;commandData(0x2A,d,4);
    d[0]=ya>>8;d[1]=ya&255;d[2]=yb>>8;d[3]=yb&255;commandData(0x2B,d,4);
    command(0x2C);
}
void lcdInit(){
    gpio_init(LCD_CS);gpio_set_dir(LCD_CS,GPIO_OUT);gpio_put(LCD_CS,1);
    gpio_init(LCD_DC);gpio_set_dir(LCD_DC,GPIO_OUT);gpio_put(LCD_DC,1);
    gpio_init(LCD_RST);gpio_set_dir(LCD_RST,GPIO_OUT);gpio_put(LCD_RST,1);
    sleep_ms(5);gpio_put(LCD_RST,0);sleep_ms(20);gpio_put(LCD_RST,1);sleep_ms(120);

    // ST7365P/ILI9488-compatible startup sequence. Values are conservative;
    // final gamma/orientation tuning must be checked on the user's panel.
    command(0x01);sleep_ms(10);
    const uint8_t c0[]={0x17,0x15}; commandData(0xC0,c0,sizeof(c0));
    const uint8_t c1[]={0x41}; commandData(0xC1,c1,sizeof(c1));
    const uint8_t c5[]={0x00,0x12,0x80}; commandData(0xC5,c5,sizeof(c5));
    const uint8_t madctl[]={0x48}; commandData(0x36,madctl,sizeof(madctl));
    const uint8_t colmod[]={0x55}; commandData(0x3A,colmod,sizeof(colmod));
    const uint8_t f1[]={0xA0}; commandData(0xB0,f1,sizeof(f1));
    const uint8_t f2[]={0x02,0x02,0x3B}; commandData(0xB1,f2,sizeof(f2));
    const uint8_t f3[]={0x00,0x00}; commandData(0xB4,f3,sizeof(f3));
    const uint8_t f4[]={0x02}; commandData(0xB6,f4,sizeof(f4));
    command(0x11);sleep_ms(120);command(0x29);sleep_ms(20);
}

// Compact 5x7 font. Each byte is one vertical glyph column, LSB at the top.

void drawChar(int x,int y,char c,uint16_t color){const uint8_t* g=glyph(c);for(int col=0;col<5;++col)for(int row=0;row<7;++row)if(g[col]&(1u<<row)){int px=x+col,py=y+row;if(px>=0&&px<WIDTH&&py>=0&&py<HEIGHT)framebuffer[py*WIDTH+px]=color;}}
void sendPixels(const uint16_t* pixels,size_t count){
    cs(true);dc(true);
    for(size_t i=0;i<count;++i){writeByte(static_cast<uint8_t>(pixels[i]>>8));writeByte(static_cast<uint8_t>(pixels[i]&0xff));}
    wait_tx();cs(false);
}
}

void init(){
    stdio_init_all();
    gpio_init(KEY_SDA);gpio_set_function(KEY_SDA,GPIO_FUNC_I2C);gpio_pull_up(KEY_SDA);
    gpio_init(KEY_SCL);gpio_set_function(KEY_SCL,GPIO_FUNC_I2C);gpio_pull_up(KEY_SCL);
    i2c_init(i2c1,10000);
    const uint offset=pio_add_program(LCD_PIO,&picocalc_lcd_spi_program);
    pio_gpio_init(LCD_PIO,LCD_MOSI);pio_gpio_init(LCD_PIO,LCD_SCK);
    pio_sm_config cfg=picocalc_lcd_spi_program_get_default_config(offset);
    sm_config_set_out_pins(&cfg,LCD_MOSI,1);
    sm_config_set_sideset_pins(&cfg,LCD_SCK);
    sm_config_set_out_shift(&cfg,false,true,8); // MSB first, one FIFO word per byte
    sm_config_set_fifo_join(&cfg,PIO_FIFO_JOIN_TX);
    float div=static_cast<float>(clock_get_hz(clk_sys))/(static_cast<float>(LCD_SPI_HZ)*4.0f);
    sm_config_set_clkdiv(&cfg,div);
    pio_sm_set_consecutive_pindirs(LCD_PIO,LCD_SM,LCD_MOSI,1,true);
    pio_sm_set_consecutive_pindirs(LCD_PIO,LCD_SM,LCD_SCK,1,true);
    pio_sm_init(LCD_PIO,LCD_SM,offset,&cfg);pio_sm_set_enabled(LCD_PIO,LCD_SM,true);
    lcdInit();
    std::fill(std::begin(framebuffer),std::end(framebuffer),0);
    lcd_ready=true;
    clear(0x0000);present();
    if(cyw43_arch_init()==0) { /* Wi-Fi is enabled lazily by wifiConnect(). */ }
}
gfx::Canvas& graphics() noexcept { return canvas; }
void clear(uint16_t c){canvas.resetClip();canvas.clear(c);}
void pixel(int x,int y,uint16_t c){canvas.pixel(x,y,c);}
void rect(int x,int y,int w,int h,uint16_t c,bool filled){canvas.rect(x,y,w,h,c,filled);}
void text(int x,int y,const char* s,uint16_t c){font::drawText(canvas,x,y,s,c,1,WIDTH-x);}
void present(){
    if(!lcd_ready)return;
    gfx::Rect dirty=canvas.consumeDirty();
    if(dirty.w<=0||dirty.h<=0)return;
    const int x0=std::max(0,dirty.x),y0=std::max(0,dirty.y);
    const int x1=std::min(WIDTH,dirty.x+dirty.w),y1=std::min(HEIGHT,dirty.y+dirty.h);
    if(x0>=x1||y0>=y1)return;
    if(x0==0&&x1==WIDTH){
        setWindow(x0,y0,x1-1,y1-1);
        sendPixels(framebuffer+static_cast<size_t>(y0)*WIDTH,static_cast<size_t>(y1-y0)*WIDTH);
    } else {
        // Reissue RAMWR for each short row: this avoids relying on a panel's
        // behavior when CS is toggled between non-contiguous framebuffer rows.
        for(int y=y0;y<y1;++y){
            setWindow(x0,y,x1-1,y);
            sendPixels(framebuffer+static_cast<size_t>(y)*WIDTH+x0,static_cast<size_t>(x1-x0));
        }
    }
}
int readKey(){const uint8_t reg=REG_ID_FIF;int w=i2c_write_timeout_us(i2c1,KEYBOARD_ADDR,&reg,1,true,5000);if(w<0)return -1;uint8_t packet[2]{};int r=i2c_read_timeout_us(i2c1,KEYBOARD_ADDR,packet,2,false,5000);if(r<0)return -1;return (static_cast<int>(packet[0])<<8)|packet[1];}
bool wifiConnect(const char* ssid,const char* password){int r=cyw43_arch_wifi_connect_timeout_ms(ssid,password,CYW43_AUTH_WPA2_AES_PSK,15000);return r==0;}
}
