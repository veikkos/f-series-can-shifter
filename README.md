# BMW F-series gear lever (GWS) for sim racing

![Arduino Build](../../actions/workflows/build.yml/badge.svg)

Connects a real BMW F-series electronic gear selector (GWS) to a PC with an Arduino, so the physical lever shifts gears in driving games. The lever's gear indicator and backlight are driven back from the game's telemetry.

Spinoff of https://github.com/veikkos/e90-can-cluster.

![BMW F-series gear lever](./media/shifter.jpg)

## How it works

- Lever movements are translated into USB-HID gamepad button presses
- The game's current gear is sent back to the lever over serial for synchronization purposes

## Hardware

The lever sits on a 500 kbit/s CAN bus. Because the lever is controlled through a USB-HID gamepad, the board must be able to present one. Currently supported boards are Arduino Micro / Leonardo and Teensy 4.x and alike.

A CAN transceiver (e.g. SN65HVD230) is required on the bus pins for the Teensy. The MCP2515 module includes one.

## Libraries

- [mcp_can](https://github.com/coryjfowler/MCP_CAN_lib) (MCP2515 builds)
- [ArduinoJoystickLibrary](https://github.com/MHeironimus/ArduinoJoystickLibrary) (ATmega32u4 builds).

Teensy uses its built-in USB Joystick. Select a USB Type that includes a joystick under Tools > USB Type.

## Build

1. Pick the CAN adapter in [config.h](config.h)
2. Install the libraries above as needed
3. Install the sketch

## Telemetry from the PC

Shifter uses same binary protocol and proxy as the [cluster project](https://github.com/veikkos/e90-can-cluster/#custom-end-to-end-solution).

### Configuration mode

When no telemetry has arrived from the game for 5 seconds (proxy is not on or the game is not responding), the lever automatically goes into configuration mode. Use this to bind the lever's gamepad buttons in the game.

### BeamNG.drive

Bind the lever's gamepad buttons in BeamNG's controller settings as follows:

| Shifter         | BeamNG    |
| --------------- | --------- |
| Park            | 1st gear  |
| Drive           | 2nd gear  |
| Sport           | 3rd gear  |
| <s>Manual</s>   | <s>6th gear</s>  |
| Reverse         | Reverse   |
| Push in M/S     | Gear down |
| Pull in M/S     | Gear up   |

Manual should not be bound explicitly. BeamNG will automatically engage manual mode if gears are changed in Sport mode.

## Credits

- [TeksuSiK/bmw-gws-simhub](https://github.com/TeksuSiK/bmw-gws-simhub) for the original gear-lever logic
- [OpenInverter.org — BMW F-Series Gear Lever](https://openinverter.org/wiki/BMW_F-Series_Gear_Lever) for the CAN messages
