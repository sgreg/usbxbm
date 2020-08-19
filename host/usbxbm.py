#!/usr/bin/env python3
#
# usbxbm - XBM to LCD by USB
# Host-side Control Application
#
# Copyright (C) 2020 Sven Gregori <sven@craplab.fi>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
import os
import sys
import cv2
import glob
import time
import struct
import argparse
import usb.core
import numpy as np
from PIL import Image

# Expected USB device information
USB_VID = 0x1209
USB_PID = 0xb00b
USB_SERIAL_NUMBER = "usbxbm"
USB_DEVICE_STRING_PREFIX = "RUDY usbxbm "

# USB request types
USB_SEND = 0x40
USB_RECV = 0xC0

# USB request commands (make sure these are kept in sync with the device side firmware)
CMD_HELLO = 0x55
CMD_PROPS = 0x10
CMD_DATA  = 0x20
CMD_RESET = 0xf0
CMD_BYE   = 0xaa

# parameters for CMD_HELLO (it's just ASCII for the Finnish greeting "Moi!")
HELLO_VALUE = 0x4d6f
HELLO_INDEX = 0x6921


def parse_args():
    """
    Parse all command line parameters

    Returns:
    argparse.Namespace: object containing all parsed values
    """
    parser = argparse.ArgumentParser(
            description='usbxbm host-side control application',
            epilog='Either one of --camera, --image, --imgseries, or --video must be given')


    modes = parser.add_mutually_exclusive_group(required=True)

    modes.add_argument(
            '-c', '--camera',
            metavar='ID',
            nargs='?',
            type=int,
            const=0,
            help='Open camera #ID in OpenCV and continuously capture and send frames to USB device, default 0')

    modes.add_argument(
            '-s', '--imgseries',
            metavar='PATH',
            help='Send series of images in PATH to USB device')

    modes.add_argument(
            '-i', '--image',
            metavar='PATH',
            help='Send a single image located in PATH to USB device')

    modes.add_argument(
            '-v', '--video',
            metavar='PATH',
            help='Send a whole video located in PATH to USB device')

    modes.add_argument(
            '-r', '--reset',
            action='store_true',
            help='Simple resets the device to its initial state (i.e. showing splash screen')


    parser.add_argument(
            '-t', '--threshold',
            metavar='[0-255]',
            type=int,
            default=128,
            help='Set color threshold value (0-255) that sets pixel on or off, default 128')

    parser.add_argument(
            '-d', '--delay',
            metavar='SECONDS',
            type=float,
            default=0,
            help='Add optional delay between frames, given in seconds as float number, so 0.2 is 200ms')

    parser.add_argument(
            '-l', '--loop',
            action='store_true',
            help='Loop a playback forever. Only relevant for --video and --imgseries mode, ignored otherwise')

    return parser.parse_args()


def open_usb_device():
    """
    Look for USB device with RUDY VID/PID pair and open a connection to it.
    Once connection is established, the HELLO request is sent and verified that
    the device is actually the one we're expecting it to be.

    Returns:
    usb.core.Device: matching device if the expected device was found
    None: if no matching device was found
    """
    # Try to find a device that matches the expected vendor ID and product ID
    dev = usb.core.find(idVendor=USB_VID, idProduct=USB_PID)

    if dev is None:
        print('Error: No device found')
        return None

    if dev.serial_number != USB_SERIAL_NUMBER:
        print('Error: expected serial number "{}" but got "{}"'.format(USB_SERIAL_NUMBER, dev.serial_number))
        return None

    # Device found, sending HELLO request with expected magic numbers,
    # expecting to receive back the device banner identifying usbxbm
    print('<- [HELLO]')
    ret = dev.ctrl_transfer(USB_RECV, CMD_HELLO, HELLO_VALUE, HELLO_INDEX, 128)
    device_banner = ret.tobytes().decode('UTF-8')
    print('-> [HELLO] {}'.format(device_banner))

    if not device_banner.startswith(USB_DEVICE_STRING_PREFIX):
        print('Error: Device responded unexpected "{}"'.format(device_banner))
        return None

    # All good, return the device object
    return dev


def get_usb_device_properties(dev):
    """
    Request display properties from the USB device.

    Sends a CMD_PROPS request to receive display properties like its resolution
    and identifier, and return the values as dictionary.

    Parameters:
    dev (usb.core.Device): USB device object

    Returns:
    dict: Display properties as dictionary
    """
    # Send request and read back its response
    print('<- [PROPS]')
    properties = dev.ctrl_transfer(USB_RECV, CMD_PROPS, 0, 0, 128)

    # Unpack raw data into a struct to extract the individual property values
    properties_struct_string = "= H H B 20s"
    (res_x, res_y, color_bits, identifier) = struct.unpack(properties_struct_string, properties)
    print('-> [PROPS] {}: {}x{}@{}'.format(identifier.decode('UTF-8'), res_x, res_y, color_bits))

    # return the properties as dictionary
    return {'res_x': res_x, 'res_y': res_y, 'color_bits': color_bits, 'display': identifier}


def close_usb_device(dev):
    """
    Close USB connection by sending a CMD_BYE request.

    Note this doesn't actually *close* the connection, but as this is executed right
    before exiting the script, and all resources will be freed anyway, it seemed not
    necessary to do manually here. If that behavior shouls change at some point, use
    usb.util.dispose_resources(dev)

    Parameters:
    dev (usb.core.Device): USB device object
    """
    print('<- [BYE]')
    dev.ctrl_transfer(USB_SEND, CMD_BYE, 0, 0)


def send_image(image, data):
    """
    Send a given image to the connected usbxbm device.

    Parameters:
    image (PIL.Image.Image): Source image to convert and send via USB
    data (dict): Script-internal meta data
    """
    # Resize the given image to the display's resolution
    small = image.resize((data['res_x'], data['res_y']))

    # Rotate and flip the image to match the LCD/OLED arrangements
    rotflip = small.transpose(Image.ROTATE_270).transpose(Image.FLIP_LEFT_RIGHT)

    # Turn image into 8-bit black and white based on the threshold value given as command line parameter
    bw = rotflip.convert('L').point(lambda x: 0 if x > data['args'].threshold else 255, '1')

    # Turn that black-and-white image into XBM image string..
    xbm_string = bw.tobitmap().decode('ASCII')
    # ..and extract raw XBM values from the parts within curly braces (also removing line breaks)
    data_start = xbm_string.find('{') + 1
    data_end = xbm_string.find('}')
    raw_xbm = xbm_string[data_start:data_end].replace('\n', '')

    # Turn raw XBM data into an array of hex numbers..
    arr = [int(n, base=16) for n in raw_xbm.split(',')]
    # ..and create a numpy array from it
    nparr = np.asarray(arr)

    # Split the array based on the display width, turn it into a matrix, and transpose it
    matrix = np.asmatrix(np.split(nparr, data['res_x'])).transpose()

    # Finally, turn the matrix into a series of bytes..
    frame_data = matrix.astype('uint8').tobytes('C')
    # ..and send it as-is to the device
    data['dev'].ctrl_transfer(USB_SEND, CMD_DATA, 0, 0, frame_data)

    # If a --delay command line parameter was set, delay accordingly
    if data['args'].delay > 0:
        time.sleep(data['args'].delay)



def init_camera(args):
    """
    Initialization callback for camera mode.

    Creates OpenCV video capture object from the given args information (camera source id)
    that was passed from the command line parameters and returns it in a dictionary.

    Parameters:
    args (argparse.Namespace): Parsed command line argument object

    Returns:
    dict: Dictionary containing a cv2.VideoCapture object
    """
    cap = cv2.VideoCapture(args.camera)
    return {'cap': cap}


def init_video(args):
    """
    Initialization callback for video mode.

    Creates OpenCV video capture object from the given args information (video file)
    that was passed from the command line parameters and returns it in a dictionary.

    Parameters:
    args (argparse.Namespace): Parsed command line argument object

    Returns:
    dict: Dictionary containing a cv2.VideoCapture object
    """
    cap = cv2.VideoCapture(args.video)
    return {'cap': cap}


def process_video(data):
    """
    Frame-processing callback for video capture modes (camera and video)

    Retrieves frames from the video capture object referenced in the given meta data
    dictionary and sends them to the device. If retrieving a frame fails in video mode,
    it's assumed that the video itself was finished sending, and the loop either starts
    over (if the --loop command line parameter was given), or stops and returns.

    Parameters:
    data (dict): Script-internal meta data
    """
    while True:
        # Get the next frame from the video source
        ret, frame = data['cap'].read()

        if not ret:
            # No frame retrieved.
            # If source is a video file, end of file was presumably reached.
            # Check if --loop parameter was given and rewind back to the first frame
            if data['args'].video is not None and data['args'].loop:
                data['cap'].set(cv2.CAP_PROP_POS_FRAMES, 0)
                continue

            # Either not a video or no --loop option given, we're done then
            break

        # Create an image from the video frame and send it
        image = Image.fromarray(frame)
        send_image(image, data)


def process_single_image(data):
    """
    Frame-processing callback for single image mode.

    Opens the image file defined in the parsed command line parameters (stored within
    the given meta data dictionary) and sends it to the device.

    Parameters:
    data (dict): Script-internal meta data
    """
    image = Image.open(data['args'].image)
    send_image(image, data)


def process_image_series(data):
    """
    Frame-processing callback for image series mode.

    Loops through the directory defined in the parsed command line parameters (stored within
    the given meta data dictionary), opens each *.jpg file (currently only considers JPG files
    but could be adjusted if e.g. PNG is also needed) ands ends it to the device either until
    every single picture in the directory is sent, or (depending on another parameter) for all
    eternity (or until CTRL+C is hit).

    The files are processed in alphabetical order, and an image series itself could be easily
    generated with ffmpeg using printf-style formatting as output name:
        ffmpeg -i /path/to/video xxx_%05d.jpg
    or with some optional cropping or scaling:
        ffmpeg -i /path/to/video -vf crop:460:360,scale=128:64 xxx_%05d.jpg

    Note that unlike the video mode, the video frame rate is ignored in the image series mode
    and pre-scaling the image could slightly speed up the processing time, and by that the
    frame rate itself - although the OLED is mostly limited to the i2c clock speed and won't
    benefit much here. The SPI-connected Nokia LCD on the other hand could gain a frame or
    two by pre-scaling the image.

    Parameters:
    data (dict): Script-internal meta data
    """
    keep_going = True
    while keep_going:
        for infile in sorted(glob.glob(data['args'].imgseries + "/*.jpg")):
            image = Image.open(infile)
            send_image(image, data)

        if not data['args'].loop:
            keep_going = False


def process_reset(data):
    """
    Frame-processing callback for reset mode.

    Doesn't really process anything, just sends a CMD_RESET request to the device.

    Parameters:
    data (dict): Script-internal meta data
    """
    print('-> [RESET]')
    data['dev'].ctrl_transfer(USB_SEND, CMD_RESET, 0, 0)


def cleanup_video(data):
    """
    Cleanup callback for modes that handle OpenCV video (camera, video)

    Releases the video capture object referenced in the given data parameter

    Parameters:
    data (dict): Script-internal meta data
    """
    data['cap'].release()


def main():
    """
    Put it all together:
        - parse the command line parameters
        - set up the callback functions based on the mode given in those parameters
        - set up the USB connection
        - and get going by calling all corresponding callback functions
    """
    args = parse_args()

    # Some modes may have no need for an init and cleanup callback
    # (single image and image series), so they can be None and skipped
    mode_init = None
    mode_cleanup = None

    # Modes are mutually exclusive, so only one single of them should be ever set.
    # Set up mandatory frame-processing callback (mode_process) for all of them,
    # and the init / cleanup ones for those that need them (camera, video mode)
    if args.camera is not None:
        mode_init = init_camera
        mode_process = process_video
        mode_cleanup = cleanup_video

    elif args.image is not None:
        mode_process = process_single_image

    elif args.imgseries is not None:
        mode_process = process_image_series

    elif args.video is not None:
        mode_init = init_video
        mode_process = process_video
        mode_cleanup = cleanup_video

    elif args.reset:
        mode_process = process_reset

    else:
        # This shouldn't happen - unless a new mode was introduced and its
        # callback setup was forgotten to set up along the way here.
        print("Uh oh, don't know what to do..")
        sys.exit(1)


    # Set up the script-internal meta data dictionary.
    # This dictionary holds everything needed to handle the image processing
    # and USB sending: parsed command line parameters, USB device object,
    # display properties ..and anything that the init callback returns.
    #
    # If there is no init callback, mode_data gets simply initialized with
    # an empty dictionary (and filled with more data later on)
    if mode_init is not None:
        mode_data = mode_init(args)
    else:
        mode_data = {}

    # Try to connect to the USB device.
    # Quit if that fails, nothing we can do without it anyway.
    dev = open_usb_device()
    if dev is None:
        print("Failed to open USB device")
        sys.exit(1)

    # Retrieve the display properties from the USB device
    # and store it in the meta data dictionary.
    mode_data.update(get_usb_device_properties(dev))

    # Add all other useful data to the dictionary
    mode_data['dev'] = dev
    mode_data['args'] = args

    # Run the frame-processing callback, which may run inside any form of loop.
    # The loop can be interrupted with CTRL+C, which is caught here to provide
    # a graceful way to end it all.
    try:
        mode_process(mode_data)
    except KeyboardInterrupt:
        print("")

    # If we reached here, the frame-processing callback was either terminated
    # by natural causes (i.e. everything that was supposed to be send was sent),
    # or it was aborted with CTRL+C. Either way, we're done sending images to
    # the USB device, so call the cleanup callback (if there was one defined),
    # and close the USB device connection.
    if mode_cleanup is not None:
        mode_cleanup(mode_data)

    close_usb_device(dev)
    
    # The End.


if __name__ == "__main__":
    main()

