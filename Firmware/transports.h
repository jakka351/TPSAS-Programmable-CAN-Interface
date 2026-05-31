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
//  transports.h  -  The three DPP transports behind one uniform getPdu()/sendPdu():
//                     (1) USB-C native CDC   : Serial    (TP-framed)
//                     (2) UART1 service link : Serial1   (TP-framed, 921600 8N1)
//                     (3) CAN via ISO-TP     : IsoTp on the configured programming bus (0x7FE/0x7FA)
//                   Serial framing: MAGIC 'TP'(0x54 0x50) | LEN(2 BE) | PDU | CRC16-CCITT(2 BE),
//                   with magic-resync on CRC fail. dpp.cpp pumps these every loop.
// ----------------------------------------------------------------------------------------------------

#ifndef TP_TRANSPORTS_H
#define TP_TRANSPORTS_H

#include <Arduino.h>
#include "config.h"
#include "isotp.h"

// Which transport a given request arrived on (so the response goes back the same way).
enum TransportId {
  TR_NONE = 0,
  TR_USB,        // native USB CDC (Serial)
  TR_UART,       // UART1 (Serial1)
  TR_CAN         // ISO-TP on CAN1/CAN2
};

// Serial frame constants (DPP §2.2/2.3).
#define TP_MAGIC0   0x54   // 'T'
#define TP_MAGIC1   0x50   // 'P'

class Transports {
public:
  Transports();

  // Bring up USB CDC + UART1. (CAN/ISO-TP is bound separately via setIsoTp once the controllers exist.)
  void begin();

  // Provide the ISO-TP engine bound to the active programming bus.
  void setIsoTp(IsoTp *iso) { _iso = iso; }

  // Feed a CAN frame (already read off the bus by the poll/MITM loop) to the ISO-TP receiver.
  // Returns true if ISO-TP consumed it (i.e. it was addressed to 0x7FE).
  bool feedCanFrame(const CanFrame &f) { return _iso ? _iso->onCanFrame(f) : false; }

  // Non-blocking: if a complete request PDU is available on ANY transport, copy it into out[],
  // set len, record which transport it came from, and return true.
  bool getPdu(uint8_t *out, uint16_t &len, uint16_t maxLen, TransportId &src);

  // Send a response PDU back over the transport the request came from.
  bool sendPdu(const uint8_t *pdu, uint16_t len, TransportId dst);

private:
  IsoTp *_iso;

  // ---- per-serial-stream receive accumulators (USB + UART share the same logic) ----
  struct SerialRx {
    Stream  *stream;
    uint8_t  buf[8 + ISOTP_MAX_PDU];   // raw bytes scanned for a frame
    uint16_t count;
  };
  SerialRx _usb;
  SerialRx _uart;

  // Pull bytes from one serial stream and try to extract a framed PDU (with magic-resync).
  bool pumpSerial(SerialRx &rx, uint8_t *out, uint16_t &len, uint16_t maxLen);

  // Emit a TP-framed PDU on a serial stream.
  bool sendSerial(Stream *s, const uint8_t *pdu, uint16_t len);
};

#endif // TP_TRANSPORTS_H
