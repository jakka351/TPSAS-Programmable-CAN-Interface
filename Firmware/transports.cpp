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
//  transports.cpp  -  USB-CDC + UART1 TP-framing (with magic-resync) and the CAN/ISO-TP glue.
// ----------------------------------------------------------------------------------------------------

#include "transports.h"
#include "dpp.h"   // crc16_ccitt()

Transports::Transports() : _iso(nullptr) {
  _usb.stream  = nullptr;  _usb.count  = 0;
  _uart.stream = nullptr;  _uart.count = 0;
}

void Transports::begin() {
  // USB native CDC. (Serial is the USB-CDC device when "USB CDC On Boot" is enabled.)
  Serial.begin(115200);                 // baud ignored by CDC, but harmless
  _usb.stream = &Serial;

  // UART1 service link on the harness pins / J3.
  Serial1.begin(UART1_BAUD, SERIAL_8N1, PIN_UART1_RX, PIN_UART1_TX);
  _uart.stream = &Serial1;
}

// ====================================================================================================
//  Serial transmit: MAGIC | LEN(2 BE) | PDU | CRC16(2 BE) over MAGIC+LEN+PDU.
// ====================================================================================================
bool Transports::sendSerial(Stream *s, const uint8_t *pdu, uint16_t len) {
  if (!s) return false;
  uint8_t hdr[4];
  hdr[0] = TP_MAGIC0;
  hdr[1] = TP_MAGIC1;
  hdr[2] = (len >> 8) & 0xFF;
  hdr[3] = len & 0xFF;

  // CRC over MAGIC + LEN + PDU.
  uint16_t crc = crc16_ccitt(hdr, 4, 0xFFFF);
  crc = crc16_ccitt(pdu, len, crc);

  uint8_t tail[2];
  tail[0] = (crc >> 8) & 0xFF;
  tail[1] = crc & 0xFF;

  s->write(hdr, 4);
  if (len) s->write(pdu, len);
  s->write(tail, 2);
  return true;
}

// ====================================================================================================
//  Serial receive with self-correcting framing.
//  Strategy: append all available bytes into rx.buf, then scan for a complete, CRC-valid frame at the
//  front. On CRC mismatch (or junk) slide forward one byte and re-scan for MAGIC.
// ====================================================================================================
bool Transports::pumpSerial(SerialRx &rx, uint8_t *out, uint16_t &len, uint16_t maxLen) {
  if (!rx.stream) return false;

  // Ingest available bytes (bounded by buffer capacity).
  while (rx.stream->available() && rx.count < sizeof(rx.buf)) {
    rx.buf[rx.count++] = (uint8_t)rx.stream->read();
  }

  // Try to parse a frame from the front of the buffer.
  while (rx.count >= 4) {
    // Find MAGIC at position 0; if not present, slide.
    if (!(rx.buf[0] == TP_MAGIC0 && rx.buf[1] == TP_MAGIC1)) {
      // Slide forward by one byte and retry.
      memmove(rx.buf, rx.buf + 1, --rx.count);
      continue;
    }

    uint16_t plen = ((uint16_t)rx.buf[2] << 8) | rx.buf[3];
    if (plen > ISOTP_MAX_PDU) {
      // Bad length -> treat first MAGIC byte as junk, resync.
      memmove(rx.buf, rx.buf + 1, --rx.count);
      continue;
    }

    uint16_t frameLen = 4 + plen + 2;   // MAGIC+LEN + PDU + CRC
    if (rx.count < frameLen) {
      // Need more bytes; keep what we have. Guard against a runaway buffer.
      if (rx.count >= sizeof(rx.buf) - 1 && frameLen > sizeof(rx.buf)) {
        // Frame can never fit -> drop one byte to recover.
        memmove(rx.buf, rx.buf + 1, --rx.count);
        continue;
      }
      return false;   // wait for the rest
    }

    // Validate CRC over MAGIC+LEN+PDU.
    uint16_t crc = crc16_ccitt(rx.buf, 4 + plen, 0xFFFF);
    uint16_t rxCrc = ((uint16_t)rx.buf[4 + plen] << 8) | rx.buf[4 + plen + 1];

    if (crc == rxCrc) {
      uint16_t n = (plen <= maxLen) ? plen : maxLen;
      memcpy(out, &rx.buf[4], n);
      len = n;
      // Consume the whole frame.
      uint16_t remaining = rx.count - frameLen;
      memmove(rx.buf, rx.buf + frameLen, remaining);
      rx.count = remaining;
      return true;
    } else {
      // CRC fail -> slide forward one byte (past this MAGIC) and re-scan.
      memmove(rx.buf, rx.buf + 1, --rx.count);
      continue;
    }
  }
  return false;
}

// ====================================================================================================
//  getPdu / sendPdu (uniform API used by dpp.cpp)
// ====================================================================================================
bool Transports::getPdu(uint8_t *out, uint16_t &len, uint16_t maxLen, TransportId &src) {
  // USB first (most common for the configurator), then UART, then CAN.
  if (pumpSerial(_usb, out, len, maxLen))  { src = TR_USB;  return true; }
  if (pumpSerial(_uart, out, len, maxLen)) { src = TR_UART; return true; }

  if (_iso && _iso->isBound()) {
    if (_iso->poll(out, len, maxLen)) { src = TR_CAN; return true; }
  }
  src = TR_NONE;
  return false;
}

bool Transports::sendPdu(const uint8_t *pdu, uint16_t len, TransportId dst) {
  switch (dst) {
    case TR_USB:  return sendSerial(_usb.stream,  pdu, len);
    case TR_UART: return sendSerial(_uart.stream, pdu, len);
    case TR_CAN:  return _iso ? _iso->sendPdu(pdu, len, DPP_P2_STAR_MS) : false;
    default:      return false;
  }
}
