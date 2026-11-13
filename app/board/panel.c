/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 珠海笙科智能科技有限公司 (M-VAVE).
 * Connect the FM1 control API to the board scanner. */
#include "smk37_hw.h"
#include "fm1_lcd_hw.h"
#include "lcd.inc"

static uint16_t button_leds;
/* KamoteKeys 0.41: 320 leading zero bytes (~400 us low at the default timing)
 * form a valid WS2812 reset (>= 280 us) in front of every frame, so a stray
 * pulse when SPI1 is switched over can no longer prepend a bit and shift the
 * whole chain (pad 1 blinking green, colour bits spilling between pads).
 * SMK37 OPEN's 36 bytes (~45 us) were shorter than the reset time. Four
 * trailing zero bytes leave the line low. */
enum { RGB_LEAD = 320, RGB_TAIL = 4 };
static uint8_t rgb_buffer[RGB_LEAD + 16 * 24 + RGB_TAIL] __attribute__((aligned(4)));
static uint32_t rgb_colors[16];
static unsigned rgb_dirty=0xffff;
static const uint8_t rgb_map[16]={0,15,14,13,12,11,10,9,1,2,3,4,5,6,7,8};
/* PH8 is an encoder input on both boards, with no fitted TRS MIDI RX. */
uint8_t fm1_io_trs_midi_enabled(void) {return 0;}
void fm1_controls_init(void) {smk37_scan_init();}
/* KamoteKeys: the scanner delivers every event straight to the performance
 * core (smk37_control_event). This snapshot only reports the battery ADC;
 * button/pad LEDs are produced by app/kamotkeys/kk_led.c. */
void fm1_controls_read(struct fm1_controls *state)
{
    smk37_scan_read();
    memset(state,0,sizeof *state);
    state->battery=smk37_battery_adc();
}
void fm1_led_set(unsigned key,int on)
{
    static const uint8_t physical[14]={4,5,14,8,9,9,13,10,12,3,6,15,16,17};
    if(key<14) {
        if(on)button_leds|=1U<<key;else button_leds&=~(1U<<key);
        smk37_led(physical[key],(key==4||key==5)?(button_leds&0x30)!=0:on);
    }
}
void smk37_rgb_set(unsigned id,unsigned rgb)
{
    if(rgb_colors[id]!=rgb){rgb_colors[id]=rgb;rgb_dirty|=1U<<id;}
}
/* Busy-wait on the 24 MHz TIMER4 counter (started by smk37_scan_init). */
static void spi_settle(unsigned microseconds)
{
    uint32_t started=fm1_device_ticks();
    while((uint32_t)(fm1_device_ticks()-started)<24U*microseconds) {}
}
void smk37_rgb_touch(void) {rgb_dirty=0xffff;}
/* KamoteKeys 0.35: pad LED bit timing, selectable on the Diagnostics page
 * because SMK37 OPEN's fixed timing garbled frames on real hardware. The
 * SPI clock divider sets the bit period; zero/one set each bit's pulse. */
static const struct {uint8_t baud,zero,one;} rgb_timings[SMK37_RGB_TIMINGS]={
    {8,0xc0,0xf0},  /* 0 SMK37 OPEN original      */
    {8,0x80,0xf0},  /* 1 shorter 0-pulse          */
    {8,0x80,0xf8},  /* 2 short 0, long 1          */
    {9,0xc0,0xf0},  /* 3 slower clock             */
    {10,0x80,0xf0}, /* 4 slower clock, short 0    */
    {7,0xc0,0xf0},  /* 5 faster clock             */
    {6,0xc0,0xf8},  /* 6 faster clock, long 1     */
    {11,0x80,0xe0}};/* 7 slowest, short pulses    */
static uint8_t rgb_timing;
void smk37_rgb_timing(unsigned timing)
{
    if(timing<SMK37_RGB_TIMINGS && timing!=rgb_timing){rgb_timing=(uint8_t)timing;rgb_dirty=0xffff;}
}
volatile uint32_t smk37_rgb_frames;   /* pad frames sent (diagnostics) */
void smk37_rgb_service(void)
{
    /* KamoteKeys: keep >= 20 ms between frames (<= 50 fps); every frame starts
     * with a full-length reset (RGB_LEAD). */
    static uint32_t rgb_sent;
    if(!rgb_dirty || fm1_screen_pending())return;
    if((uint32_t)(fm1_millis()-rgb_sent)<20U)return;   /* <= 50 frames/s (0.36) */
    /* One SPI byte per LED bit, MSB first: the high part of the byte is the
     * LED's pulse. SMK37 OPEN used 0xC0/0xF0 at BAUD 8 (timing 0). */
    const unsigned zero=rgb_timings[rgb_timing].zero,one=rgb_timings[rgb_timing].one;
    for(unsigned i=0;i<16;++i) {
        if(!(rgb_dirty&(1U<<i)))continue;
        unsigned rgb=rgb_colors[i],grb=((rgb&0xff00)<<8)|((rgb>>8)&0xff00)|(rgb&255);
        uint8_t *bits=rgb_buffer+RGB_LEAD+24*rgb_map[i];
        for(unsigned b=0;b<24;++b)bits[b]=(uint8_t)((grb>>(23-b))&1U?one:zero);
    }
    /* One task owns SPI1: finish LCD, keep CS high during RGB DMA, restore.
     * KamoteKeys: the DMA "done" flag can precede the last bits leaving the
     * shift register. Let any LCD tail drain before routing SPI1 to the pad
     * LEDs, and let the last LED bits drain before routing it back, so no
     * stray pulses reach the LED chain (which garbles every pad). */
    fm1_lcd_deselect();spi_settle(20);
    /* With the LCD deselected, clock out one zero byte so the SPI data line
     * idles low before it is routed to the LED chain (no high glitch). */
    LCD_REGISTER(0)|=LCD_ACK;LCD_REGISTER(8)=0;fm1_lcd_wait();
    JL_IOMAP->CON1|=BIT(4);JL_SPI1->BAUD=rgb_timings[rgb_timing].baud;
    JL_SPI1->CON|=BIT(14);fm1_device_fence();
    JL_SPI1->ADR=(uint32_t)(uintptr_t)rgb_buffer;JL_SPI1->CNT=sizeof rgb_buffer;
    uint32_t errors=fm1_lcd_timeouts;fm1_lcd_wait();
    spi_settle(20);
    JL_IOMAP->CON1&=~BIT(4);JL_SPI1->BAUD=4;
    rgb_sent=fm1_millis();++smk37_rgb_frames;
    if(errors==fm1_lcd_timeouts)rgb_dirty=0;
}
