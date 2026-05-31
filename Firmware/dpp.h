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
//  dpp.h  -  Transport-agnostic Tester Present Device Programming Protocol (TP-DPP) engine.
//            Parses an Application PDU and dispatches EVERY service in Docs/Device_Programming_Protocol:
//              0x10 SESSION_CONTROL   0x11 ECU_RESET        0x22 READ_DATA_BY_ID  0x23 READ_MEMORY
//              0x27 SECURITY_ACCESS   0x2E WRITE_DATA_BY_ID  0x31 ROUTINE_CONTROL  0x34 REQUEST_DOWNLOAD
//              0x36 TRANSFER_DATA     0x37 REQUEST_TRANSFER_EXIT                   0x3E TESTER_PRESENT
//            OTA firmware write goes to the inactive slot via Update.h / esp_ota, CRC32-verified,
//            boot-set with rollback safety. DIDs, routines, and the §8 config blob (NVS/Preferences)
//            are all here. CRC16-CCITT + CRC32 helpers are exported for transports + config use.
// ----------------------------------------------------------------------------------------------------

#ifndef TP_DPP_H
#define TP_DPP_H

#include <Arduino.h>
#include "config.h"
#include "transports.h"
#include "mcp2518fd.h"

// ----- CRC helpers (shared) -------------------------------------------------------------------------
//  crc16_ccitt : CRC-16/CCITT-FALSE  (poly 0x1021, init 0xFFFF, no reflect, xorout 0x0000).
//                Pass `crc` to chain across buffers; start with 0xFFFF.
uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc);
//  crc32_ieee  : CRC-32/ISO-HDLC (poly 0x04C11DB7 reflected, init 0xFFFFFFFF, xorout 0xFFFFFFFF) -
//                the same CRC32 used by zlib / esptool, so the PC and device agree on image CRCs.
uint32_t crc32_ieee(const uint8_t *data, size_t len, uint32_t crc);

// ----- SIDs -----------------------------------------------------------------------------------------
#define SID_SESSION_CONTROL   0x10
#define SID_ECU_RESET         0x11
#define SID_READ_DID          0x22
#define SID_READ_MEMORY       0x23
#define SID_WRITE_DID         0x2E
#define SID_SECURITY_ACCESS   0x27
#define SID_ROUTINE_CONTROL   0x31
#define SID_REQUEST_DOWNLOAD  0x34
#define SID_TRANSFER_DATA     0x36
#define SID_REQUEST_XFER_EXIT 0x37
#define SID_TESTER_PRESENT    0x3E
#define SID_NEGATIVE          0x7F
#define POSITIVE_OFFSET       0x40

// ----- NRCs -----------------------------------------------------------------------------------------
#define NRC_GENERAL_REJECT         0x10
#define NRC_SERVICE_NOT_SUPPORTED  0x11
#define NRC_INVALID_FORMAT         0x13
#define NRC_CONDITIONS_NOT_CORRECT 0x22
#define NRC_REQUEST_SEQUENCE_ERROR 0x24
#define NRC_REQUEST_OUT_OF_RANGE   0x31
#define NRC_INVALID_KEY            0x35
#define NRC_PROGRAMMING_FAILURE    0x72
#define NRC_WRONG_BLOCK_SEQUENCE   0x73
#define NRC_RESPONSE_PENDING       0x78

// ----- Sessions -------------------------------------------------------------------------------------
#define SESSION_DEFAULT       0x01
#define SESSION_PROGRAMMING   0x02

// ----- DIDs -----------------------------------------------------------------------------------------
#define DID_PROTOCOL_VERSION  0xF180
#define DID_APP_VERSION       0xF181
#define DID_SERIAL_NUMBER     0xF18A
#define DID_CHIP_MAC          0xF190
#define DID_PARTITION_MAP     0xF1A0
#define DID_RUNNING_SLOT      0xF1B0
#define DID_CONFIG_BLOB       0x0100
#define DID_PROG_BUS_SELECT   0x0103

// ----- Routines -------------------------------------------------------------------------------------
#define RID_ERASE_INACTIVE    0xFF01
#define RID_VERIFY_IMAGE      0xFF02
#define RID_ACTIVATE_IMAGE    0xFF03
#define RID_SELF_TEST         0x0202

// Forward-declared types from the Arduino-ESP32 OTA API (kept opaque in the header).
struct esp_partition_t;

class DppEngine {
public:
  DppEngine();

  // Wire up the engine. `progBusSwitch` is a callback the engine calls when DID 0x0103 changes the
  // programming bus, so the host sketch can re-bind ISO-TP to the new controller. mitmApply is called
  // when the config blob changes so the live MITM mode/bus can be updated.
  void begin(Transports *tr,
             MCP2518FD *can1, MCP2518FD *can2,
             void (*progBusSwitch)(uint8_t bus),
             void (*configChanged)(const uint8_t *blob, uint16_t len));

  // Pump the transport(s); if a request arrived, dispatch it and emit the response. Non-blocking.
  void service();

  // Load persisted config (blob + programming bus) from NVS into RAM. Call once in setup().
  void loadPersisted();

  // Accessors used by the host sketch at boot.
  uint8_t  progBus()  const { return _progBus; }
  uint8_t  mitmMode() const { return _cfg[CFG_OFF_MITM_MODE]; }
  const uint8_t *configBlob() const { return _cfg; }

  // Confirm the running image is healthy (cancels OTA rollback). Call after a successful boot.
  static void markAppValid();

private:
  Transports *_tr;
  MCP2518FD  *_can1;
  MCP2518FD  *_can2;
  void (*_progBusSwitch)(uint8_t bus);
  void (*_configChanged)(const uint8_t *blob, uint16_t len);

  // ---- session / security state ----
  uint8_t  _session;          // SESSION_DEFAULT / SESSION_PROGRAMMING
  bool     _securityUnlocked;
  uint32_t _seed;             // last issued seed (security)
  uint8_t  _progBus;          // PROG_BUS_CAN1 / PROG_BUS_CAN2

  // ---- config blob (RAM mirror of NVS) ----
  uint8_t  _cfg[CFG_BLOB_LEN];

  // ---- OTA / download state ----
  bool             _downloadActive;
  uint32_t         _downloadSize;        // total bytes expected
  uint32_t         _downloadReceived;    // bytes written so far
  uint32_t         _downloadCrc32Decl;   // CRC32 declared in REQUEST_DOWNLOAD
  uint32_t         _downloadCrc32Calc;   // running CRC32 of what we wrote
  uint8_t          _expectedBsc;         // next expected block sequence counter (1..255 wrap 0)
  uint16_t         _maxBlock;            // negotiated max TRANSFER_DATA payload
  bool             _otaBegun;            // esp_ota_begin() succeeded (write in progress)
  uint32_t         _otaHandle;           // esp_ota_handle_t (kept as uint32 to keep header light)
  const void      *_otaPart;             // target esp_partition_t* (inactive OTA slot)
  bool             _otaStaged;           // esp_ota_end() validated the staged image OK

  // ---- response scratch ----
  uint8_t  _resp[8 + ISOTP_MAX_PDU];

  // ---- dispatch + per-service handlers (each returns the response length in _resp) ----
  uint16_t dispatch(const uint8_t *req, uint16_t len, TransportId src);

  uint16_t svcSession(const uint8_t *req, uint16_t len);
  uint16_t svcReset(const uint8_t *req, uint16_t len, TransportId src);
  uint16_t svcSecurity(const uint8_t *req, uint16_t len);
  uint16_t svcTesterPresent(const uint8_t *req, uint16_t len);
  uint16_t svcReadDid(const uint8_t *req, uint16_t len);
  uint16_t svcWriteDid(const uint8_t *req, uint16_t len);
  uint16_t svcReadMemory(const uint8_t *req, uint16_t len);
  uint16_t svcRoutine(const uint8_t *req, uint16_t len, TransportId src);
  uint16_t svcRequestDownload(const uint8_t *req, uint16_t len);
  uint16_t svcTransferData(const uint8_t *req, uint16_t len, TransportId src);
  uint16_t svcTransferExit(const uint8_t *req, uint16_t len);

  // ---- response builders ----
  uint16_t mkPositive(uint8_t sid, const uint8_t *data, uint16_t dlen);
  uint16_t mkNegative(uint8_t sid, uint8_t nrc);
  void     sendResponsePending(uint8_t sid, TransportId src);  // emit 0x78 immediately for long ops

  // ---- config blob helpers ----
  void     loadDefaultConfig();
  bool     validateConfig(const uint8_t *blob, uint16_t len);
  void     persistConfig();
  void     persistProgBus();

  // ---- DID read assembler ----
  uint16_t readDidPayload(uint16_t did, uint8_t *out, uint16_t maxOut, bool &ok);

  // ---- OTA helpers ----
  bool     otaBegin();                       // Update.begin() on the inactive OTA slot
  bool     otaWrite(const uint8_t *d, uint16_t n);
  bool     otaFinalize();                    // Update.end()
  bool     otaActivate();                    // set boot partition on staged image
  void     otaAbort();
  uint32_t slotSizeA();
  uint32_t slotSizeB();
  uint32_t appUsed();
};

#endif // TP_DPP_H
