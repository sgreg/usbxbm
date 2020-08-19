/*
 * usbxbm - XBM to LCD by USB
 * Device Firmware
 * Nokia 5110 LCD implementation
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

/** LCD X resolution, i.e. display width, in pixels */
#define X_RES 84
/** LCD Y resolution, i.e. display height, in pixels */
#define Y_RES 48

/** LCD reset pin data direction register */
#define LCD_RESET_DDR  DDRB
/** LCD reset pin port register */
#define LCD_RESET_PORT PORTB
/** LCD reset pin number */
#define LCD_RESET_PIN  PB0

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
#define SPI_DC_PIN   PB1

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


/**
 * Set up the ports for all required connections other than the usual SPI ones.
 */
static void
port_setup(void)
{
    /* Set up LCD specific pins as outputs */
    LCD_RESET_DDR |= (1 << LCD_RESET_PIN);
    SPI_CS_DDR |= (1 << SPI_CS_PIN);
    SPI_DC_DDR |= (1 << SPI_DC_PIN);
    /* Set up SPI data and clock as outputs */
    DDRB |= (1 << DDB3) | (1 << DDB5);
}

/**
 * Hardware reset the display.
 */
static void
nokia5110_reset(void)
{
    lcd_rst_low();
    _delay_ms(50);
    lcd_rst_high();
}

/**
 * Initialize SPI.
 *
 * SPI is set up as controller, Mode 0, F_CPU/4 clock speed, MSB first
 */
void
spi_init(void)
{
    /* Enable as controller */
    SPCR  = (1 << SPE) | (1 << MSTR);
    /* Mode 0 */
    SPCR |= (0 << CPOL) | (0 << CPHA);
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

/**
 * Initialize the LCD.
 *
 * Sends the splash screen content once the LCD itself is set up.
 */
void
nokia5110_init(void)
{
    uint16_t i;

    port_setup();
    spi_init();
    nokia5110_reset();

    lcd_spi_enable();
    lcd_command_mode();

    spi_send_byte(0x21); /* set H=1 (and display on / horizontal addressing) */
    spi_send_byte(0xc8); /* set Vop register (0b1001000) */
    spi_send_byte(0x06); /* set temperature coefficient (0b10) */
    spi_send_byte(0x12); /* set bias system (0b010) */
    spi_send_byte(0x20); /* set H=0 (and keep display / addressing as-is) */
    spi_send_byte(0x08); /* set display blank */
    spi_send_byte(0x0c); /* set display to normal mode */

    /* Send splash screen */
    spi_send_byte(0x80); /* set X address to 0 */
    spi_send_byte(0x40); /* set Y address to 0 */
    lcd_data_mode();

    for (i = 0; i < X_RES * (Y_RES / 8); i++) {
        spi_send_byte(pgm_read_byte(&(nokia_gfx_nokia_splash[i])));
    }

    lcd_spi_disable();
}

/**
 * Nokia 5110 LCD start frame part.
 */
void
nokia5110_frame_start(void)
{
    lcd_spi_enable();
    lcd_command_mode();
    spi_send_byte(0x80); /* set X address to 0 */
    spi_send_byte(0x40); /* set Y address to 0 */
    lcd_data_mode();
}

/**
 * Nokia 5110 LCD end frame part.
 */
void
nokia5110_frame_done(void)
{
    lcd_spi_disable();
}


/** Display struct for the Nokia 5110 LCD */
display_t display = {
    .init = nokia5110_init,
    .frame_start = nokia5110_frame_start,
    .send_byte = spi_send_byte,
    .frame_done = nokia5110_frame_done,
    .properties = {
        .res_x = X_RES,
        .res_y = Y_RES,
        .color_bits = 1,
        .identifier = "Nokia 5110"
    }
};

