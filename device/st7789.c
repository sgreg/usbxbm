/*
 * usbxbm - XBM to LCD by USB
 * Device Firmware
 * ST7789 240x240px IPS (1.3" / 1.54")
 *
 * Copyright (C) 2020 Sven Gregori <sven@craplab.fi>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>
#include "display.h"
#include "nokia_gfx.h"


/*
 * Most of the ST7789 parts are based on Adafruit's Arduino library
 * https://github.com/adafruit/Adafruit-ST7735-Library
 */

/*
 * NOTE
 *
 * There seem to be two ways to make the display actually work with the /CS line handling:
 *
 * 1. pull it low before every transfer, and pull it back high after every transfer
 * 2. use SPI mode 3 (CPOL = 1, CPHA = 1) instead of usual mode 0 (CPOL = 0, CPHA = 0)
 *
 * If the CS pin isn't exposed (i.e. pulled internally to GND), option 2 will save you,
 * and if there's no other SPI device connected (and therefore there's no real need to
 * release CS pin between operations), option 2 will also avoid additional pin pulling
 * operations the whole time.
 */


/** LCD X resolution, i.e. display width, in pixels */
#define X_RES 240
/** LCD Y resolution, i.e. display height, in pixels */
#define Y_RES 240


// DC  -  8  PB0  (this is reset in nokia lcd)
// RST -  9  PB1  (this is dc in nokia lcd) ..those two could be swapped, but let's see (maybe swap nokia pins)
// CS  - 10  PB2

/** LCD reset pin data direction register */
#define LCD_RESET_DDR  DDRB
/** LCD reset pin port register */
#define LCD_RESET_PORT PORTB
/** LCD reset pin number */
#define LCD_RESET_PIN  PB1

/* SPI chip select pin data direction register */
#define SPI_CS_DDR  DDRB
/** SPI chip select pin port register */
#define SPI_CS_PORT PORTB
/** SPI chip select pin number */
#define SPI_CS_PIN  PB2

/** SPI data/command pin data direction register */
#define SPI_DC_DDR   DDRB
/** SPI data/command pin port register */
#define SPI_DC_PORT PORTB
/** SPI data/command pin number */
#define SPI_DC_PIN   PB0

/** Set LCD reset pin high */
#define lcd_rst_high()  do { LCD_RESET_PORT |=  (1 << LCD_RESET_PIN); } while (0)
/** Set LCD reset pin low */
#define lcd_rst_low()   do { LCD_RESET_PORT &= ~(1 << LCD_RESET_PIN); } while (0)
/** Set SPI chip select pin high */
#define spi_cs_high()   do { SPI_CS_PORT |=  (1 << SPI_CS_PIN); } while (0)
/** Set SPI chip select pin low */
#define spi_cs_low()    do { SPI_CS_PORT &= ~(1 << SPI_CS_PIN); } while (0)
/** Set SPI data/command pin high */
#define spi_dc_high()   do { SPI_DC_PORT |=  (1 << SPI_DC_PIN); } while (0)
/** Set SPI data/command pin low */
#define spi_dc_low()    do { SPI_DC_PORT &= ~(1 << SPI_DC_PIN); } while (0)

/* Define some aliases to make it more clear what's happening */
#define lcd_spi_enable      spi_cs_low
#define lcd_spi_disable     spi_cs_high
#define lcd_command_mode    spi_dc_low
#define lcd_data_mode       spi_dc_high

static void st7789_frame_start(void);

/**
 * Set up the ports for all required connections other than the usual SPI ones.
 */
static void
port_setup(void)
{
    /* Set up SPI data and clock as outputs */
    DDRB = (1 << DDB3) | (1 << DDB5);

    /* Set up LCD specific pins as outputs */
    LCD_RESET_DDR |= (1 << LCD_RESET_PIN);
    SPI_CS_DDR |= (1 << SPI_CS_PIN);
    SPI_DC_DDR |= (1 << SPI_DC_PIN);

    LCD_RESET_PORT |= (1 << LCD_RESET_PIN);
    SPI_CS_PORT |= (1 << SPI_CS_PIN);
    SPI_DC_PORT |= (1 << SPI_DC_PIN);
}

/**
 * Hardware reset the display.
 */
static void
st7789_reset(void)
{
    //lcd_rst_high(); // FIXME this needed?
    //_delay_ms(100); // FIXME this needed?
    lcd_rst_low();
    //_delay_ms(100);
    _delay_ms(10);
    lcd_rst_high();
    //_delay_ms(200); // FIXME this needed?
}

/**
 * Initialize SPI.
 *
 * SPI is set up as controller, Mode 0, F_CPU/4 clock speed, MSB first
 */
static void
spi_init(void)
{
    /* Enable as controller */
    SPCR  = (1 << SPE) | (1 << MSTR);
    /* Mode 0 */
    SPCR |= (1 << CPOL) | (1 << CPHA);
    /* Clock F_CPU/4 */
    SPCR |= (0 << SPR0) | (0 << SPR1);
    /* Data direction MSB first */
    SPCR |= (0 << DORD);

    SPSR |= (1 << SPI2X);
}

/**
 * Send a single byte via SPI.
 *
 * @param data Byte to send
 */
static void
spi_send_byte(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF))) {
        /* wait */
    }
}

#define ST77XX_SWRESET 0x01
#define ST77XX_SLPOUT 0x11
#define ST77XX_COLMOD 0x3A
#define ST77XX_MADCTL 0x36
#define ST77XX_CASET 0x2A
#define ST77XX_RASET 0x2B
#define ST77XX_INVON 0x21
#define ST77XX_NORON 0x13
#define ST77XX_DISPON 0x29
#define ST77XX_RAMWR 0x2C

/**
 * Initialize the LCD.
 *
 * Sends the splash screen content once the LCD itself is set up.
 */
void
st7789_init(void)
{
    uint8_t row, col;

    port_setup();
    spi_init();
    st7789_reset();

    lcd_spi_enable();

    lcd_command_mode();
    spi_send_byte(ST77XX_SWRESET);
    //lcd_data_mode();
    //lcd_spi_disable();
    _delay_ms(150);

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_SLPOUT);
    //lcd_data_mode();
    //lcd_spi_disable();
    _delay_ms(10);

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_COLMOD);
    lcd_data_mode();
    spi_send_byte(0x55); // 16 bit color
    //lcd_spi_disable();
    _delay_ms(10);

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_MADCTL); // MADCTL
    lcd_data_mode();
    spi_send_byte(0x08);
    //lcd_spi_disable();

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_CASET); // CASET
    lcd_data_mode();
    spi_send_byte(0x00);
    spi_send_byte(0x00);
    spi_send_byte(0x00);
    spi_send_byte(240);
    //lcd_spi_disable();

    // write row address (with 80 offset)
    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_RASET); // RASET
    lcd_data_mode();
    spi_send_byte(0x00);
    spi_send_byte(0); // ?
    spi_send_byte(320 >> 8);
    spi_send_byte(320 & 0xff);
    //lcd_spi_disable();

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_INVON); 
    //lcd_data_mode();
    //lcd_spi_disable();
    _delay_ms(10);

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_NORON);
    //lcd_data_mode();
    //lcd_spi_disable();
    _delay_ms(10);

    //lcd_spi_enable();
    //lcd_command_mode();
    //spi_send_byte(ST77XX_DISPON);
    //lcd_data_mode();
    //lcd_spi_disable();
    //_delay_ms(10);

    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_MADCTL);
    lcd_data_mode();
    spi_send_byte(0xc0); // rotation arrangements (MX | MY | RGB)
    //lcd_spi_disable();


    /* Send splash screen */
    st7789_frame_start();

    for (row = 0; row < Y_RES; row++) {
        for (col = 0; col < X_RES; col++) {
            // send hi, then lo (Adafruit_SPITFT.cpp line 1307)

            //spi_send_byte(0x55);
            //spi_send_byte(0xaa);
            spi_send_byte(0);
            spi_send_byte(col);
        }
    }
    lcd_command_mode();
    spi_send_byte(ST77XX_DISPON);

    lcd_spi_disable();
}

/**
 * ST7789 LCD start frame part.
 */
static void
st7789_frame_start(void)
{
    lcd_spi_enable();

    // write column address
    lcd_command_mode();
    spi_send_byte(ST77XX_CASET);
    lcd_data_mode();
    spi_send_byte(0x00); // start 15..08
    spi_send_byte(0x00); // start 07..00
    spi_send_byte(0x00); // end 15..08
    spi_send_byte(239); // end 07..00
    //lcd_spi_disable();

    // write row address (with 80 offset)
    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_RASET);
    lcd_data_mode();
    spi_send_byte(0x00); // start 15..08
    spi_send_byte(80); // start 07..00 (80)
    spi_send_byte(319 >> 8); // end 15..08
    spi_send_byte(319 & 0xff); // end 07..00
    //spi_send_byte(0); // start 07..00 (80)
    //spi_send_byte(239); // start 07..00 (80)
    //lcd_spi_disable();

    // initiate Memory Write
    //lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(ST77XX_RAMWR);
    lcd_data_mode();
}


static void
st7789_send_byte(uint8_t data)
{
    uint8_t i;
    uint8_t byte;

    for (i = 0; i < 8; i++) {
        byte = (data >> i) & 0x01;
        if (byte) {
            spi_send_byte(0xff);
            spi_send_byte(0xff);
        } else {
            spi_send_byte(0x00);
            spi_send_byte(0x00);
        }
    }
}


/**
 * ST7789 LCD end frame part.
 */
static void
st7789_frame_done(void)
{
    lcd_spi_disable();
}


/** Display struct for the ST7789 LCD */
display_t display = {
    .init = st7789_init,
    .frame_start = st7789_frame_start,
    .send_byte = st7789_send_byte,
    .frame_done = st7789_frame_done,
    .properties = {
        .res_x = X_RES,
        .res_y = Y_RES,
        .color_bits = 1, // FIXME
        .identifier = "ST7789 IPS"
    }
};

