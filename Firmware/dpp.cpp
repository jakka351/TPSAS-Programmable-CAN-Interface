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
//  dpp.cpp  -  TP-DPP service engine implementation. Byte layouts follow Docs/Device_Programming_-
//              Protocol.md exactly (see §3/§4/§6/§8 and the §9 worked examples).
// ----------------------------------------------------------------------------------------------------

#include "dpp.h"
#include "isotp.h"

#include <Preferences.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_app_format.h>
#include <esp_mac.h>

static Preferences s_prefs;

// ====================================================================================================
//  CRC helpers
// ====================================================================================================
uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc) {
  // CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0x0000.
  for (size_t i = 0; i < len; i++) {
    crc ^= ((uint16_t)data[i]) << 8;
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
      else              crc = (uint16_t)(crc << 1);
    }
  }
  return crc;
}

uint32_t crc32_ieee(const uint8_t *data, size_t len, uint32_t crc) {
  // CRC-32/ISO-HDLC (zlib/esptool): reflected poly 0xEDB88320. Call with crc=0xFFFFFFFF, then this
  // function returns the FINAL value (already XORed with 0xFFFFFFFF) for a single call.
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 1u) crc = (crc >> 1) ^ 0xEDB88320u;
      else          crc = (crc >> 1);
    }
  }
  return ~crc;
}

// ====================================================================================================
//  Construction / setup
// ====================================================================================================
DppEngine::DppEngine()
  : _tr(nullptr), _can1(nullptr), _can2(nullptr),
    _progBusSwitch(nullptr), _configChanged(nullptr),
    _session(SESSION_DEFAULT), _securityUnlocked(false), _seed(0), _progBus(PROG_BUS_CAN1),
    _downloadActive(false), _downloadSize(0), _downloadReceived(0),
    _downloadCrc32Decl(0), _downloadCrc32Calc(0xFFFFFFFF),
    _expectedBsc(1), _maxBlock(0x0200),
    _otaBegun(false), _otaHandle(0), _otaPart(nullptr), _otaStaged(false) {
  loadDefaultConfig();
}

void DppEngine::begin(Transports *tr,
                      MCP2518FD *can1, MCP2518FD *can2,
                      void (*progBusSwitch)(uint8_t bus),
                      void (*configChanged)(const uint8_t *blob, uint16_t len)) {
  _tr = tr;
  _can1 = can1;
  _can2 = can2;
  _progBusSwitch = progBusSwitch;
  _configChanged = configChanged;
}

// ====================================================================================================
//  Config blob (DID 0x0100) + programming bus (DID 0x0103) persistence
// ====================================================================================================
void DppEngine::loadDefaultConfig() {
  memset(_cfg, 0, sizeof(_cfg));
  _cfg[CFG_OFF_VERSION]  = CFG_VERSION;
  _cfg[CFG_OFF_MITM_MODE] = MITM_MODE_PASSTHRU;
  // can1_bitrate / can2_bitrate (big-endian)
  uint32_t br = CAN_DEFAULT_NOMINAL_BITRATE;
  _cfg[CFG_OFF_CAN1_BR + 0] = (br >> 24) & 0xFF; _cfg[CFG_OFF_CAN1_BR + 1] = (br >> 16) & 0xFF;
  _cfg[CFG_OFF_CAN1_BR + 2] = (br >> 8)  & 0xFF; _cfg[CFG_OFF_CAN1_BR + 3] =  br        & 0xFF;
  _cfg[CFG_OFF_CAN2_BR + 0] = (br >> 24) & 0xFF; _cfg[CFG_OFF_CAN2_BR + 1] = (br >> 16) & 0xFF;
  _cfg[CFG_OFF_CAN2_BR + 2] = (br >> 8)  & 0xFF; _cfg[CFG_OFF_CAN2_BR + 3] =  br        & 0xFF;
  _cfg[CFG_OFF_FD_ENABLE] = 0x00;
  uint32_t dbr = CAN_DEFAULT_DATA_BITRATE;
  _cfg[CFG_OFF_FD_DATA_BR + 0] = (dbr >> 24) & 0xFF; _cfg[CFG_OFF_FD_DATA_BR + 1] = (dbr >> 16) & 0xFF;
  _cfg[CFG_OFF_FD_DATA_BR + 2] = (dbr >> 8)  & 0xFF; _cfg[CFG_OFF_FD_DATA_BR + 3] =  dbr        & 0xFF;
  _cfg[CFG_OFF_TERM1] = 0x01;
  _cfg[CFG_OFF_TERM2] = 0x01;
  memset(&_cfg[CFG_OFF_NAME], 0, CFG_NAME_LEN);
  strncpy((char *)&_cfg[CFG_OFF_NAME], CFG_DEFAULT_NAME, CFG_NAME_LEN);
  uint16_t crc = crc16_ccitt(_cfg, CFG_OFF_CRC16, 0xFFFF);
  _cfg[CFG_OFF_CRC16 + 0] = (crc >> 8) & 0xFF;
  _cfg[CFG_OFF_CRC16 + 1] =  crc       & 0xFF;
}

bool DppEngine::validateConfig(const uint8_t *blob, uint16_t len) {
  if (len != CFG_BLOB_LEN) return false;
  if (blob[CFG_OFF_VERSION] != CFG_VERSION) return false;
  if (blob[CFG_OFF_MITM_MODE] > MITM_MODE_BLOCK) return false;
  uint16_t crc = crc16_ccitt(blob, CFG_OFF_CRC16, 0xFFFF);
  uint16_t got = ((uint16_t)blob[CFG_OFF_CRC16] << 8) | blob[CFG_OFF_CRC16 + 1];
  return crc == got;
}

void DppEngine::persistConfig() {
  s_prefs.begin(NVS_NAMESPACE, false);
  s_prefs.putBytes(NVS_KEY_CONFIG, _cfg, CFG_BLOB_LEN);
  s_prefs.end();
}

void DppEngine::persistProgBus() {
  s_prefs.begin(NVS_NAMESPACE, false);
  s_prefs.putUChar(NVS_KEY_PROGBUS, _progBus);
  s_prefs.end();
}

void DppEngine::loadPersisted() {
  s_prefs.begin(NVS_NAMESPACE, true);   // read-only
  uint8_t tmp[CFG_BLOB_LEN];
  size_t n = s_prefs.getBytes(NVS_KEY_CONFIG, tmp, CFG_BLOB_LEN);
  uint8_t pb = s_prefs.getUChar(NVS_KEY_PROGBUS, PROG_BUS_CAN1);
  s_prefs.end();

  if (n == CFG_BLOB_LEN && validateConfig(tmp, CFG_BLOB_LEN)) {
    memcpy(_cfg, tmp, CFG_BLOB_LEN);
  } // else keep defaults already loaded in the ctor

  _progBus = (pb <= PROG_BUS_CAN2) ? pb : PROG_BUS_CAN1;
}

// ====================================================================================================
//  Response builders
// ====================================================================================================
uint16_t DppEngine::mkPositive(uint8_t sid, const uint8_t *data, uint16_t dlen) {
  _resp[0] = sid + POSITIVE_OFFSET;
  if (dlen && data) memcpy(&_resp[1], data, dlen);
  return (uint16_t)(1 + dlen);
}

uint16_t DppEngine::mkNegative(uint8_t sid, uint8_t nrc) {
  _resp[0] = SID_NEGATIVE;
  _resp[1] = sid;
  _resp[2] = nrc;
  return 3;
}

void DppEngine::sendResponsePending(uint8_t sid, TransportId src) {
  uint8_t p[3] = { SID_NEGATIVE, sid, NRC_RESPONSE_PENDING };
  _tr->sendPdu(p, 3, src);
}

// ====================================================================================================
//  Top-level pump + dispatch
// ====================================================================================================
void DppEngine::service() {
  static uint8_t req[ISOTP_MAX_PDU];
  uint16_t len = 0;
  TransportId src = TR_NONE;

  if (!_tr->getPdu(req, len, sizeof(req), src)) return;
  if (len == 0) return;

  uint16_t rlen = dispatch(req, len, src);
  if (rlen > 0) {
    _tr->sendPdu(_resp, rlen, src);
  }
}

uint16_t DppEngine::dispatch(const uint8_t *req, uint16_t len, TransportId src) {
  uint8_t sid = req[0];
  switch (sid) {
    case SID_SESSION_CONTROL:   return svcSession(req, len);
    case SID_ECU_RESET:         return svcReset(req, len, src);
    case SID_SECURITY_ACCESS:   return svcSecurity(req, len);
    case SID_TESTER_PRESENT:    return svcTesterPresent(req, len);
    case SID_READ_DID:          return svcReadDid(req, len);
    case SID_WRITE_DID:         return svcWriteDid(req, len);
    case SID_READ_MEMORY:       return svcReadMemory(req, len);
    case SID_ROUTINE_CONTROL:   return svcRoutine(req, len, src);
    case SID_REQUEST_DOWNLOAD:  return svcRequestDownload(req, len);
    case SID_TRANSFER_DATA:     return svcTransferData(req, len, src);
    case SID_REQUEST_XFER_EXIT: return svcTransferExit(req, len);
    default:                    return mkNegative(sid, NRC_SERVICE_NOT_SUPPORTED);
  }
}

// ====================================================================================================
//  0x10 SESSION_CONTROL  : subfn (01 default, 02 programming) -> subfn + P2(2) + P2*(2)
// ====================================================================================================
uint16_t DppEngine::svcSession(const uint8_t *req, uint16_t len) {
  if (len < 2) return mkNegative(SID_SESSION_CONTROL, NRC_INVALID_FORMAT);
  uint8_t sub = req[1];
  if (sub != SESSION_DEFAULT && sub != SESSION_PROGRAMMING)
    return mkNegative(SID_SESSION_CONTROL, NRC_REQUEST_OUT_OF_RANGE);

  _session = sub;
  if (sub == SESSION_DEFAULT) {
    // Leaving programming session resets security + any in-flight download.
    _securityUnlocked = false;
    if (_downloadActive) otaAbort();
  }

  uint8_t d[5];
  d[0] = sub;
  d[1] = (DPP_P2_MS >> 8) & 0xFF;       d[2] = DPP_P2_MS & 0xFF;
  d[3] = (DPP_P2_STAR_MS >> 8) & 0xFF;  d[4] = DPP_P2_STAR_MS & 0xFF;
  return mkPositive(SID_SESSION_CONTROL, d, 5);
}

// ====================================================================================================
//  0x11 ECU_RESET  : subfn (01 hardReset, 03 jumpToApp) -> subfn   (reset happens AFTER the response)
// ====================================================================================================
uint16_t DppEngine::svcReset(const uint8_t *req, uint16_t len, TransportId src) {
  if (len < 2) return mkNegative(SID_ECU_RESET, NRC_INVALID_FORMAT);
  uint8_t sub = req[1];
  if (sub != 0x01 && sub != 0x03)
    return mkNegative(SID_ECU_RESET, NRC_REQUEST_OUT_OF_RANGE);

  // Send the positive response NOW (so the PC sees 51 01 before we drop the link), then reset.
  uint8_t d[1] = { sub };
  uint16_t rl = mkPositive(SID_ECU_RESET, d, 1);
  _tr->sendPdu(_resp, rl, src);
  delay(50);                 // let the frame flush over USB/UART/CAN
  esp_restart();             // hard reset; boots the (possibly newly-activated) image
  return 0;                  // not reached
}

// ====================================================================================================
//  0x27 SECURITY_ACCESS  : 01 requestSeed -> seed(4) ; 02 sendKey+key(4) -> (empty)
//   Open build (DPP_SECURITY=0): seed = 0, any key accepted.
// ====================================================================================================
uint16_t DppEngine::svcSecurity(const uint8_t *req, uint16_t len) {
  if (len < 2) return mkNegative(SID_SECURITY_ACCESS, NRC_INVALID_FORMAT);
  uint8_t sub = req[1];

  if (sub == 0x01) {            // requestSeed
#if DPP_SECURITY
    _seed = (uint32_t)esp_random();
    if (_seed == 0) _seed = 0x12345678;   // never hand out an "already unlocked" seed
#else
    _seed = 0;                  // open build
#endif
    uint8_t d[4] = { (uint8_t)(_seed >> 24), (uint8_t)(_seed >> 16),
                     (uint8_t)(_seed >> 8),  (uint8_t)(_seed) };
    if (_seed == 0) _securityUnlocked = true;   // seed 0 means "already unlocked"
    return mkPositive(SID_SECURITY_ACCESS, d, 4);
  }

  if (sub == 0x02) {            // sendKey
    if (len < 6) return mkNegative(SID_SECURITY_ACCESS, NRC_INVALID_FORMAT);
    uint32_t key = ((uint32_t)req[2] << 24) | ((uint32_t)req[3] << 16) |
                   ((uint32_t)req[4] << 8)  |  (uint32_t)req[5];
#if DPP_SECURITY
    // Simple example transform: key == seed XOR 0xA5A5A5A5. Replace for production.
    uint32_t expect = _seed ^ 0xA5A5A5A5u;
    if (key != expect) return mkNegative(SID_SECURITY_ACCESS, NRC_INVALID_KEY);
#else
    (void)key;                  // any key accepted on open builds
#endif
    _securityUnlocked = true;
    return mkPositive(SID_SECURITY_ACCESS, nullptr, 0);
  }

  return mkNegative(SID_SECURITY_ACCESS, NRC_REQUEST_OUT_OF_RANGE);
}

// ====================================================================================================
//  0x3E TESTER_PRESENT  : 00 -> 00
// ====================================================================================================
uint16_t DppEngine::svcTesterPresent(const uint8_t *req, uint16_t len) {
  if (len < 2 || req[1] != 0x00) return mkNegative(SID_TESTER_PRESENT, NRC_INVALID_FORMAT);
  uint8_t d[1] = { 0x00 };
  return mkPositive(SID_TESTER_PRESENT, d, 1);
}

// ====================================================================================================
//  0x22 READ_DATA_BY_ID  : DID(2) -> DID(2) + data
// ====================================================================================================
uint16_t DppEngine::readDidPayload(uint16_t did, uint8_t *out, uint16_t maxOut, bool &ok) {
  ok = true;
  switch (did) {
    case DID_PROTOCOL_VERSION: {
      const char *s = TP_PROTOCOL_VERSION_STR;
      uint16_t n = strlen(s); if (n > maxOut) n = maxOut;
      memcpy(out, s, n); return n;
    }
    case DID_APP_VERSION: {
      const char *s = TP_APP_VERSION_STR;
      uint16_t n = strlen(s); if (n > maxOut) n = maxOut;
      memcpy(out, s, n); return n;
    }
    case DID_SERIAL_NUMBER: {
      // 12 bytes derived from the factory eFuse MAC (stable per unit).
      uint8_t mac[8] = {0};
      esp_efuse_mac_get_default(mac);
      uint8_t sn[12];
      for (int i = 0; i < 12; i++) sn[i] = mac[i % 6] ^ (uint8_t)(0x30 + i);
      uint16_t n = (maxOut < 12) ? maxOut : 12;
      memcpy(out, sn, n); return n;
    }
    case DID_CHIP_MAC: {
      uint8_t mac[8] = {0};
      esp_efuse_mac_get_default(mac);
      uint16_t n = (maxOut < 6) ? maxOut : 6;
      memcpy(out, mac, n); return n;
    }
    case DID_PARTITION_MAP: {
      // running(1) slotA_size(4) slotB_size(4) appUsed(4)  (big-endian sizes)
      if (maxOut < 13) { ok = false; return 0; }
      const esp_partition_t *run = esp_ota_get_running_partition();
      uint8_t running = 0;
      if (run && run->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) running = 1;
      uint32_t a = slotSizeA(), b = slotSizeB(), used = appUsed();
      out[0] = running;
      out[1] = (a >> 24) & 0xFF; out[2] = (a >> 16) & 0xFF; out[3] = (a >> 8) & 0xFF; out[4] = a & 0xFF;
      out[5] = (b >> 24) & 0xFF; out[6] = (b >> 16) & 0xFF; out[7] = (b >> 8) & 0xFF; out[8] = b & 0xFF;
      out[9] = (used >> 24) & 0xFF; out[10] = (used >> 16) & 0xFF; out[11] = (used >> 8) & 0xFF; out[12] = used & 0xFF;
      return 13;
    }
    case DID_RUNNING_SLOT: {
      const esp_partition_t *run = esp_ota_get_running_partition();
      out[0] = (run && run->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? 1 : 0;
      return 1;
    }
    case DID_CONFIG_BLOB: {
      uint16_t n = (maxOut < CFG_BLOB_LEN) ? maxOut : CFG_BLOB_LEN;
      memcpy(out, _cfg, n); return n;
    }
    case DID_PROG_BUS_SELECT: {
      out[0] = _progBus; return 1;
    }
    default:
      ok = false; return 0;
  }
}

uint16_t DppEngine::svcReadDid(const uint8_t *req, uint16_t len) {
  if (len < 3) return mkNegative(SID_READ_DID, NRC_INVALID_FORMAT);
  uint16_t did = ((uint16_t)req[1] << 8) | req[2];

  uint8_t payload[256];
  bool ok = false;
  uint16_t n = readDidPayload(did, payload, sizeof(payload), ok);
  if (!ok) return mkNegative(SID_READ_DID, NRC_REQUEST_OUT_OF_RANGE);

  // Response: DID(2) + data
  _resp[0] = SID_READ_DID + POSITIVE_OFFSET;
  _resp[1] = (did >> 8) & 0xFF;
  _resp[2] = did & 0xFF;
  memcpy(&_resp[3], payload, n);
  return (uint16_t)(3 + n);
}

// ====================================================================================================
//  0x2E WRITE_DATA_BY_ID  : DID(2) + data -> DID(2)
// ====================================================================================================
uint16_t DppEngine::svcWriteDid(const uint8_t *req, uint16_t len) {
  if (len < 3) return mkNegative(SID_WRITE_DID, NRC_INVALID_FORMAT);
  uint16_t did = ((uint16_t)req[1] << 8) | req[2];
  const uint8_t *data = &req[3];
  uint16_t dlen = len - 3;

  switch (did) {
    case DID_CONFIG_BLOB: {
      if (!validateConfig(data, dlen))
        return mkNegative(SID_WRITE_DID, NRC_REQUEST_OUT_OF_RANGE);
      memcpy(_cfg, data, CFG_BLOB_LEN);
      persistConfig();
      if (_configChanged) _configChanged(_cfg, CFG_BLOB_LEN);   // live-apply MITM mode etc.
      break;
    }
    case DID_PROG_BUS_SELECT: {
      if (dlen < 1 || data[0] > PROG_BUS_CAN2)
        return mkNegative(SID_WRITE_DID, NRC_REQUEST_OUT_OF_RANGE);
      _progBus = data[0];
      persistProgBus();
      if (_progBusSwitch) _progBusSwitch(_progBus);             // re-bind ISO-TP to the new bus
      break;
    }
    default:
      return mkNegative(SID_WRITE_DID, NRC_REQUEST_OUT_OF_RANGE);  // read-only / unknown DID
  }

  uint8_t d[2] = { (uint8_t)(did >> 8), (uint8_t)(did & 0xFF) };
  return mkPositive(SID_WRITE_DID, d, 2);
}

// ====================================================================================================
//  0x23 READ_MEMORY  : addr(4) + len(2) -> data (len <= 1024)
//   addr is a flash offset; we read from the running app partition (for backup dumps).
// ====================================================================================================
uint16_t DppEngine::svcReadMemory(const uint8_t *req, uint16_t len) {
  if (len < 7) return mkNegative(SID_READ_MEMORY, NRC_INVALID_FORMAT);
  uint32_t addr = ((uint32_t)req[1] << 24) | ((uint32_t)req[2] << 16) |
                  ((uint32_t)req[3] << 8)  |  (uint32_t)req[4];
  uint16_t rlen = ((uint16_t)req[5] << 8) | req[6];
  if (rlen == 0 || rlen > 1024) return mkNegative(SID_READ_MEMORY, NRC_REQUEST_OUT_OF_RANGE);

  const esp_partition_t *run = esp_ota_get_running_partition();
  if (!run) return mkNegative(SID_READ_MEMORY, NRC_CONDITIONS_NOT_CORRECT);
  if ((uint64_t)addr + rlen > run->size)
    return mkNegative(SID_READ_MEMORY, NRC_REQUEST_OUT_OF_RANGE);

  _resp[0] = SID_READ_MEMORY + POSITIVE_OFFSET;
  if (esp_partition_read(run, addr, &_resp[1], rlen) != ESP_OK)
    return mkNegative(SID_READ_MEMORY, NRC_GENERAL_REJECT);
  return (uint16_t)(1 + rlen);
}

// ====================================================================================================
//  0x31 ROUTINE_CONTROL  : 01 start routineId(2) [+opt] -> 01 + routineId(2) + status(1) [+result]
// ====================================================================================================
uint16_t DppEngine::svcRoutine(const uint8_t *req, uint16_t len, TransportId src) {
  if (len < 4) return mkNegative(SID_ROUTINE_CONTROL, NRC_INVALID_FORMAT);
  uint8_t sub = req[1];
  if (sub != 0x01) return mkNegative(SID_ROUTINE_CONTROL, NRC_REQUEST_OUT_OF_RANGE);
  uint16_t rid = ((uint16_t)req[2] << 8) | req[3];

  switch (rid) {
    case RID_ERASE_INACTIVE: {
      if (_session != SESSION_PROGRAMMING)
        return mkNegative(SID_ROUTINE_CONTROL, NRC_CONDITIONS_NOT_CORRECT);
      // Long op -> announce responsePending first.
      sendResponsePending(SID_ROUTINE_CONTROL, src);
      // "Erase" = prepare the inactive OTA slot for a fresh write via Update.begin().
      bool ok = otaBegin();
      uint8_t status = ok ? 0x00 : 0x02;     // 0x00 ok / 0x02 fail
      uint8_t d[4] = { 0x01, (uint8_t)(rid >> 8), (uint8_t)(rid & 0xFF), status };
      return mkPositive(SID_ROUTINE_CONTROL, d, 4);
    }

    case RID_VERIFY_IMAGE: {
      // Extra req: crc32(4) expected. Compare against what we computed while writing.
      if (len < 8) return mkNegative(SID_ROUTINE_CONTROL, NRC_INVALID_FORMAT);
      uint32_t expect = ((uint32_t)req[4] << 24) | ((uint32_t)req[5] << 16) |
                        ((uint32_t)req[6] << 8)  |  (uint32_t)req[7];
      sendResponsePending(SID_ROUTINE_CONTROL, src);
      // _downloadCrc32Calc holds the authoritative full-image CRC32 (recomputed from the staged
      // partition in otaFinalize()); compare it to the expected value the PC supplied.
      uint8_t status = (expect == _downloadCrc32Calc) ? 0x00 : 0x03;   // 0x00 match / 0x03 mismatch
      uint8_t d[4] = { 0x01, (uint8_t)(rid >> 8), (uint8_t)(rid & 0xFF), status };
      return mkPositive(SID_ROUTINE_CONTROL, d, 4);
    }

    case RID_ACTIVATE_IMAGE: {
      sendResponsePending(SID_ROUTINE_CONTROL, src);
      bool ok = otaActivate();
      uint8_t status = ok ? 0x00 : 0x02;
      uint8_t d[4] = { 0x01, (uint8_t)(rid >> 8), (uint8_t)(rid & 0xFF), status };
      return mkPositive(SID_ROUTINE_CONTROL, d, 4);
    }

    case RID_SELF_TEST: {
      // bitmask result(1): bit0 CAN1 loopback ok, bit1 CAN2 loopback ok, bit2 iso rails present.
      uint8_t mask = 0;
      if (_can1 && _can1->loopbackSelfTest()) mask |= 0x01;
      if (_can2 && _can2->loopbackSelfTest()) mask |= 0x02;
      mask |= 0x04;   // isolated rails: assumed present (no ADC sense line on this build)
      uint8_t d[5] = { 0x01, (uint8_t)(rid >> 8), (uint8_t)(rid & 0xFF), 0x00, mask };
      return mkPositive(SID_ROUTINE_CONTROL, d, 5);
    }

    default:
      return mkNegative(SID_ROUTINE_CONTROL, NRC_REQUEST_OUT_OF_RANGE);
  }
}

// ====================================================================================================
//  0x34 REQUEST_DOWNLOAD : fmt(1) alfid(1) addr(4) size(4) crc32(4) -> lenFmt(1) + maxBlock(2)
// ====================================================================================================
uint16_t DppEngine::svcRequestDownload(const uint8_t *req, uint16_t len) {
  if (_session != SESSION_PROGRAMMING)
    return mkNegative(SID_REQUEST_DOWNLOAD, NRC_CONDITIONS_NOT_CORRECT);
#if DPP_SECURITY
  if (!_securityUnlocked)
    return mkNegative(SID_REQUEST_DOWNLOAD, NRC_CONDITIONS_NOT_CORRECT);
#endif
  // fmt(1) alfid(1) addr(4) size(4) crc32(4) = 14 bytes + SID = 15
  if (len < 15) return mkNegative(SID_REQUEST_DOWNLOAD, NRC_INVALID_FORMAT);
  uint8_t fmt   = req[1];
  uint8_t alfid = req[2];
  uint32_t addr = ((uint32_t)req[3] << 24) | ((uint32_t)req[4] << 16) |
                  ((uint32_t)req[5] << 8)  |  (uint32_t)req[6];
  uint32_t size = ((uint32_t)req[7] << 24) | ((uint32_t)req[8] << 16) |
                  ((uint32_t)req[9] << 8)  |  (uint32_t)req[10];
  uint32_t crc  = ((uint32_t)req[11] << 24) | ((uint32_t)req[12] << 16) |
                  ((uint32_t)req[13] << 8)  |  (uint32_t)req[14];

  if (fmt != 0x00 || alfid != 0x44)                  // we mandate fmt=0, alfid=0x44 (4B addr + 4B size)
    return mkNegative(SID_REQUEST_DOWNLOAD, NRC_REQUEST_OUT_OF_RANGE);
  if (addr != 0x00000000)                            // addr 0 means "the inactive OTA slot"
    return mkNegative(SID_REQUEST_DOWNLOAD, NRC_REQUEST_OUT_OF_RANGE);
  if (size == 0 || size > slotSizeB())               // must fit the target slot
    return mkNegative(SID_REQUEST_DOWNLOAD, NRC_REQUEST_OUT_OF_RANGE);

  // (Re)prepare the OTA write if RID_ERASE_INACTIVE was not separately issued.
  if (!_otaBegun) {
    if (!otaBegin()) return mkNegative(SID_REQUEST_DOWNLOAD, NRC_PROGRAMMING_FAILURE);
  }

  _downloadActive    = true;
  _downloadSize      = size;
  _downloadReceived  = 0;
  _downloadCrc32Decl = crc;
  _downloadCrc32Calc = 0xFFFFFFFF;          // running CRC seed; finalised in otaFinalize()
  _expectedBsc       = 1;
  _maxBlock          = 0x0200;              // 512 bytes per block (matches §9 example)

  uint8_t d[3];
  d[0] = 0x02;                              // lenFmt: maxBlock is a 2-byte field
  d[1] = (_maxBlock >> 8) & 0xFF;
  d[2] = _maxBlock & 0xFF;
  return mkPositive(SID_REQUEST_DOWNLOAD, d, 3);
}

// ====================================================================================================
//  0x36 TRANSFER_DATA : bsc(1) + data -> bsc(1)
// ====================================================================================================
uint16_t DppEngine::svcTransferData(const uint8_t *req, uint16_t len, TransportId src) {
  if (!_downloadActive)
    return mkNegative(SID_TRANSFER_DATA, NRC_REQUEST_SEQUENCE_ERROR);
  if (len < 2)
    return mkNegative(SID_TRANSFER_DATA, NRC_INVALID_FORMAT);

  uint8_t bsc = req[1];
  if (bsc != _expectedBsc)
    return mkNegative(SID_TRANSFER_DATA, NRC_WRONG_BLOCK_SEQUENCE);

  uint16_t dlen = len - 2;
  if (dlen > _maxBlock)
    return mkNegative(SID_TRANSFER_DATA, NRC_INVALID_FORMAT);
  if (_downloadReceived + dlen > _downloadSize)
    return mkNegative(SID_TRANSFER_DATA, NRC_REQUEST_OUT_OF_RANGE);

  // Long write may exceed P2 -> announce responsePending first for large blocks.
  if (dlen >= 256) sendResponsePending(SID_TRANSFER_DATA, src);

  if (!otaWrite(&req[2], dlen))
    return mkNegative(SID_TRANSFER_DATA, NRC_PROGRAMMING_FAILURE);

  _downloadReceived += dlen;
  // (The authoritative full-image CRC32 is computed in otaFinalize() by reading back the staged
  //  partition, so we do not accumulate a per-block CRC here.)
  _expectedBsc = (uint8_t)((_expectedBsc == 0xFF) ? 0x00 : (_expectedBsc + 1));  // wrap 0xFF -> 0x00

  uint8_t d[1] = { bsc };
  return mkPositive(SID_TRANSFER_DATA, d, 1);
}

// ====================================================================================================
//  0x37 REQUEST_TRANSFER_EXIT : (none) -> status(1)
// ====================================================================================================
uint16_t DppEngine::svcTransferExit(const uint8_t *req, uint16_t len) {
  (void)req; (void)len;
  if (!_downloadActive)
    return mkNegative(SID_REQUEST_XFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR);

  bool ok = otaFinalize();
  _downloadActive = false;

  uint8_t status = ok ? 0x00 : 0x01;       // 0x00 ok / nonzero = problem
  uint8_t d[1] = { status };
  return mkPositive(SID_REQUEST_XFER_EXIT, d, 1);
}

// ====================================================================================================
//  OTA helpers  (native esp_ota: write to the inactive slot, validate at end, set boot ONLY on
//  activate so the spec's verify-before-activate ordering is honoured; rollback safety on first boot.)
//
//  Flow mapping:
//    routine 0xFF01 erase   -> otaBegin()    (esp_ota_begin erases the inactive slot)
//    0x34 REQUEST_DOWNLOAD  -> otaBegin() if not already begun
//    0x36 TRANSFER_DATA     -> otaWrite()    (esp_ota_write)
//    0x37 REQUEST_XFER_EXIT -> otaFinalize() (esp_ota_end -> image validated; CRC32 read back)
//    routine 0xFF03 activate-> otaActivate() (esp_ota_set_boot_partition)
// ====================================================================================================
bool DppEngine::otaBegin() {
  // Abort any stale handle first.
  if (_otaBegun) { esp_ota_abort((esp_ota_handle_t)_otaHandle); _otaBegun = false; }
  _otaStaged = false;

  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  if (!next) return false;
  _otaPart = (const void *)next;

  esp_ota_handle_t h = 0;
  // OTA_SIZE_UNKNOWN lets esp_ota erase the whole target slot up front (clean "erase inactive slot").
  esp_err_t e = esp_ota_begin(next, OTA_SIZE_UNKNOWN, &h);
  if (e != ESP_OK) return false;
  _otaHandle = (uint32_t)h;
  _otaBegun = true;
  _downloadCrc32Calc = 0xFFFFFFFF;
  return true;
}

bool DppEngine::otaWrite(const uint8_t *d, uint16_t n) {
  if (!_otaBegun) return false;
  return esp_ota_write((esp_ota_handle_t)_otaHandle, d, n) == ESP_OK;
}

bool DppEngine::otaFinalize() {
  if (!_otaBegun) return false;
  // esp_ota_end() validates the staged image (magic/size/secure-boot signature if enabled) but does
  // NOT change the boot partition - exactly what we want before the verify + activate steps.
  esp_err_t e = esp_ota_end((esp_ota_handle_t)_otaHandle);
  _otaBegun = false;
  if (e != ESP_OK) { _otaStaged = false; return false; }
  _otaStaged = true;

  // Recompute CRC32 over exactly _downloadSize bytes of the freshly-written slot so routine 0xFF02
  // verifies the IMAGE on flash, not merely the byte stream we received.
  const esp_partition_t *next = (const esp_partition_t *)_otaPart;
  if (next) {
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buf[512];
    uint32_t remaining = _downloadSize;
    uint32_t off = 0;
    while (remaining) {
      uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
      if (esp_partition_read(next, off, buf, chunk) != ESP_OK) break;
      crc = crc32_ieee(buf, chunk, crc);
      off += chunk; remaining -= chunk;
    }
    _downloadCrc32Calc = crc;   // authoritative full-image CRC for routine 0xFF02
  }
  return true;
}

bool DppEngine::otaActivate() {
  // Only activate a validated staged image. Setting the boot partition arms the rollback-protected
  // first boot (image starts in PENDING_VERIFY and reverts automatically if it never self-confirms).
  if (!_otaStaged || !_otaPart) return false;
  esp_err_t e = esp_ota_set_boot_partition((const esp_partition_t *)_otaPart);
  return (e == ESP_OK);
}

void DppEngine::otaAbort() {
  if (_otaBegun) { esp_ota_abort((esp_ota_handle_t)_otaHandle); _otaBegun = false; }
  _otaStaged = false;
  _downloadActive = false;
  _downloadReceived = 0;
}

// ---- partition introspection -----------------------------------------------------------------------
uint32_t DppEngine::slotSizeA() {
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                      ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  return p ? p->size : 0;
}
uint32_t DppEngine::slotSizeB() {
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                      ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  return p ? p->size : 0;
}
uint32_t DppEngine::appUsed() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (!run) return 0;
  esp_app_desc_t desc;
  if (esp_ota_get_partition_description(run, &desc) == ESP_OK) {
    // No explicit image-size field in the descriptor; report the partition size as the upper bound.
    return run->size;
  }
  return run->size;
}

// ---- rollback confirmation -------------------------------------------------------------------------
void DppEngine::markAppValid() {
  // Tell the bootloader the freshly-booted image is healthy (cancels the pending rollback). Safe to
  // call even when not in the "pending verify" state.
  esp_ota_img_states_t st;
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (run && esp_ota_get_state_partition(run, &st) == ESP_OK) {
    if (st == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_ota_mark_app_valid_cancel_rollback();
    }
  }
}
