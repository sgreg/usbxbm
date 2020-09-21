#
# usbxbm - XBM to LCD by USB
# Device Firmware
# Makefile
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

MCU = atmega328p
F_CPU = 12000000

PROGRAM=usbxbm

OBJS = main.o
OBJS += usbdrv/usbdrv.o usbdrv/usbdrvasm.o

NOKIA_5110_OBJS = nokia5110.o nokia_gfx.o
SSD1306_OBJS = ssd1306.o ssd1306_gfx.o
ST7789_OBJS = st7789.o

ALL_OBJS = $(OBJS) $(NOKIA_5110_OBJS) $(SSD1306_OBJS)

CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
AVRDUDE = avrdude

CFLAGS += -g -Os -std=gnu99 -I. -I../common/\
-funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
-Wall -Wextra -Wstrict-prototypes \
-DF_CPU=$(F_CPU) -mmcu=$(MCU) 

ASFLAGS = -Wa,-adhlms=$(<:.c=.lst),-gstabs 
ASFLAGS_ASM = -Wa,-gstabs 

LDFLAGS = -Wl,-Map=$(<:.o=.map),--cref

AVRDUDE_FLAGS = -p $(MCU) -c usbasp


.PRECIOUS : %.elf %.o

all:
	@echo "Please specify the make target:"
	@echo ""
	@echo "  make nokia5110     Build $(PROGRAM) using Nokia 5110 LCD"
	@echo "  make ssd1306       Build $(PROGRAM) using SSD1306 OLED"
	@echo "  make st7789        Build $(PROGRAM) using ST7789 TFT"
	@echo "  make program       Flash $(PROGRAM) to the device"
	@echo "  make clean         Remove all intermediate build files (.o files)"
	@echo "  make distclean     Remove all build files (.o, .lst, .map, .elf, .hex)"
	@echo "  make burn-fuse     Set the device's fuses"
	@echo ""

$(PROGRAM).hex: $(PROGRAM).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@$(SIZE) $^

nokia5110: $(OBJS) $(NOKIA_5110_OBJS)
	$(CC) $(CFLAGS) $^ -o $(PROGRAM).elf $(LDFLAGS)
	@make -s $(PROGRAM).hex

ssd1306: $(OBJS) $(SSD1306_OBJS)
	$(CC) $(CFLAGS) $^ -o $(PROGRAM).elf $(LDFLAGS)
	@make -s $(PROGRAM).hex

st7789: $(OBJS) $(ST7789_OBJS)
	$(CC) $(CFLAGS) $^ -o $(PROGRAM).elf $(LDFLAGS)
	@make -s $(PROGRAM).hex

.c.o:
	$(CC) -c $(CFLAGS) $(ASFLAGS) $< -o $@

.S.o:
	$(CC) -c $(CFLAGS) -x assembler-with-cpp $(ASFLAGS_ASM) $< -o $@

burn-fuse:
	# External full swing crystal oscillator, 16K CK / 14 CK + 65ms
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:0xf7:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m

program:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(PROGRAM).hex

clean:
	rm -f $(ALL_OBJS)

distclean: clean
	rm -f $(ALL_OBJS:.o=.lst)
	rm -f $(ALL_OBJS:.o=.map)
	rm -f $(PROGRAM).elf
	rm -f $(PROGRAM).hex

.PHONY : all nokia5110 ssd1306 clean distclean program

