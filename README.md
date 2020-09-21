# usbxbm - XBM to LCD by USB

usbxbm is a USB device / Python control script combo to transfer raw [XBM](https://en.wikipedia.org/wiki/X_BitMap) image data over USB to a display connected to an ATmega328. The Python script converts images or video frames into XBM data and sends them via USB to the matching device. The device then reads the data and dumps it straight to its connected display. usbxbm itself is based on (and a showcase for) [RUDY](https://github.com/sgreg/rudy), adjusted as a standalone project.

---

<p align="center"><b>ACHTUNG: THIS IS A SCRAPPED FEATURE BRANCH</b><br><br>Contains incomplete code for ST7789 based TFTs<br>Mostly kept around for personal reference / notes</center>

---

## Features

### Device

The ATmega328 USB device currently supports two displays:
- the good old Nokia 5110 LCD connected via SPI
- the ubiquitous SSD1306 128x64 OLED connected via I2C

The latter one is specifically meant for the 4-pin version (VCC, GND, SCL, SDA) of it, but combining the code of both displays could adjust it for SPI as well, which would (presumably) increase the frame rate significantly.

### Host-side script

The Python script supports several input sources to convert and send:
- any video input file supported by OpenCV, including animated GIFs
- a camera source supported by OpenCV
- a series of JPG images, for example a video split into individual frames
- simply one static image

In addition, some of these operation modes support features like
- looping the video, which is great for short animated GIFs
- adding delay between each frame to either slow down a low-frame-rate video, or create a slide show
- setting the threshold value when converting the source image to black and white pixels


## Build it

Note that all instructions from here on forward are mainly valid for Linux.

### Hardware

As mentioned, usbxbm is built using the [RUDY](https://github.com/sgreg/rudy) board, but since that's just a generic ATmega328 development board, using a regular ATmega328 microcontroller will do, too - just remember to add the V-USB wiring to it then. Other than that, you'll obviously need to connect a display to it.


#### Nokia 5110 LCD

| ATmega328 | LCD | Comment |
| ---: | :--- | --- |
| 14 `PB0` | 1 `#RST`| Display reset |
| 16 `PB2`| 2 `CE`| SPI chip select |
| 15 `PB1`| 3 `D/#C`| Data / Command mode |
| 17 `PB3`| 4 `SDIN`| SPI data in |
| 19 `PB5`| 5 `SCK`| SPI clock|
| 7 `VCC`| 6 `VCC`| Supply voltage |
| 8 / 22 `GND`| 7 `LIGHT`| Backlight |
| 8 / 22 `GND`| 8 `GND`| Ground |


#### SSD1306 OLED

| ATmega328 | OLED | Comment |
| ---: | :---: | --- |
| 7 `VCC` | 1 `VCC` | Supply voltage (3.3V) |
| 8 / 22 `GND` | 2 `GND` | Ground |
| 28 `SCL` | 3 `SCL` | I2C clock |
| 27 `SDA` | 4 `SDA` | I2C Data|


#### ST7789 LCD

| ATmega328 | LCD | Comment |
| ---: | :--- | --- |
| 8 / 22 `GND`| 1 `GND`| Ground |
| 7 `VCC`| 2 `VCC`| Supply voltage |
| 19 `PB5`| 3 `SCL`| SPI clock|
| 17 `PB3`| 4 `SDA`| SPI data in |
| 15 `PB1` | 5 `#RST`| Display reset |
| 14 `PB0`| 6 `D/#C`| Data / Command mode |
| -- | 7 `LIGHT`| Backlight |
| 16 `PB2`| 8 `CE`| SPI chip select (might not exist on some displays) |


(These should certainly be accompanied by a picture of this..)

### Firmware

Again, as this is using the RUDY board, the [general build information for RUDY](https://github.com/sgreg/rudy/tree/master/firmware) apply here. Summarized, you'll need the AVR GCC toolchain, AVRDUDE, and a programmer (by default USBasp is used).

If the toolchain is set up and all, you should be able to simply run `make` with the desired target to compile the firmware. While the device could support both displays at once, the build system is layed out to assume only one display will be used at the time. Therefore, the firmware will be built based on the display in use.

The firmware itself is inside the [`device/`](device/) directory, so enter it before you proceed.

#### Nokia 5110 LCD

```
$ make nokia5110
```

#### SSD1306 OLED

```
$ make ssd1306
```

#### ST7789 TFT

```
$ make st7789
```

## Flash it

Again, check the [RUDY documentation](https://github.com/sgreg/rudy/tree/master/firmware) for setting it all up. But essentially, you'll need a programmer, and if you're not using USBasp, adjust the `AVRDUDE_FLAGS` line in the `Makefile` for the one you are using.

Once the programmer is connected, just run `make` once more with the `program` target:
```
$ make program
```

That's it, if all went well and your display is properly hooked up, you should see the usbxbm splash screen on it.

If you connect the device via USB, your syslog should print something along the lines of this:
```
[663945.031111] usb 2-3.3: new low-speed USB device number 23 using xhci_hcd
[663945.126326] usb 2-3.3: New USB device found, idVendor=1209, idProduct=b00b, bcdDevice= 1.00
[663945.126332] usb 2-3.3: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[663945.126335] usb 2-3.3: Product: RUDY
[663945.126338] usb 2-3.3: Manufacturer: CrapLab
[663945.126341] usb 2-3.3: SerialNumber: usbxbm
```

Congrats, you're ready to go.

## Run it

As the firmware implements a custom USB device, you most likely have to set up a udev rule for it in order to have access to it as a regular user. Once again, [RUDY has the answer](https://github.com/sgreg/rudy/tree/master/firmware/v-usb#add-udev-rule). Alternatively, you can use `sudo`, but setting up a udev rule is still the recommended way.

With the device connected, the Python script in the [`host/`](host/) directory will do the rest.

### usbxbm.py quick guide

#### Video

To play a video and send it to the USB device, run:
```
$ ./usbxbm.py -v /path/to/video.xyz
```

#### Camera

To open up a webcam and stream it to the device:
```
$ ./usbxbm.py -c
```
or, in case you have more than one camera attached, change the video id by passing it along:
```
$ ./usbxbm.py -c 2
```

#### Static image

To simply display one static image:
```
$ ./usbxbm.py -i /path/to/image.jpg
```

#### Everything else

The full documentation of the usbxbm Python script can be found in [the `host` directory README](host/).

