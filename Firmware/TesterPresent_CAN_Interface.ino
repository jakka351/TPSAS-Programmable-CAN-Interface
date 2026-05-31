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
//  TesterPresent_CAN_Interface.ino  -  Top-level sketch for the TP-CAN-2I.
//    setup(): LEDs, both MCP2518FD @ 500k (FD optional via config), USB-CDC + UART1, lightweight
//             Wi-Fi/BLE bring-up, the MITM bridge, and the ISO-TP binding.
//    loop():  poll the transports -> DPP dispatch, run the MITM bridge, service LEDs + IGN, keep the
//             OTA rollback watchdog confirmed once we're up and healthy.
//
//  See README.md for the pin map, board settings, and the mapping to Docs/Device_Programming_Protocol.
// ----------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>

#include "config.h"
#include "mcp2518fd.h"
#include "isotp.h"
#include "transports.h"
#include "mitm.h"
#include "dpp.h"

// ====================================================================================================
//  Globals
// ====================================================================================================
SPIClass    g_spi(FSPI);                                  // shared FSPI bus to both MCP2518FD
MCP2518FD   g_can1(&g_spi, PIN_CAN1_CS, PIN_CAN1_INT);    // U7 = CAN1 (vehicle / upstream)
MCP2518FD   g_can2(&g_spi, PIN_CAN2_CS, PIN_CAN2_INT);    // U8 = CAN2 (ECU / downstream)

IsoTp       g_iso;
Transports  g_transports;
Mitm        g_mitm;
DppEngine   g_dpp;

static bool     s_appConfirmed = false;     // OTA rollback confirmation latch
static uint32_t s_bootMs       = 0;
static uint32_t s_statusBlinkMs = 0;
static bool     s_statusLed    = false;

// ====================================================================================================
//  Helpers to read bit-rates / FD flags from the live config blob
// ====================================================================================================
static uint32_t cfgBE32(const uint8_t *cfg, uint8_t off) {
  return ((uint32_t)cfg[off] << 24) | ((uint32_t)cfg[off + 1] << 16) |
         ((uint32_t)cfg[off + 2] << 8) | (uint32_t)cfg[off + 3];
}

// Bind ISO-TP to whichever controller currently carries the programming bus.
static void bindIsoToProgBus(uint8_t bus) {
  MCP2518FD *c = (bus == PROG_BUS_CAN2) ? &g_can2 : &g_can1;
  g_iso.bind(c, DPP_CAN_REQ_ID, DPP_CAN_RESP_ID);
  g_mitm.setProgBus(bus);
}

// Callback: DID 0x0103 changed the programming bus -> re-bind ISO-TP.
static void onProgBusSwitch(uint8_t bus) {
  bindIsoToProgBus(bus);
}

// Callback: config blob (DID 0x0100) changed -> live-apply MITM mode (bit-rate changes take effect on
// the next reset, which is the safe/standard behaviour for a CAN controller re-init).
static void onConfigChanged(const uint8_t *blob, uint16_t len) {
  (void)len;
  g_mitm.setMode(blob[CFG_OFF_MITM_MODE]);
}

// ====================================================================================================
//  LED bring-up
// ====================================================================================================
static void ledsInit() {
  pinMode(PIN_LED_WIFI,   OUTPUT);
  pinMode(PIN_LED_CAN1,   OUTPUT);
  pinMode(PIN_LED_CAN2,   OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_WIFI,   LED_OFF);
  digitalWrite(PIN_LED_CAN1,   LED_OFF);
  digitalWrite(PIN_LED_CAN2,   LED_OFF);
  digitalWrite(PIN_LED_STATUS, LED_OFF);
  // LED1 PWR (green) is hard-wired to 3V3 and needs no GPIO.
}

// ====================================================================================================
//  Wi-Fi / BLE - lightweight bring-up (kept off the hot path; only drives the Wi-Fi LED).
// ====================================================================================================
static void wirelessInit() {
#if WIFI_ENABLE
  // Bring up a quiet SoftAP for field config; the heavy lifting (telemetry/OTA-over-wifi) is left to
  // the application layer so the MITM path stays deterministic.
  WiFi.mode(WIFI_AP);
  char ap[24];
  snprintf(ap, sizeof(ap), "%s-%04X", TP_DEVICE_MODEL_STR, (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
  WiFi.softAP(ap);
  digitalWrite(PIN_LED_WIFI, LED_ON);
#else
  // Radios off by default for lowest quiescent current; Wi-Fi LED stays off.
  WiFi.mode(WIFI_OFF);
  digitalWrite(PIN_LED_WIFI, LED_OFF);
#endif
  // BLE bring-up intentionally deferred (BLE_ENABLE reserved for a future build) to keep flash/RAM
  // headroom for the dual OTA banks and MITM buffers.
}

// ====================================================================================================
//  CAN bring-up using the persisted config
// ====================================================================================================
static void canInit() {
  const uint8_t *cfg = g_dpp.configBlob();
  uint32_t br1  = cfgBE32(cfg, CFG_OFF_CAN1_BR);
  uint32_t br2  = cfgBE32(cfg, CFG_OFF_CAN2_BR);
  uint32_t dbr  = cfgBE32(cfg, CFG_OFF_FD_DATA_BR);
  uint8_t  fdEn = cfg[CFG_OFF_FD_ENABLE];
  if (br1 == 0) br1 = CAN_DEFAULT_NOMINAL_BITRATE;
  if (br2 == 0) br2 = CAN_DEFAULT_NOMINAL_BITRATE;
  if (dbr == 0) dbr = CAN_DEFAULT_DATA_BITRATE;

  bool fd1 = false, fd2 = false;
#if CANFD_ENABLE
  fd1 = (fdEn & 0x01) != 0;
  fd2 = (fdEn & 0x02) != 0;
#else
  (void)fdEn;   // FD compiled out -> classic CAN regardless of the blob
#endif

  // Shared FSPI bus. (SS = -1: we manage CS manually per controller.)
  g_spi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  bool ok1 = g_can1.begin(br1, dbr, fd1);
  bool ok2 = g_can2.begin(br2, dbr, fd2);

  // STATUS LED: solid if both controllers came up, else leave to the loop() error blink.
  if (ok1 && ok2) digitalWrite(PIN_LED_STATUS, LED_ON);
}

// ====================================================================================================
//  setup()
// ====================================================================================================
void setup() {
  s_bootMs = millis();

  ledsInit();

  // Ignition / wake sense as an input (no pulls; the divider sets the level).
  pinMode(PIN_IGN_SENSE, INPUT);

  // Bring up the transports (USB-CDC + UART1) early so a host can talk to us during init.
  g_transports.begin();
  g_transports.setIsoTp(&g_iso);

  // Load persisted config + programming-bus selection from NVS.
  g_dpp.loadPersisted();

  // Wireless (radios off by default).
  wirelessInit();

  // CAN controllers at the configured bit-rates.
  canInit();

  // MITM bridge + live mode from config.
  g_mitm.begin(&g_can1, &g_can2, &g_transports);
  g_mitm.setMode(g_dpp.mitmMode());

  // Bind ISO-TP to the persisted programming bus.
  bindIsoToProgBus(g_dpp.progBus());

  // Wire up the DPP engine (with the re-bind + config-apply callbacks).
  g_dpp.begin(&g_transports, &g_can1, &g_can2, onProgBusSwitch, onConfigChanged);
}

// ====================================================================================================
//  loop()
// ====================================================================================================
void loop() {
  // 1) MITM bridge: pump both CAN directions. Frames addressed to 0x7FE on the active programming bus
  //    are siphoned into ISO-TP here (not bridged).
  g_mitm.service();

  // 2) DPP: poll all transports (USB / UART / CAN-ISOTP) and dispatch one request if present.
  g_dpp.service();

  // 3) Confirm the running image healthy once we've been up briefly (cancels OTA rollback).
  if (!s_appConfirmed && (millis() - s_bootMs > 5000)) {
    DppEngine::markAppValid();
    s_appConfirmed = true;
  }

  // 4) STATUS heartbeat (blue): slow blink = alive & idle.
  uint32_t now = millis();
  if (now - s_statusBlinkMs > 1000) {
    s_statusBlinkMs = now;
    s_statusLed = !s_statusLed;
    digitalWrite(PIN_LED_STATUS, s_statusLed ? LED_ON : LED_OFF);
  }
}
