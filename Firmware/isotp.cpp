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
//  isotp.cpp  -  ISO-TP receive state machine + segmented transmit. PCI nibbles:
//                0x0=SingleFrame 0x1=FirstFrame 0x2=ConsecutiveFrame 0x3=FlowControl.
// ----------------------------------------------------------------------------------------------------

#include "isotp.h"

// ISO-TP PCI type (high nibble of byte 0)
#define PCI_SF   0x0
#define PCI_FF   0x1
#define PCI_CF   0x2
#define PCI_FC   0x3

// Flow Control flow-status (low nibble of byte 0)
#define FC_CTS   0x0   // clear to send
#define FC_WAIT  0x1
#define FC_OVFLW 0x2

IsoTp::IsoTp()
  : _can(nullptr), _rxId(DPP_CAN_REQ_ID), _txId(DPP_CAN_RESP_ID),
    _rxState(RX_IDLE), _rxLen(0), _rxGot(0), _rxNextSn(0), _rxLastMs(0) {}

void IsoTp::bind(MCP2518FD *can, uint32_t rxId, uint32_t txId) {
  _can = can;
  _rxId = rxId;
  _txId = txId;
  _rxState = RX_IDLE;
  _rxLen = _rxGot = 0;
}

// --------------------------------------------------------------------------------------------------
//  Send one 8-byte (padded) frame on the response ID.
// --------------------------------------------------------------------------------------------------
bool IsoTp::sendRaw(const uint8_t *frame8, uint8_t dlc) {
  if (!_can) return false;
  CanFrame f;
  memset(&f, 0, sizeof(f));
  f.id = _txId;
  f.extended = false;
  f.fd = false;
  f.brs = false;
  f.len = 8;                              // FRAME_PAD: always 8 bytes
  memset(f.data, ISOTP_PAD_BYTE, 8);      // pad with 0xAA
  memcpy(f.data, frame8, (dlc <= 8) ? dlc : 8);
  return _can->sendFrame(f);
}

void IsoTp::sendFlowControl(uint8_t fs, uint8_t bs, uint8_t stmin) {
  uint8_t fc[3];
  fc[0] = (PCI_FC << 4) | (fs & 0x0F);
  fc[1] = bs;
  fc[2] = stmin;
  sendRaw(fc, 3);
}

// --------------------------------------------------------------------------------------------------
//  Receive path: feed frames that arrived on _rxId.
// --------------------------------------------------------------------------------------------------
bool IsoTp::onCanFrame(const CanFrame &f) {
  if (!_can) return false;
  if (f.id != _rxId) return false;        // not ours -> let MITM handle it
  if (f.len < 1) return true;             // consumed but empty

  uint8_t pci = (f.data[0] >> 4) & 0x0F;

  switch (pci) {
    case PCI_SF: {
      uint16_t sfLen = f.data[0] & 0x0F;  // classic SF: length in low nibble (1..7)
      if (sfLen == 0 || sfLen > 7) return true;
      _rxLen = sfLen;
      _rxGot = 0;
      for (uint16_t i = 0; i < sfLen && (1 + i) < f.len; i++) _rxBuf[_rxGot++] = f.data[1 + i];
      _rxState = RX_COMPLETE;
      _rxLastMs = millis();
      return true;
    }
    case PCI_FF: {
      // First Frame: 12-bit length = ((data[0]&0x0F)<<8) | data[1], 6 data bytes follow.
      _rxLen = (((uint16_t)(f.data[0] & 0x0F)) << 8) | f.data[1];
      if (_rxLen == 0 || _rxLen > ISOTP_MAX_PDU) { _rxState = RX_IDLE; return true; }
      _rxGot = 0;
      for (uint16_t i = 0; i < 6 && (2 + i) < f.len; i++) _rxBuf[_rxGot++] = f.data[2 + i];
      _rxNextSn = 1;
      _rxState = RX_RECEIVING;
      _rxLastMs = millis();
      // Send Flow Control: clear-to-send, no block size limit, no STmin.
      sendFlowControl(FC_CTS, 0x00, 0x00);
      return true;
    }
    case PCI_CF: {
      if (_rxState != RX_RECEIVING) return true;   // unexpected CF
      uint8_t sn = f.data[0] & 0x0F;
      if (sn != _rxNextSn) { _rxState = RX_IDLE; return true; }  // sequence error -> abort
      for (uint16_t i = 0; i < 7 && (1 + i) < f.len && _rxGot < _rxLen; i++) _rxBuf[_rxGot++] = f.data[1 + i];
      _rxNextSn = (uint8_t)((_rxNextSn + 1) & 0x0F);
      _rxLastMs = millis();
      if (_rxGot >= _rxLen) _rxState = RX_COMPLETE;
      return true;
    }
    case PCI_FC:
    default:
      // FC on the request ID is unusual for us; ignore.
      return true;
  }
}

// --------------------------------------------------------------------------------------------------
//  poll(): expose a completed PDU, and time out a stalled multiframe receive (N_Cr).
// --------------------------------------------------------------------------------------------------
bool IsoTp::poll(uint8_t *out, uint16_t &len, uint16_t maxLen) {
  if (_rxState == RX_RECEIVING) {
    if (millis() - _rxLastMs > 1000) { _rxState = RX_IDLE; }   // N_Cr ~1s -> abort
    return false;
  }
  if (_rxState == RX_COMPLETE) {
    uint16_t n = (_rxLen <= maxLen) ? _rxLen : maxLen;
    memcpy(out, _rxBuf, n);
    len = n;
    _rxState = RX_IDLE;
    return true;
  }
  return false;
}

// --------------------------------------------------------------------------------------------------
//  Wait for a Flow Control frame from the peer (used by the segmented transmit path).
// --------------------------------------------------------------------------------------------------
bool IsoTp::waitFlowControl(uint8_t &fs, uint8_t &bs, uint8_t &stmin, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    CanFrame f;
    if (_can->available() && _can->readFrame(f)) {
      if (f.id == _rxId && f.len >= 3 && ((f.data[0] >> 4) & 0x0F) == PCI_FC) {
        fs = f.data[0] & 0x0F;
        bs = f.data[1];
        stmin = f.data[2];
        return true;
      }
      // Any non-FC frame during a transmit handshake is fed to the RX machine for safety.
      onCanFrame(f);
    }
    delay(1);
  }
  return false;
}

// --------------------------------------------------------------------------------------------------
//  Transmit a complete PDU (SF if <=7 bytes, else FF + CF blocks honouring FC).
// --------------------------------------------------------------------------------------------------
bool IsoTp::sendPdu(const uint8_t *pdu, uint16_t len, uint32_t timeoutMs) {
  if (!_can) return false;

  // ---- Single Frame ----
  if (len <= 7) {
    uint8_t fr[8];
    memset(fr, ISOTP_PAD_BYTE, 8);
    fr[0] = (PCI_SF << 4) | (len & 0x0F);
    memcpy(&fr[1], pdu, len);
    return sendRaw(fr, 8);
  }

  // ---- First Frame ----
  uint8_t ff[8];
  memset(ff, ISOTP_PAD_BYTE, 8);
  ff[0] = (PCI_FF << 4) | ((len >> 8) & 0x0F);
  ff[1] = len & 0xFF;
  memcpy(&ff[2], pdu, 6);
  if (!sendRaw(ff, 8)) return false;
  uint16_t sent = 6;

  // ---- Wait for the peer's Flow Control ----
  uint8_t fs = FC_CTS, bs = 0, stmin = 0;
  if (!waitFlowControl(fs, bs, stmin, timeoutMs)) return false;
  if (fs == FC_OVFLW) return false;

  // ---- Consecutive Frames ----
  uint8_t sn = 1;
  uint16_t blockCount = 0;
  while (sent < len) {
    if (fs == FC_WAIT) {
      // Peer asked us to wait; re-arm for another FC.
      if (!waitFlowControl(fs, bs, stmin, timeoutMs)) return false;
      if (fs == FC_OVFLW) return false;
      continue;
    }

    uint8_t cf[8];
    memset(cf, ISOTP_PAD_BYTE, 8);
    cf[0] = (PCI_CF << 4) | (sn & 0x0F);
    uint16_t chunk = (len - sent > 7) ? 7 : (len - sent);
    memcpy(&cf[1], &pdu[sent], chunk);
    if (!sendRaw(cf, 8)) return false;
    sent += chunk;
    sn = (uint8_t)((sn + 1) & 0x0F);
    blockCount++;

    // STmin spacing between consecutive frames.
    if (stmin > 0 && stmin <= 0x7F) delay(stmin);                 // 1..127 ms
    else if (stmin >= 0xF1 && stmin <= 0xF9) delayMicroseconds((stmin - 0xF0) * 100); // 100..900 us

    // Honour block size: after BS frames, wait for the next FC (BS==0 => unlimited).
    if (bs != 0 && blockCount >= bs && sent < len) {
      if (!waitFlowControl(fs, bs, stmin, timeoutMs)) return false;
      if (fs == FC_OVFLW) return false;
      blockCount = 0;
    }
  }
  return true;
}
