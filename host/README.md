# usbxbm Python Control Script

The host-side Python script here handles the device and does the video frame sending.

Its main concept is to open an image, video, or camera either via OpenCV or the Pillow library. Either way, the next step is to use Pillow to transform the image or single video frames that match the device's display: scale it down to its resolution and do some rotation, flipping, and matrix transformation to arrange the raw data just the way the display's memory likes it. Once the image / frame data is in that form, it's sent via USB to the device and dumped as-is there.

## Dependencies

To perform all this, the script requires a few dependencies:
- [NumPy](https://pypi.org/project/numpy/)
- [OpenCV](https://pypi.org/project/opencv-python/)
- [Pillow](https://pypi.org/project/Pillow/)
- [pyusb](https://pypi.org/project/pyusb/)

To make things easier, there's a `requirements.txt` so it can be easily installed via `pip`, either system-wide or within a `virtualenv` setup:

```
$ pip install -r requirements.txt
```

## Usage


```
$ ./usbxbm.py -h
usage: usbxbm.py [-h] (-c [ID] | -s PATH | -i PATH | -v PATH | -r)
                 [-t [0-255]] [-d SECONDS] [-l]

usbxbm host-side control application

optional arguments:
  -h, --help            show this help message and exit
  -c [ID], --camera [ID]
                        Open camera #ID in OpenCV and continuously capture and
                        send frames to USB device, default 0
  -s PATH, --imgseries PATH
                        Send series of images in PATH to USB device
  -i PATH, --image PATH
                        Send a single image located in PATH to USB device
  -v PATH, --video PATH
                        Send a whole video located in PATH to USB device
  -r, --reset           Simple resets the device to its initial state (i.e.
                        showing splash screen
  -t [0-255], --threshold [0-255]
                        Set color threshold value (0-255) that sets pixel on
                        or off, default 128
  -d SECONDS, --delay SECONDS
                        Add optional delay between frames, given in seconds as
                        float number, so 0.2 is 200ms
  -l, --loop            Loop a playback forever. Only relevant for --video and
                        --imgseries mode, ignored otherwise

Either one of --camera, --image, --imgseries, or --video must be given
$
```

`usbxbm.py` has a mandatory mode argument and a few optional option arguments.

### Modes

The Modes define the input source:

| CLI Parameter | Mode |
| --- | --- |
| `-c [ID], --camera [ID]` | Camera with optional `ID` as [video capturing device index](https://docs.opencv.org/4.4.0/d8/dfe/classcv_1_1VideoCapture.html#aabce0d83aa0da9af802455e8cf5fd181) (`0` by default) |
| `-s PATH, --imgseries PATH` | Slide show of all JPG files found inside a given `PATH` |
| `-i PATH, --image PATH` | Single image of given `PATH` |
| `-v PATH, --video PATH` | Video at given `PATH` |
| `-r, --reset` | Reset and re-initializes the display, showing splash screen again |

All modes are mutually exclusive, so only one can be defined.

### Options

Depending on the mode, a few additional options are available:

| CLI Parameter | `-c` | `-s` | `-i` | `-v` | Option |
| --- | :---: | :---: | :---: | :---: | --- |
| `-t [0-255], --threshold [0-255]` | X | X | X | X | Image threshold value<sup>[1]</sup> (`128` by default)|
| `-d SECONDS, --delay SECONDS` | X | X | | X |Delay between single frame transitions<sup>[2]</sup>|
| `-l, --loop` | | X | | X | Loop playback |

Unlike modes, any amount and combination of options can be defined. If an option makes no sense (e.g. looping or delaying a single image), it will simply be ignored.

<sup>[1]</sup> When transforming the source image to a black and white XBM image, the threshold defines the value (0-255) when a pixel is defined as on or off. The lower the value, the brighter the image becomes and vice versa. Different input sources / light situations might require a bit of tweaking to get the best results.

<sup>[2]</sup> Values are floating point values of full seconds, so `-d 2` will add a 2 seconds delay between frames, and `-d 0.01` would add a 10ms delay. Accuracy may depend on the underlying operating system.

## Examples

Loop a video with a threshold value of 100
```
$ ./usbxbm.py -v /path/to/video.xyz -t 100 -l
```

Display a webcam frame every 5 seconds at default threshold using default capture device 0
```
$ ./usbxbm.py -c -d 5
```

Loop a slide show at threshold value 60 with 10fps frame rate (i.e 100ms delay between frames)
```
$ ./usbxbm.py -s /path/to/image/directory/ -t 60 -d 0.1 -l
```

The same but with long options
```
$ ./usbxbm.py --imgseries /path/to/image/directory/ --threshold 60 --delay 0.1 --loop
```

..and feel free to mix long and short options as you please
```
$ ./usbxbm.py -s /path/to/image/directory/ -t 60 --delay 0.1 --loop
```
