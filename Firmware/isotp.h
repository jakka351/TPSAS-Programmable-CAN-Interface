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
//  isotp.h  -  Minimal ISO 15765-2 (ISO-TP) over one classic-CAN MCP2518FD channel.
//              Receives segmented requests addressed to 0x7FE (Single/First Frame + Flow Control +
//              Consecutive Frames) and transmits segmented responses on 0x7FA. The J2534 PassThru
//              adapter on the PC is the ISO-TP peer. Normal addressing, 8-byte frames, 0xAA padding.
// ----------------------------------------------------------------------------------------------------

#ifndef TP_ISOTP_H
#define TP_ISOTP_H

#include <Arduino.h>
#include "mcp2518fd.h"
#include "config.h"

class IsoTp {
public:
  IsoTp();

  // Bind to the CAN controller that currently carries the programming bus (CAN1 or CAN2) and set the
  // request/response 11-bit IDs. Called at boot and whenever DID 0x0103 switches the programming bus.
  void bind(MCP2518FD *can, uint32_t rxId, uint32_t txId);

  // Feed a frame that was already pulled off the bus by the MITM/poll loop. If it belongs to this
  // ISO-TP channel (id == rxId) it is consumed and drives the receive state machine; returns true if
  // the frame was consumed (so the caller does not also treat it as MITM traffic).
  bool onCanFrame(const CanFrame &f);

  // Poll the receive state machine (handles FC timing / completion). Returns true and fills out[]/len
  // when a COMPLETE request PDU has been reassembled. Non-blocking.
  bool poll(uint8_t *out, uint16_t &len, uint16_t maxLen);

  // Send a complete response PDU, segmenting into SF or FF+CF as needed and honouring the peer's
  // Flow Control (BS/STmin). Blocking but bounded by P2* via the supplied timeout. Returns false on
  // timeout / bus error.
  bool sendPdu(const uint8_t *pdu, uint16_t len, uint32_t timeoutMs);

  bool isBound() const { return _can != nullptr; }

private:
  MCP2518FD *_can;
  uint32_t   _rxId;
  uint32_t   _txId;

  // ---- receive reassembly state ----
  enum RxState { RX_IDLE, RX_RECEIVING, RX_COMPLETE };
  RxState  _rxState;
  uint8_t  _rxBuf[ISOTP_MAX_PDU];
  uint16_t _rxLen;        // expected total length
  uint16_t _rxGot;        // bytes received so far
  uint8_t  _rxNextSn;     // expected sequence number (0..15)
  uint32_t _rxLastMs;     // last activity (for N_Cr timeout)

  // ---- helpers ----
  bool sendRaw(const uint8_t *frame8, uint8_t dlc);    // one padded CAN frame on _txId
  void sendFlowControl(uint8_t fs, uint8_t bs, uint8_t stmin);
  bool waitFlowControl(uint8_t &fs, uint8_t &bs, uint8_t &stmin, uint32_t timeoutMs);
};

#endif // TP_ISOTP_H
