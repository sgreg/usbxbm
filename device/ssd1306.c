/*
 * usbxbm - XBM to LCD by USB
 * Device Firmware
 * SSD1306 OLED implementation (the 0.96 128x64 kind)
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
#include <util/twi.h>
#include <util/delay.h>
#include "display.h"
#include "ssd1306_gfx.h"

/** OLED X resolution, i.e. display width, in pixels */
#define X_RES 128
/** OLED Y resolution, i.e. display height, in pixels */
#define Y_RES  64

/** OLED TWI address (actual address, shifting happens internally) */
#define SSD1306_ADDR 0x3c

/** TWI clock speed in Hz */
#define F_SCL 400000UL

void ssd1306_init_send(void);


/**
 * Initialize TWI.
 *
 * Sets the baud rate to F_SCL defined above
 */
static void
twi_init(void)
{
    TWSR = 0;
    TWBR = ((F_CPU / F_SCL) - 16) / 2;
    TWCR = 0; /* turn off for now */
}

/**
 * Send TWI START condition.
 *
 * This also sends tha OLED's address and sets up communication for writing,
 * as that's all we do with the display here anyway.
 *
 * @return 0 on success, 1 in case of an error
 */
static uint8_t
twi_start(void)
{
    /* Send START condition */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {
        /* wait for end of transmission */
    }

    /* Make sure all went well */
    if ((TWSR & 0xf8) != TW_START) {
        /* TODO add some debug option to indicate this happened */
        return 1;
    }

    /* Send OLED address and don't add read flag since we're gonna write */
    TWDR = (SSD1306_ADDR << 1);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {
        /* wait for end of transmission */
    }

    /*
     * TODO this should check TWSR for TW_MT_SLA_ACK, but we don't have
     * any debug / error indication options here currently anyway
     */
	return 0;
}

/**
 * Send a single byte via TWI.
 *
 * This only sends the actual byte, so all setup (start condition,
 * address, mode, ...) needs to be set prior to calling this function!
 *
 * @param data Byte to send via TWI
 */
static void
twi_send_byte(uint8_t data)
{
    TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {
        /* wait for end of transmission */
    }
}

/**
 * Send TWI STOP condition.
 */
static void
twi_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}

/**
 * SSD1306 init sequence.
 *
 * Wholeheartedly stolen from https://github.com/Sylaina/oled-display
 * and mostly kept as-is
 */
static const uint8_t init_sequence[] PROGMEM =
{
    0xAE,            // Display OFF (sleep mode)
    0x20, 0b00,      // Set Memory Addressing Mode
    // 00=Horizontal Addressing Mode; 01=Vertical Addressing Mode;
    // 10=Page Addressing Mode (RESET); 11=Invalid
    0xB0,            // Set Page Start Address for Page Addressing Mode, 0-7
    0xC8,            // Set COM Output Scan Direction
    0x00,            // --set low column address
    0x10,            // --set high column address
    0x40,            // --set start line address
    0x81, 0x3F,      // Set contrast control register
    0xA1,            // Set Segment Re-map. A0=address mapped; A1=address 127 mapped.
    0xA7,            // Set display mode. A6=Normal; A7=Inverse
    0xA8, Y_RES-1,   // Set multiplex ratio(1 to 64)
    0xA4,            // Output RAM to Display
					 // 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
    0xD3, 0x00,      // Set display offset. 00 = no offset
    0xD5,            // --set display clock divide ratio/oscillator frequency
    0xF0,            // --set divide ratio
    0xD9, 0x22,      // Set pre-charge period
		     // Set com pins hardware configuration
    0xDA, 0x12,      // display width = 64
    0xDB,            // --set vcomh
    0x20,            // 0x20,0.77xVcc
    0x8D, 0x14,      // Set DC-DC enable
    0xAF,            // diaply on
};


/**
 * Initialize the SSD1306 OLED.
 *
 * Sends the init_sequence pattern along with the splash screen.
 */
void
ssd1306_init(void)
{
    uint16_t i;

    /* Initialize TWI itself */
    twi_init();

    /* Send the init_sequence */
    twi_start();
    twi_send_byte(0x00); /* command mode */
    for (i = 0; i < sizeof(init_sequence); i++) {
        //twi_send_byte(init_sequence[i]);
        twi_send_byte(pgm_read_byte(&init_sequence[i]));
    }
    twi_stop();

    /* Send the splash screen */
    ssd1306_init_send();
    for (i = 0; i < X_RES * (Y_RES / 8); i++) {
        twi_send_byte(pgm_read_byte(&ssd1306_gfx_ssd1306_splash[i]));
    }
    twi_stop();
}

/**
 * SSD1306 OLED start frame part.
 */
void
ssd1306_init_send(void)
{
    twi_start();
    twi_send_byte(0x00); /* Command mode */
    twi_send_byte(0xb0); /* Set Y position to 0 */
    twi_send_byte(0x21);
    twi_send_byte(0x00); /* Set X position to 0 */
    twi_send_byte(0x7f);
    twi_stop();

    twi_start();
    twi_send_byte(0x40); /* Data mode */
}


/** Display struct for the SSD1306 OLED */
display_t display = {
    .init = ssd1306_init,
    .frame_start = ssd1306_init_send,
    .send_byte = twi_send_byte,
    .frame_done = twi_stop,
    .properties = {
        .res_x = X_RES,
        .res_y = Y_RES,
        .color_bits = 1,
        .identifier = "SSD1306 OLED"
    }
};

