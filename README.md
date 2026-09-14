# RT4K-CEC

A Seeed Studio XIAO RP2350 project that bridges HDMI CEC remote-control input
to the RetroTINK 4K's USB serial control interface.

![Fully assembled Pico-CEC.](https://github.com/user-attachments/assets/7b971a8d-e5fd-4bc1-8ff5-a342004288a5)

## Update January 2026

The reference hardware is the Seeed Studio XIAO RP2350.

## What Works
* HDMI CEC frame send and receive
* EDID parsing to determine HDMI physical address
* TinyUSB USB host mode on the XIAO RP2350
* FTDI CDC host support for the RetroTINK 4K
* RT4K commands at 2,000,000 baud, 8-N-1, newline terminated
* UART diagnostics on XIAO D6/TX0 and D7/RX0 at 115200 baud

Hardware validation against a physical RetroTINK 4K and TV/AVR remote is still
pending.
 
> [!CAUTION]
> The build quality of the HDMI breakout boards is highly variable, thus pass through of 4K video may not function in all circumstances.

## Cloning
Clone the project and its pinned dependencies recursively:
```
git clone --recurse-submodules
```

## Building
This project uses the normal CMake build with Ninja and the Arm GNU embedded
toolchain. Homebrew manages the required native build tools from the included
`Brewfile`:
```
$ brew bundle --file=Brewfile
```

The project-local mise configuration provides repeatable setup and build task
shortcuts. It does not manage tool versions:
```
$ mise run setup
```

Build the Seeed XIAO RP2350 firmware:
```
$ mise run build
```

Reconfiguring from scratch requires a clean build directory (`rm -rf build`).

### Customising the Build
* CEC_PIN: specify GPIO pin for HDMI CEC, defaults to GPIO3
* CEC_OSD_NAME: specify the OSD string for HDMI input Pico-CEC is controlling, defaults to "Pico-CEC"
Example invocation to specify:
* use GPIO pin 11
* use OSD_NAME "Bazzite"

```
$ cmake -S . -B build -G Ninja -DCEC_PIN=11 -DCEC_OSD_NAME="Bazzite"
$ cmake --build build
```

## Installing
Assuming a successful build, the build directory will contain `pico-cec.uf2`,
this can be written to the Pico as per normal:
* connect the XIAO to a computer via USB cable
* reset the Pico by holding 'Boot' and pressing 'Reset'
   * Pico now presents as a USB mass storage device
* copy `pico-cec.uf2` to the Pico
* disconnect

## Blinking Lights
The RGB LED provides basic functional diagnosis:
* blue 2Hz: idle, CEC standby
* green 2Hz: CEC active
* green flash: CEC user button pressed
* red: crash

If there are no lights, something is very wrong.
If this occurs, please consider raising an issue.

# CEC validation history

The underlying CEC engine has been exercised with Sharp and Sony TVs and
through a Denon AVR. RT4K USB serial and remote-control behavior still needs
physical hardware validation.

# Design
## Hardware
The hardware connections are extremely simple. Both HDMI CEC and the Pico are
3.3V obviating the need for level shifters. The DDC bus (for EDID) is I2C and
5V, however, the Pico appears to be 5V tolerant.

Additionally, we rely on the GPIO input/output impedance states to read or drive
the CEC bus. DDC is I2C requiring data and clock lines.
Thus, we need to directly connect four wires:
* HDMI CEC pin 13 direct to a GPIO
* HDMI CEC ground pin 17 direct to GND
* HDMI DDC clock pin 15 direct to SCL
* HDMI DDC data pin 16 direct to SDA
* Optional if safe:
   * HDMI +5V power pin direct to 5V

For the Seeed Studio XIAO RP2350:
* HDMI pin 13 --> D10
* HDMI pin 17 --> GND
* HDMI pin 15 --> D5
* HDMI pin 16 --> D4
* HDMI pin 18 --> 5V/VUSB

### Schematic
![Basic schematic.](https://github.com/user-attachments/assets/61a759ca-198a-4f6b-a60f-0255d08b8441)

For the RT4K connection, use a USB-C splitter: feed the RT4K's existing power
source into the power input and connect the XIAO's USB host port to the RT4K
USB-C data input. The XIAO can be powered from HDMI pin 18 while its USB-C
connector is used for host data.

For diagnostics, connect a 3.3 V TTL UART adapter only after the firmware is
ready to test:
* XIAO D6/TX0 (GPIO0) --> adapter RX
* XIAO D7/RX0 (GPIO1) --> adapter TX
* XIAO GND --> adapter GND

Do not connect the adapter's VCC pin.

### Prototype

![Initial prototype.](https://github.com/user-attachments/assets/88f2631f-e33f-4994-91dc-cc9e3c07016a)

### Enclosure
The enclosure is a reasonably simple three piece sandwich 3d print modelled with OpenSCAD. It is designed to be printed as three separate pieces which are bolted together with M3 nuts and bolts.
An exploded preview of the result can be found in this [STL](openscad/pico-cec.stl).


### Assembly
![XIAO board with HDMI pass through and DDC.](https://github.com/user-attachments/assets/01c244b4-b5af-4926-94d2-38306876485b)

![Partially assembled Pico-CEC.](https://github.com/user-attachments/assets/c37bb127-409a-4ed1-acc1-4e83cf8a6d58)

## Software
The software is built on FreeRTOS tasks:
* cec_task
   * interact with HDMI CEC and send mapped user-control values to a queue
* rt4k_serial_task
   * translate queued controls into RT4K serial commands
* rt4k_serial_host_task
   * run the TinyUSB host stack and FTDI CDC transport
* debug/log task
   * send CEC and USB diagnostics to the dedicated TTL UART
* blink_task
   * heart beat, no blink == no work

## cec_task
The CEC task comprises three major components:
* `cec_frame_recv`
   * receives and validates CEC packets from the CEC GPIO pin
   * edge interrupt driven state machine
      * rewritten from busy wait loop to reduce CPU load
* `cec_frame_send`
   * formats and sends CEC packets on the CEC GPIO pin
   * alarm interrupt driven state machine
      * rewritten from busy wait loop to reduce CPU load
* main control loop
   * manages CEC send and receive

All the HDMI frame handling was rewritten to be hardware/timer interrupt driven
to meet real-time constraints.
Attempts to increase the FreeRTOS tick timer along with busy wait loops were
simply unable to consistently meet the CEC timing windows.

## Dependencies
This project uses:
* [crc](https://github.com/gityf/crc)
* [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)
* [pico-sdk](https://github.com/raspberrypi/pico-sdk)
   * [tinyusb](https://github.com/hathach/tinyusb)

# Hardware
* Seeed Studio XIAO RP2350 (chosen for form factor)
   * https://www.seeedstudio.com/Seeed-XIAO-RP2350-p-5944.html
   * Originally prototyped on the Raspberry Pi Pico board (still works but
     requires RGB unhacking)
* HDMI male/female passthrough adapter
   * Listed as 'HDMI Male and Female Test Board MINI Connector with Board PCB
     2.54mm pitch 19/20pin DP HD A Female To Male Adapter Board'
   * Model number: WP-905
   * https://www.aliexpress.com/item/1005004791079117.html
* custom 3d printed housing

# Bill of Materials
| Component | Quantity | Price (January 2026) (AUD) |
| :--- | ---: | ---: |
| Seeed Studio XIAO RP2350 | 1 | 10.60 |
| HDMI male/female adapter | 1 | 4.30 |
| M3x10mm bolt & nut | 2 | 0.16 |
| M3x20mm bolt & nut | 2 | 0.17 |
| Random short wires | 4 | basically free |
| Scunge 3D print from friend | 1 | mostly free |
| Umpteen hours of engineering | 1 | priceless |
| Total | | 15.23 |

# cec-compliance
As of v0.2.2, `pico-cec` now passes the cec-compliance test suite found in the
Linux `v4l-utils` package.
More details can be found in the wiki entry:
https://github.com/gkoh/pico-cec/wiki/CEC-Compliance-Testing

Furthermore, `pico-cec` has been able to survive one hour of cec-compliance fuzz
testing.

# Debugging

Diagnostics are emitted on the dedicated 3.3 V TTL UART described above. The
USB-C port is reserved for the RT4K host connection, so it is not a firmware
CLI connection.

# Future
* implement CEC send and receive in PIO
* port to ESP32?
   * WS2812 driver will need platform support, perhaps to RMT
   * implement CEC in RMT

# References
Inspiration and/or ground work was obtained from the following:
* https://github.com/SzymonSlupik/CEC-Tiny-Pro
* https://github.com/tsowell/avr-hdmi-cec-volume
