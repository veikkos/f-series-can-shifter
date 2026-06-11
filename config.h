#pragma once

// CAN adapter: pick exactly one option below.
// USE_MCP_CAN_SPI and USE_FLEXCAN_T4 are mutually exclusive.

// MCP2515 SPI adapter (default). Install "mcp_can" library.
// https://github.com/coryjfowler/MCP_CAN_lib
#define USE_MCP_CAN_SPI

#ifndef MCP_CAN_SPI_SPEED
    // Set to MCP_16MHZ if you have an adapter with 16MHz crystal
    #define MCP_CAN_SPI_SPEED MCP_8MHZ
#endif

#ifndef MCP_CAN_SPI_CS_PIN
    #define MCP_CAN_SPI_CS_PIN 10
#endif

// Teensy 4.x built-in FlexCAN controller. Uses the FlexCAN_T4 library
// bundled with Teensyduino. Requires an external CAN transceiver
// (e.g. SN65HVD230) on the bus pins.
//#define USE_FLEXCAN_T4

#ifndef FLEXCAN_T4_BUS
    // Pick CAN1 (TX=22, RX=23), CAN2 (TX=1, RX=0) or CAN3 (TX=31, RX=30)
    // on Teensy 4.1. Teensy 4.0 exposes CAN1 and CAN3 only.
    #define FLEXCAN_T4_BUS CAN1
#endif

#if !defined(USE_MCP_CAN_SPI) && !defined(USE_FLEXCAN_T4)
    #error "Select a CAN adapter in config.h (USE_MCP_CAN_SPI or USE_FLEXCAN_T4)"
#endif

// The lever drives the game through a USB-HID gamepad, so the board must be
// able to present one
#if defined(__AVR_ATmega32U4__)
    #define GWS_BOARD_AVR
#elif defined(ARDUINO_TEENSY40) || defined(ARDUINO_TEENSY41) || defined(__IMXRT1062__)
    #define GWS_BOARD_TEENSY
#else
    #error "Unsupported board: the gamepad requires an ATmega32u4 (Micro/Leonardo) or Teensy 4.x"
#endif

// Configuration mode: when true, the gear logic stays out of the way so the
// lever's gamepad buttons can be bound in the game. See README.
#ifndef CONFIGURATION_MODE
    #define CONFIGURATION_MODE false
#endif

// Serial (custom binary protocol) baud rate to the PC.
#ifndef PC_SERIAL_BAUD
    #define PC_SERIAL_BAUD 921600
#endif

// Highest forward gear the binary serial protocol may report (used when
// clamping an explicit manual gear).
#ifndef NUMBER_OF_GEARS
    #define NUMBER_OF_GEARS 7
#endif
