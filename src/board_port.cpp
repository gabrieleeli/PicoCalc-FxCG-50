#include "board_port.hpp"
#include "board_config.hpp"
#include "bt_support.hpp"
#include "font_renderer.hpp"
#include "mem_pools.hpp"
#include "psram.hpp"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "lcd_spi.pio.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

// PicoCalc v2 display: 320x320 RGB565, ST7365P-compatible command set.
// LCD signals use a small PIO SPI transmitter on GP10/GP11 (pins from board_config.hpp).
namespace board {
namespace {
namespace cfg = board::config;
constexpr int WIDTH = static_cast<int>(cfg::kLcdWidth), HEIGHT = static_cast<int>(cfg::kLcdHeight);
constexpr std::size_t FRAME_PIXELS = static_cast<std::size_t>(WIDTH) * HEIGHT;
constexpr std::size_t FRAME_BYTES = FRAME_PIXELS * sizeof(std::uint16_t);
constexpr uint8_t REG_ID_FIF = 0x09;
constexpr int LCD_Y_OFFSET = 0; // Keep configurable until verified against each panel revision.
constexpr uint32_t LCD_SPI_HZ = 12'500'000;
constexpr uint LCD_SM = 0;
inline PIO lcdPio() { return pio0; }

// Framebuffer placement. Default: static SRAM buffer (DMA-safe, no XIP cache
// pressure, stays usable during flash writes). FXAI_FRAMEBUFFER_IN_PSRAM moves
// it to the PSRAM pool and frees 200 KiB of SRAM for the heap.
#if FXAI_FRAMEBUFFER_IN_PSRAM
std::uint16_t* framebufferPixels = nullptr;
#else
alignas(4) std::uint16_t staticFramebuffer[FRAME_PIXELS];
std::uint16_t* framebufferPixels = staticFramebuffer;
#endif
bool framebufferInPsram = false;
gfx::Canvas canvas(nullptr, WIDTH, HEIGHT);
bool lcd_ready = false;
bool wifi_ready = false;
bool wifi_sta = false;
std::uint32_t presentUs = 0, fullFrameUs = 0;

inline void cs(bool active){gpio_put(cfg::kLcdCs,active?0:1);}
inline void dc(bool data){gpio_put(cfg::kLcdDc,data?1:0);}
// Wait until the state machine has shifted out the last bit: clear the TX
// stall flag, then wait for the program to stall on an empty FIFO again.
void wait_tx(){
    const uint32_t mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + LCD_SM);
    lcdPio()->fdebug = mask;
    while(!(lcdPio()->fdebug & mask)) tight_loop_contents();
}
void writeByte(uint8_t v){pio_sm_put_blocking(lcdPio(),LCD_SM,static_cast<uint32_t>(v)<<24);}
void command(uint8_t c){cs(true);dc(false);writeByte(c);wait_tx();cs(false);}
void commandData(uint8_t c,const uint8_t* data,size_t n){cs(true);dc(false);writeByte(c);wait_tx();if(n){dc(true);for(size_t i=0;i<n;++i)writeByte(data[i]);wait_tx();}cs(false);}
void setWindow(int x0,int y0,int x1,int y1){
    uint16_t xa=static_cast<uint16_t>(x0), xb=static_cast<uint16_t>(x1);
    uint16_t ya=static_cast<uint16_t>(y0+LCD_Y_OFFSET), yb=static_cast<uint16_t>(y1+LCD_Y_OFFSET);
    uint8_t d[4];
    d[0]=static_cast<uint8_t>(xa>>8);d[1]=static_cast<uint8_t>(xa&255);d[2]=static_cast<uint8_t>(xb>>8);d[3]=static_cast<uint8_t>(xb&255);commandData(0x2A,d,4);
    d[0]=static_cast<uint8_t>(ya>>8);d[1]=static_cast<uint8_t>(ya&255);d[2]=static_cast<uint8_t>(yb>>8);d[3]=static_cast<uint8_t>(yb&255);commandData(0x2B,d,4);
    command(0x2C);
}
void lcdInit(){
    gpio_init(cfg::kLcdCs);gpio_set_dir(cfg::kLcdCs,GPIO_OUT);gpio_put(cfg::kLcdCs,1);
    gpio_init(cfg::kLcdDc);gpio_set_dir(cfg::kLcdDc,GPIO_OUT);gpio_put(cfg::kLcdDc,1);
    gpio_init(cfg::kLcdReset);gpio_set_dir(cfg::kLcdReset,GPIO_OUT);gpio_put(cfg::kLcdReset,1);
    sleep_ms(5);gpio_put(cfg::kLcdReset,0);sleep_ms(20);gpio_put(cfg::kLcdReset,1);sleep_ms(120);

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

void sendPixels(const uint16_t* pixels,size_t count){
    cs(true);dc(true);
    for(size_t i=0;i<count;++i){writeByte(static_cast<uint8_t>(pixels[i]>>8));writeByte(static_cast<uint8_t>(pixels[i]&0xff));}
    wait_tx();cs(false);
}

void allocateFramebuffer(){
#if FXAI_FRAMEBUFFER_IN_PSRAM
    framebufferPixels = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Policy::PSRAM_ONLY, FRAME_BYTES, 32));
    framebufferInPsram = framebufferPixels != nullptr;
    if(!framebufferPixels){
        // PSRAM missing or failed its self-test: the SRAM heap has room because
        // no static framebuffer was linked in this configuration.
        framebufferPixels = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Pool::SRAM, FRAME_BYTES, 4));
        std::printf("[lcd] framebuffer requested in PSRAM, falling back to SRAM\n");
    }
#endif
    if(framebufferPixels) std::fill(framebufferPixels, framebufferPixels + FRAME_PIXELS, 0);
    canvas.attach(framebufferPixels, framebufferPixels ? WIDTH : 0, framebufferPixels ? HEIGHT : 0);
}
}

void init(){
    gpio_init(cfg::kKeyboardSda);gpio_set_function(cfg::kKeyboardSda,GPIO_FUNC_I2C);gpio_pull_up(cfg::kKeyboardSda);
    gpio_init(cfg::kKeyboardScl);gpio_set_function(cfg::kKeyboardScl,GPIO_FUNC_I2C);gpio_pull_up(cfg::kKeyboardScl);
    i2c_init(i2c1,cfg::kKeyboardI2cHz);
    const uint offset=pio_add_program(lcdPio(),&picocalc_lcd_spi_program);
    pio_gpio_init(lcdPio(),cfg::kLcdMosi);pio_gpio_init(lcdPio(),cfg::kLcdSck);
    pio_sm_config pcfg=picocalc_lcd_spi_program_get_default_config(offset);
    sm_config_set_out_pins(&pcfg,cfg::kLcdMosi,1);
    sm_config_set_sideset_pins(&pcfg,cfg::kLcdSck);
    sm_config_set_out_shift(&pcfg,false,true,8); // MSB first, one FIFO word per byte
    sm_config_set_fifo_join(&pcfg,PIO_FIFO_JOIN_TX);
    float div=static_cast<float>(clock_get_hz(clk_sys))/(static_cast<float>(LCD_SPI_HZ)*4.0f);
    sm_config_set_clkdiv(&pcfg,div);
    pio_sm_set_consecutive_pindirs(lcdPio(),LCD_SM,cfg::kLcdMosi,1,true);
    pio_sm_set_consecutive_pindirs(lcdPio(),LCD_SM,cfg::kLcdSck,1,true);
    pio_sm_init(lcdPio(),LCD_SM,offset,&pcfg);pio_sm_set_enabled(lcdPio(),LCD_SM,true);
    allocateFramebuffer();
    lcdInit();
    lcd_ready=framebufferPixels!=nullptr;
    clear(0x0000);present();
    // CYW43 pins come from the SDK board header (pico2_w.h / pimoroni_pico_plus2_w_rp2350.h).
    wifi_ready = cyw43_arch_init()==0;
    if(!wifi_ready) std::printf("[wifi] cyw43_arch_init failed\n");
    else if(fx::bt::enabled()) std::printf("[bt] HCI power on: %s\n", fx::bt::init()?"ok":"failed");
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
    const uint64_t start=time_us_64();
    if(x0==0&&x1==WIDTH){
        setWindow(x0,y0,x1-1,y1-1);
        sendPixels(framebufferPixels+static_cast<size_t>(y0)*WIDTH,static_cast<size_t>(y1-y0)*WIDTH);
    } else {
        // Reissue RAMWR for each short row: this avoids relying on a panel's
        // behavior when CS is toggled between non-contiguous framebuffer rows.
        for(int y=y0;y<y1;++y){
            setWindow(x0,y,x1-1,y);
            sendPixels(framebufferPixels+static_cast<size_t>(y)*WIDTH+x0,static_cast<size_t>(x1-x0));
        }
    }
    presentUs=static_cast<std::uint32_t>(time_us_64()-start);
    if(x0==0&&y0==0&&x1==WIDTH&&y1==HEIGHT) fullFrameUs=presentUs;
}
int readKey(){const uint8_t reg=REG_ID_FIF;int w=i2c_write_timeout_us(i2c1,cfg::kKeyboardAddress,&reg,1,true,5000);if(w<0)return -1;uint8_t packet[2]{};int r=i2c_read_timeout_us(i2c1,cfg::kKeyboardAddress,packet,2,false,5000);if(r<0)return -1;return (static_cast<int>(packet[0])<<8)|packet[1];}
bool wifiConnect(const char* ssid,const char* password){
    if(!wifi_ready||!ssid)return false;
    if(!wifi_sta){cyw43_arch_enable_sta_mode();wifi_sta=true;} // STA mode is required before connecting
    int r=cyw43_arch_wifi_connect_timeout_ms(ssid,password,CYW43_AUTH_WPA2_AES_PSK,15000);
    return r==0;
}

FramebufferInfo framebuffer() noexcept {
    return {framebufferPixels, framebufferPixels ? FRAME_BYTES : 0, framebufferInPsram, FXAI_FRAMEBUFFER_IN_PSRAM != 0};
}
std::uint32_t lastPresentUs() noexcept { return presentUs; }
std::uint32_t lastFullFrameUs() noexcept { return fullFrameUs; }
std::uint32_t benchmarkPresentFrom(const std::uint16_t* pixels){
    if(!lcd_ready||!pixels)return 0;
    const uint64_t start=time_us_64();
    setWindow(0,0,WIDTH-1,HEIGHT-1);
    sendPixels(pixels,FRAME_PIXELS);
    return static_cast<std::uint32_t>(time_us_64()-start);
}
std::uint32_t benchmarkFullPresent(){
    fullFrameUs=benchmarkPresentFrom(framebufferPixels);
    return fullFrameUs;
}
bool wifiReady() noexcept { return wifi_ready; }
bool setSystemClockKhz(std::uint32_t khz){
    if(!set_sys_clock_khz(khz,false)) return false;
    psram::reapplyTiming(); // M1 CLKDIV/MAX_SELECT/MIN_DESELECT depend on clk_sys
    const float div=static_cast<float>(clock_get_hz(clk_sys))/(static_cast<float>(LCD_SPI_HZ)*4.0f);
    pio_sm_set_clkdiv(lcdPio(),LCD_SM,div);
    return true;
}
namespace {
int scanResult(void* env,const cyw43_ev_scan_result_t* result){
    if(result) ++*static_cast<int*>(env);
    return 0;
}
}
int wifiScan(unsigned timeoutMs){
    if(!wifi_ready) return -1;
    if(!wifi_sta){cyw43_arch_enable_sta_mode();wifi_sta=true;}
    cyw43_wifi_scan_options_t options{};
    static int count=0;
    count=0;
    if(cyw43_wifi_scan(&cyw43_state,&options,&count,scanResult)!=0) return -1;
    const absolute_time_t until=make_timeout_time_ms(timeoutMs);
    while(cyw43_wifi_scan_active(&cyw43_state)&&!time_reached(until)) sleep_ms(50);
    return count;
}
const char* wifiStatus() noexcept {
    if(!wifi_ready) return "CYW43 non inizializzato";
    if(!wifi_sta) return "pronto (STA non attivo)";
    switch(cyw43_wifi_link_status(&cyw43_state,CYW43_ITF_STA)){
    case CYW43_LINK_JOIN: return "connesso";
    case CYW43_LINK_FAIL: return "errore di connessione";
    case CYW43_LINK_NONET: return "rete non trovata";
    case CYW43_LINK_BADAUTH: return "autenticazione fallita";
    default: return "non connesso";
    }
}
}
