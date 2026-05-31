// ////////////////////////////////////////////////////////////////////////////////////////////////////
//      Tester Present Specialist Automotive Solutions - Programmable CAN Interface - CAN/Serial/USB //
//               Copyright (c) 2026 Jack Leighton. All rights reserved.                              //
// ////////////////////////////////////////////////////////////////////////////////////////////////////
// ....................................................................................................
// ....................................DEVELOPED IN AUSTRALIA..........................................
// ....................................................................................................
// ....................................................................................................
//                                           .  ....       .            ..
//                                          .:::. .:.... ...           .=+.
//                                           .::::::::::::::.         .=++-.
//                                          .:::::::::::::..          .++++:
//                                 .:-=.   .::::::::::::::.:          :++++-
//                               .=+++++-:..:::::::::::::.  .         :+++++-=.
//                              :+++++++++:::::::::::::::::...        :++++++++.
//                           ...-+++++++++::::::::::::::::::::.  .    +++++++++:
//                         .-=.+++++++++++-:::::::::::::::::::-++:  .=+++++++++=.
//                         -++++++++++++++-:::::::::::::::::::-+++++++++++++++++-
//                        .-++++++++++++++-:::::::::::::::::::=+++++++++++++++++-
//                       .=+++++++++++++++-:::::::::::::::::::=++++++++++++++++++..
//                     .:+++++++++++++++++=:::::::::::::::::::=++++++++++++++++++++=.
//                ..-+++++++++++++++++++++=:::::::::::::::::::+++++++++++++++++++++++=.
//            .:=+++++++++++++++++++++++++=:::::::::::::::::::++++++++++++++++++++++++-
//         ..:++++++++++++++++++++++++++++=:::::::::::::::::::+++++++++++++++++++++++++.
//        :.+++++++++++++++++++++++++++++++:::::::::::::::::::+++++++++++++++++++++++++++-.
//        -++++++++++++++++++++++++++++++++.::::JAKKA351::::::+++++++++++++++++++++++++++=
//        :++++++++++++++++++++++++++++++++.::::::::::::::::::++++++++++++++++++++++++++++=.
//        -++++++++++++++++++++++++++++++++.:-------------::::======++++++++++++++++++++++++.
//        .=+++++++++++++++++++++++++++++++-+++++++++++++++++++++++=+++++++++++++++++++++++++:
//        .+-++++++++++++++++++++++++++++++-+++++++++++++++++++++++=+++++++++++++++++++++++++-
//        .:+++++++++++++++++++++++++++++++-+++++++++++++++++++++++=+++++++++++++++++++++++++:
//          .++++++++++++++++++++++++++++++-+++++++++++++++++++++++----======++++++++==++=+==:
//           .+++++++++++++++++++++++++++++=+++++++++++++++++++++++-===============-=========-.
//             =++++++++++++++++++++++++++++=++++++++++++++++++++++-=========================.
//             .++++++++++++++++++++++++++++=++==++++++++++++++++++-========================..
//              :+++++++++++++++++++++++=:.       ..:-+++++++++++++-=======================-.
//               .+++++++++++++++++=:.               .-+++++==+++++=======================-.
//               .++++++++++++++++:                    .+++:.++++++-====================-:.
//               .=++++++++:......                     .==  =:+++++=+:=================:
//              .-++++++=.                                 ..:+++++=+++:==========--=-.
//                 ..:..                                  .... .=+=+++++=----==-===--:.
//                                                              -+=+++++++++++++-==-.
//                                                              .=-+++++++++++++=:-:.
//                                                                ..:=+++++++++:::.
//                                                                    ..  .:-.
//         TESTER PRESENT SPECIALIST AUTOMOTIVE SOLUTIONS              .:.     .
//                                                                     ....   ..
//                                                                      :=====-
//                                                                      .=====:
//                                                                      .-===-.
//                                                                        ::.
//
//
// ....................................................................................................
// ....................................................................................................
// ....................................................................................................
// ....................................................................................................
// ////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ----------------------------------------------------------------------------------------------------
//  config.h  -  Board pin map, CAN IDs, firmware identity, feature flags, default config blob.
//              Single place to retarget GPIO / IDs / defaults for the TP-CAN-2I (ESP32-S3-WROOM-1).
// ----------------------------------------------------------------------------------------------------

#ifndef TP_CONFIG_H
#define TP_CONFIG_H

#include <Arduino.h>

// ====================================================================================================
//  FIRMWARE IDENTITY  (returned by DID 0xF180 / 0xF181, and used in About strings)
// ====================================================================================================
#define TP_PROTOCOL_VERSION_STR   "TP-DPP 1.0"                  // DID 0xF180 - protocol version (ASCII)
#define TP_APP_VERSION_STR        "TP-CAN-2I 1.0.0 " __DATE__   // DID 0xF181 - app semver + build date
#define TP_DEVICE_MODEL_STR       "TP-CAN-2I"

// ====================================================================================================
//  FEATURE FLAGS
// ====================================================================================================
// DPP_SECURITY: when 0 (default bench build) SECURITY_ACCESS (0x27) is "open": seed returns 0 and any
//               key is accepted, so it never blocks development. Set to 1 for a real seed/key scheme.
#ifndef DPP_SECURITY
  #define DPP_SECURITY            0
#endif

// CANFD_ENABLE: when 0 (default) both channels run classic CAN 2.0B @ 500k. When 1 the data phase is
//               enabled at runtime per the config blob (can_fd_enable / can_fd_data_bitrate). The
//               MCP2518FD driver is FD-capable either way; this only gates the default bring-up.
#ifndef CANFD_ENABLE
  #define CANFD_ENABLE            0
#endif

// WIFI_ENABLE / BLE_ENABLE: lightweight wireless bring-up (STATUS / Wi-Fi LED). Kept off the hot path.
#ifndef WIFI_ENABLE
  #define WIFI_ENABLE             0   // 0 = do not auto-start STA/AP (saves power); LED still managed
#endif
#ifndef BLE_ENABLE
  #define BLE_ENABLE              0
#endif

// ====================================================================================================
//  ESP32-S3-WROOM-1-N16R8  GPIO PIN MAP
//  ----------------------------------------------------------------------------------------------------
//  Mirrors Hardware/Schematic_Design.md (Block 6/8) and Docs/Engineering_Specification.md (section 5).
//
//   Function            | GPIO | Dir | Notes
//   --------------------+------+-----+--------------------------------------------------------------
//   USB D-              |  19  |  -  | native USB CDC (fixed by silicon)            -- not a sketch pin
//   USB D+              |  20  |  -  | native USB CDC (fixed by silicon)            -- not a sketch pin
//   SPI2 (FSPI) SCK     |  12  | OUT | shared bus to BOTH MCP2518FD SCK
//   SPI2 (FSPI) MOSI    |  11  | OUT | shared bus to BOTH MCP2518FD SDI
//   SPI2 (FSPI) MISO    |  13  | IN  | shared bus from BOTH MCP2518FD SDO
//   CAN1 CS  (U7)       |  10  | OUT | MCP2518FD #1 nCS  (10k pull-up R6)
//   CAN2 CS  (U8)       |  14  | OUT | MCP2518FD #2 nCS  (10k pull-up R6)
//   CAN1 INT (U7)       |   4  | IN  | MCP2518FD #1 nINT (active low, open-drain)
//   CAN2 INT (U8)       |   5  | IN  | MCP2518FD #2 nINT (active low, open-drain)
//   UART1 TX            |  17  | OUT | 3V3 serial -> DT pin10 + service header J3
//   UART1 RX            |  18  | IN  | 3V3 serial <- DT pin11 + service header J3
//   IGN / WAKE sense    |  16  | IN  | via R2/R3 divider + TVS; RTC-capable (deep-sleep wake)
//   LED Wi-Fi (blue)    |  38  | OUT | -> R4 1k -> LED2 -> GND
//   LED CAN1 (amber)    |  39  | OUT | -> R4 1k -> LED3 -> GND
//   LED CAN2 (amber)    |  40  | OUT | -> R4 1k -> LED4 -> GND
//   LED STATUS (blue)   |  41  | OUT | -> R4 1k -> LED5 -> GND
//   LED PWR (green)     |  --  |  -  | HARD-WIRED 3V3 -> R4 1k -> LED1 -> GND (no GPIO)
//   BOOT (SW1)          |   0  | IN  | strapping pin; tactile to GND
//   RESET (SW2)         |  EN  |  -  | chip EN; tactile to GND (not a GPIO)
//
//   RESERVED - DO NOT USE: GPIO 26..37  (SPI flash + octal PSRAM on the N16R8 module).
//   AVOID AS OUTPUTS     : GPIO 0, 3, 45, 46 (boot strapping pins).
// ====================================================================================================

// ---- SPI bus (FSPI / "SPI2") -----------------------------------------------------------------------
#define PIN_SPI_SCK            12
#define PIN_SPI_MOSI          11
#define PIN_SPI_MISO          13

// ---- MCP2518FD chip-selects & interrupts -----------------------------------------------------------
#define PIN_CAN1_CS           10
#define PIN_CAN2_CS           14
#define PIN_CAN1_INT           4
#define PIN_CAN2_INT           5

// ---- UART1 (harness pins 10/11 + service header J3) ------------------------------------------------
#define PIN_UART1_TX          17
#define PIN_UART1_RX          18
#define UART1_BAUD            921600UL          // DPP §2.2: 921600 8N1

// ---- Ignition / wake sense -------------------------------------------------------------------------
#define PIN_IGN_SENSE         16

// ---- LEDs ------------------------------------------------------------------------------------------
#define PIN_LED_WIFI          38                // LED2 blue   (Wi-Fi / BT activity)
#define PIN_LED_CAN1          39                // LED3 amber  (CAN1 vehicle traffic)
#define PIN_LED_CAN2          40                // LED4 amber  (CAN2 ECU traffic)
#define PIN_LED_STATUS        41                // LED5 blue   (system / MITM heartbeat)
// LED1 PWR (green) is hard-wired to 3V3 - no GPIO.

// ---- Buttons (informational; EN/IO0 are handled by the bootloader) ---------------------------------
#define PIN_BOOT_BTN           0

// LED polarity: anode side driven by GPIO, cathode to GND through R4 -> active HIGH = ON.
#define LED_ON                HIGH
#define LED_OFF               LOW

// ====================================================================================================
//  SPI CLOCK for the MCP2518FD  (datasheet max 20 MHz; 10 MHz is safe across the shared bus + isol- )
//  (transceivers are downstream of the controller, so SPI speed is independent of CAN bus speed.)
// ====================================================================================================
#define MCP2518FD_SPI_HZ      10000000UL        // 10 MHz SPI SCK
#define MCP2518FD_OSC_HZ      40000000UL        // Y1 = 40 MHz CMOS oscillator on CLKIN (CAN1; CAN2 via CLKO)

// ====================================================================================================
//  CAN PROGRAMMING TRANSPORT (ISO-TP) IDENTIFIERS  -- DPP §2.1
//   PC transmits requests on 0x7FE  ->  device receives.
//   Device transmits responses on 0x7FA -> PC receives.
// ====================================================================================================
#define DPP_CAN_REQ_ID        0x7FEU            // PC -> device (request)
#define DPP_CAN_RESP_ID       0x7FAU            // device -> PC (response)
#define ISOTP_PAD_BYTE        0xAA              // FRAME_PAD fill (pad to 8 bytes)
#define ISOTP_MAX_PDU         4095U             // ISO-TP 15765-2 maximum PDU length

// ====================================================================================================
//  DEFAULT CAN BIT TIMING
// ====================================================================================================
#define CAN_DEFAULT_NOMINAL_BITRATE   500000UL   // 500 kbit/s classic arbitration / nominal phase
#define CAN_DEFAULT_DATA_BITRATE      2000000UL   // 2 Mbit/s FD data phase (only if FD enabled)

// ====================================================================================================
//  DPP TIMING (DPP §7)
// ====================================================================================================
#define DPP_P2_MS             50                 // normal request -> response budget
#define DPP_P2_STAR_MS        5000               // extended (erase/write/verify), signalled by NRC 0x78
#define DPP_S3_MS             2000               // tester-present keep-alive window

// ====================================================================================================
//  DEVICE CONFIG BLOB (DID 0x0100)  -- byte layout is fixed by DPP §8.  35 bytes total.
//   off  size  field
//   0    1     version (=1)
//   1    1     mitm_mode      0=passthru 1=filter 2=rewrite 3=block
//   2    4     can1_bitrate   (bit/s)                     [big-endian on the wire]
//   6    4     can2_bitrate
//   10   1     can_fd_enable  bit0=CAN1 FD, bit1=CAN2 FD
//   11   4     can_fd_data_bitrate
//   15   1     term1_enable   on-board 120R CAN1 (0/1)
//   16   1     term2_enable   on-board 120R CAN2 (0/1)
//   17   16    device_name    ASCII, null-padded
//   33   2     crc16          CRC-16/CCITT over bytes 0..32
// ====================================================================================================
#define CFG_BLOB_LEN          35
#define CFG_VERSION           1

#define CFG_OFF_VERSION        0
#define CFG_OFF_MITM_MODE      1
#define CFG_OFF_CAN1_BR        2
#define CFG_OFF_CAN2_BR        6
#define CFG_OFF_FD_ENABLE      10
#define CFG_OFF_FD_DATA_BR     11
#define CFG_OFF_TERM1          15
#define CFG_OFF_TERM2          16
#define CFG_OFF_NAME           17
#define CFG_NAME_LEN           16
#define CFG_OFF_CRC16          33

// MITM modes (config byte 1)
#define MITM_MODE_PASSTHRU     0
#define MITM_MODE_FILTER       1
#define MITM_MODE_REWRITE      2
#define MITM_MODE_BLOCK        3

// Programming-bus select (DID 0x0103)
#define PROG_BUS_CAN1          0
#define PROG_BUS_CAN2          1

// NVS (Preferences) namespace / keys used to persist the config blob and bus select.
#define NVS_NAMESPACE         "tpcan2i"
#define NVS_KEY_CONFIG        "cfgblob"
#define NVS_KEY_PROGBUS       "progbus"

// Default device name (null-padded into the 16-byte field).
#define CFG_DEFAULT_NAME      "TP-CAN-2I"

#endif // TP_CONFIG_H
