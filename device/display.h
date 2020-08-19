/*
 * usbxbm - XBM to LCD by USB
 * Device Firmware
 * Display definition header
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
#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/** General struct to hold all information relevant to different displays */
typedef struct {
    /** Function pointer to initialize the display itself */
    void (*init)(void);
    /** Function pointer called whenever a new frame is received via USB */
    void (*frame_start)(void);
    /** Function pointer called for each single byte to send to the display */
    void (*send_byte)(uint8_t);
    /** Function pointed called after a frame was received */
    void (*frame_done)(void);
    /** Display information */
    struct {
        /* Display's X resolution i.e. display width in pixels */
        uint16_t res_x;
        /* Display's Y resolution i.e. display height in pixels */
        uint16_t res_y;
        /* Number of colors available to display */
        uint8_t color_bits;
        /* Identifier string to give a nice name to this display */
        char identifier[20];
    } properties;
} display_t;

/** Display-specific code part needs to define the struct */
extern display_t display;

#endif /* _DISPLAY_H_ */
