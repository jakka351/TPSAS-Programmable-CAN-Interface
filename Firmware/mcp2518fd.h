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
//  mcp2518fd.h  -  Compact self-contained MCP2518FD CAN-FD SPI controller driver (two instances).
//                  No external library required. Configures: reset, oscillator/PLL, RAM/FIFO, nominal
//                  500k bit timing (optional 2 Mbit data phase for FD), one RX filter/mask, TX + RX of
//                  a single frame, and INT-flag readout. Both U7 (CAN1) and U8 (CAN2) share one FSPI
//                  bus with separate CS + INT (see config.h).
// ----------------------------------------------------------------------------------------------------

#ifndef TP_MCP2518FD_H
#define TP_MCP2518FD_H

#include <Arduino.h>
#include <SPI.h>

// ---- A transport-neutral CAN frame (classic or FD) -------------------------------------------------
struct CanFrame {
  uint32_t id;          // 11-bit (or 29-bit if extended) identifier
  uint8_t  len;         // payload length in BYTES (0..8 classic, up to 64 FD)
  bool     extended;    // true = 29-bit extended ID
  bool     fd;          // true = CAN-FD frame
  bool     brs;         // true = FD bit-rate switch (data phase fast)
  uint8_t  data[64];    // payload
};

class MCP2518FD {
public:
  // csPin / intPin per controller; spi is the shared bus (begun once by the caller).
  MCP2518FD(SPIClass *spi, uint8_t csPin, uint8_t intPin);

  // Full bring-up: reset, configure clock/PLL, RAM, TX/RX FIFOs, bit timing, accept-all filter,
  // then place the controller into the requested operating mode.
  //   nominalBitrate : arbitration / classic bit-rate (e.g. 500000)
  //   dataBitrate    : FD data-phase bit-rate (used only when fdMode = true)
  //   fdMode         : true = CAN-FD enabled (BRS allowed); false = classic CAN 2.0B
  // Returns true on success (oscillator ready + mode reached).
  bool begin(uint32_t nominalBitrate, uint32_t dataBitrate, bool fdMode);

  // Soft reset over SPI (RESET instruction). Leaves the part in Configuration mode.
  void reset();

  // Transmit one frame via TX FIFO. Returns false if the FIFO is full / not ready.
  bool sendFrame(const CanFrame &f);

  // Pull one frame from the RX FIFO. Returns false if no frame is available.
  bool readFrame(CanFrame &f);

  // True if the RX FIFO holds at least one frame (checks FIFO status, not just INT pin).
  bool available();

  // True while the nINT line is asserted (active-low) for this controller.
  bool interruptAsserted();

  // Put the controller into a CAN operating mode. Returns true once the mode is reached.
  bool setMode(uint8_t mode);

  // Operating modes (OPMOD/REQOP field of C1CON).
  static const uint8_t MODE_NORMAL_FD     = 0x0;  // CAN-FD + classic
  static const uint8_t MODE_SLEEP         = 0x1;
  static const uint8_t MODE_INTERNAL_LOOP = 0x2;  // internal loopback (self-test)
  static const uint8_t MODE_LISTEN_ONLY   = 0x3;
  static const uint8_t MODE_CONFIG        = 0x4;
  static const uint8_t MODE_EXT_LOOP      = 0x5;  // external loopback
  static const uint8_t MODE_NORMAL_CLASSIC= 0x6;  // classic CAN 2.0B only
  static const uint8_t MODE_RESTRICTED    = 0x7;

  // Self-test helper: internal-loopback a single frame; returns true if it round-trips.
  bool loopbackSelfTest();

private:
  SPIClass *_spi;
  uint8_t   _cs;
  uint8_t   _int;
  bool      _fd;
  SPISettings _spiset;

  // ---- low-level SPI primitives (MCP2518FD instruction set) ----------------------------------------
  inline void csLow()  { digitalWrite(_cs, LOW); }
  inline void csHigh() { digitalWrite(_cs, HIGH); }

  uint8_t  readByte(uint16_t addr);
  void     writeByte(uint16_t addr, uint8_t val);
  uint32_t readWord(uint16_t addr);                 // 32-bit little-endian register read
  void     writeWord(uint16_t addr, uint32_t val);  // 32-bit little-endian register write
  void     readBuffer(uint16_t addr, uint8_t *buf, uint16_t len);
  void     writeBuffer(uint16_t addr, const uint8_t *buf, uint16_t len);

  bool     configureBitTiming(uint32_t nominalBitrate, uint32_t dataBitrate, bool fdMode);
  void     configureFifos();
  void     configureFilters();

  // DLC <-> byte-count helpers for FD frames.
  static uint8_t lenToDlc(uint8_t len);
  static uint8_t dlcToLen(uint8_t dlc);
};

#endif // TP_MCP2518FD_H
