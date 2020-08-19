/*
 * usbxbm - XBM to LCD by USB
 * Device Firmware
 * Main
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
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbconfig.h"
#include "usbdrv/usbdrv.h"
#include "display.h"

/** The device code's version */
#define VERSION "1.0"

/** Banner text so the host-side knows what this device is */
uint8_t banner[] = "RUDY usbxbm " VERSION;

/**
 * HELLO Request, host indicates an attempt to establish a new connection.
 * This needs to be send as first request and include the right magic number
 * wValue / wIndex pair as defined below to ensure that the host-side knows
 * what sort of device it's actually communicating with.
 * If the magic number pair matches, the device will respond with its banner
 * string (so the host can double check that as well and do some version
 * check if needed) and goes into ST_READY state.
 */
#define CMD_HELLO   0x55
/**
 * Host asks for the device's display properties, which are defined in the
 * display struct's properties field, defined by each display indididually.
 * The host uses this information to scale images specifically for the
 * display's resolution.
 */
#define CMD_PROPS   0x10
/**
 * Host sends a new image frame. This request includes a data transfer to
 * send the actual raw image data that is forwarded to the display then.
 */
#define CMD_DATA    0x20
/** Reset the display to its initial state */
#define CMD_RESET   0xf0
/** BYE Request, host gracefully disconnects */
#define CMD_BYE     0xaa

/** Idle state, device is waiting for new connections */
#define ST_IDLE     0
/** Ready state, device has a connection and is ready to receive image data */
#define ST_READY    1
/** The current state the device is in, ST_IDLE or ST_READY */
uint8_t state = ST_IDLE;

/** CMD_HELLO expected wValue parameter */
#define HELLO_VALUE 0x4d6f
/** CMD_HELLO expected wIndex parameter */
#define HELLO_INDEX 0x6921
/* ..adding up to ASCII of the Finnish greeting 'Moi!' */

/** Number of bytes to expect from the CMD_DATA request */
static uint16_t recv_len;
/** Bytes received during the CMD_DATA request's data transfer */
static uint16_t recv_cnt;


/**
 * V-USB setup callback function.
 *
 * Called from within V-USB when the host requests a setup transaction.
 * All the CMD_* requests defined above will be handled here.
 *
 * Check the RUDY project's V-USB examples for all the raunchy details
 * about this: https://github.com/sgreg/rudy/tree/master/firmware/v-usb
 *
 * @param data Setup data
 */
uchar
usbFunctionSetup(uchar data[8])
{
    /* Cast given raw data to usbRequest_t structure */
    usbRequest_t *rq = (void *) data;

    switch (rq->bRequest) {
        case CMD_HELLO:
            /*
             * HELLO Request - Host wants to establish a connection
             *
             * Make sure the wIndex values included in the request match the
             * expected magic numbers so we can ensure that the host actually
             * *wants* to talk to our device here and doesn't expect anything
             * else / host gone wild.
             */
            if (rq->wValue.word == HELLO_VALUE && rq->wIndex.word == HELLO_INDEX) {
                /*
                 * Values match.
                 *
                 * Check that the device is in ST_IDLE state, and if not,
                 * assume a previous connection has unexpectedly died without
                 * properly disconnecting. Just in case, re-initialize the
                 * display by calling its init() callback before proceeding.
                 *
                 */
                if (state != ST_IDLE) {
                    display.init();
                }

                /*
                 * All good now, set device into ST_READY state and send
                 * the banner string back to the host
                 */
                state = ST_READY;
                usbMsgPtr = banner;
                return sizeof(banner);
            }
            break;

        case CMD_PROPS:
            /*
             * PROPS Request - Host asks for display properties
             *
             * The device must be in ST_READY state to make this work, so
             * there must have been a proper CMD_HELLO exchange happening
             * prior to this request.
             */
            if (state == ST_READY) {
                /* If so, send the display properties back to the host */
                usbMsgPtr = (uint8_t *) &display.properties;
                return sizeof(display.properties);
            }
            break;

        case CMD_DATA:
            /*
             * DATA Request - Host sends a new frame of image data
             *
             * Also here, device must be in READY state..
             */
            if (state == ST_READY) {
                /*
                 * ..which it is.
                 *
                 * Reset receive counter and set the expected length to the
                 * wLength value received as part of the request.
                 */
                recv_cnt = 0;
                recv_len = rq->wLength.word;

                /*
                 * Call the display's frame_start() callback function which
                 * should set the display in a state that it's ready to
                 * receive a full frame of raw image data to display
                 */
                display.frame_start();

                /*
                 * Return special USB_NO_MSG value to indicate to V-USB that
                 * additional data is expected to be transferred here, which
                 * is then handled in the usbFunctionWrite() callback function
                 */
                return USB_NO_MSG;
            }
            break;

        case CMD_RESET:
            /*
             * RESET Request - Re-initialize the display
             *
             * Device must be again in ST_READY state for this.
             */
            if (state == ST_READY) {
                display.init();
            }
            break;

        case CMD_BYE:
            /*
             * BYE Request - Host is leaving is
             *
             * Go to ST_IDLE state, regardless of the current state.
             */
            state = ST_IDLE;
            break;
    }

    /* In any other case, including unknown requests, simply return 0 */
    return 0;
}

/**
 * V-USB write callback function
 *
 * Called when the *host writes data to the device*, so in other words,
 * us being the device here, called when data is received from the host.
 *
 * This is executed as part of the CMD_DATA request and receives raw image
 * data to send straight as-is to the display.
 *
 * Note that data isn't received all at once but in chunks of (max) 8 bytes.
 *
 * @param data Pointer where to received data sent from the host is stored
 * @param len Number of bytes received in this single chunk
 * @return 1 if the host should send more data, 0 of all data was received
 */
uchar
usbFunctionWrite(uchar *data, uchar len)
{
    uint8_t i;

    /*
     * Forward the received data as-is to the display via its send_byte()
     * callback function and keep track of the amount of received bytes
     */
    for (i = 0; recv_cnt < recv_len && i < len; i++, recv_cnt++) {
        display.send_byte(data[i]);
    }

    /*
     * If all expected number of bytes for this frame was received,
     * call the display's frame_done() callback.
     */
    if (recv_cnt == recv_len) {
        display.frame_done();
    }

    /* Notify V-USB if we expected more data to come or not */
    return (recv_cnt == recv_len);
}

/*
 * Get Going..
 */
int
main(void) {
    /* Initialize the display via its init() callback function */
    display.init();

    /* Force USB device re-enumeration */
    usbDeviceDisconnect();
    _delay_ms(300);
    usbDeviceConnect();

    /* Initialize USB and enable interrupts */
    usbInit();
    sei();

    while (1) {
        /* Poll USB forever */
        usbPoll();
    }

    return 0;
}

