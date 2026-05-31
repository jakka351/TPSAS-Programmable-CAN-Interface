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
//  mitm.cpp  -  The bridge. The ruleHook() is the single place an integrator customises filtering /
//               rewriting. Defaults: passthru forwards everything; filter drops a small example block-
//               list; rewrite shows an in-place edit example; block drops all bridged traffic. The DPP
//               programming frames (0x7FE/0x7FA) are never bridged.
// ----------------------------------------------------------------------------------------------------

#include "mitm.h"

#define LED_BLINK_MS 30   // activity LED on-time per frame burst

Mitm::Mitm()
  : _can1(nullptr), _can2(nullptr), _tr(nullptr),
    _mode(MITM_MODE_PASSTHRU), _progBus(PROG_BUS_CAN1),
    _led1Until(0), _led2Until(0) {}

void Mitm::begin(MCP2518FD *can1, MCP2518FD *can2, Transports *tr) {
  _can1 = can1;
  _can2 = can2;
  _tr = tr;
}

// ====================================================================================================
//  Per-frame rule hook.  EDIT ME to implement application filtering / rewriting.
//  Returns MITM_PASS / MITM_REWRITE / MITM_DROP. `f` may be modified in place when rewriting.
// ====================================================================================================
MitmAction Mitm::ruleHook(CanFrame &f, bool fromCan1) {
  switch (_mode) {
    case MITM_MODE_PASSTHRU:
      return MITM_PASS;

    case MITM_MODE_FILTER: {
      // Example allow/deny: drop a couple of well-known noisy/diagnostic IDs. Replace with your map.
      if (f.id == 0x7DF || f.id == 0x7E0) return MITM_DROP;   // OBD functional/physical request demo
      return MITM_PASS;
    }

    case MITM_MODE_REWRITE: {
      // Example rewrite: clamp byte 0 of a specific ID (e.g. spoof a sensor). Replace with your logic.
      if (f.id == 0x3B0 && f.len >= 1) {
        f.data[0] = 0x00;
        return MITM_REWRITE;
      }
      return MITM_PASS;
    }

    case MITM_MODE_BLOCK:
    default:
      return MITM_DROP;   // full cut (monitor-only / safe state)
  }
}

// ====================================================================================================
//  Bridge one direction.
// ====================================================================================================
bool Mitm::bridgeOne(MCP2518FD *src, MCP2518FD *dst, bool fromCan1, uint8_t srcBusId,
                     uint8_t ledPin, uint32_t &ledUntil) {
  if (!src || !dst) return false;
  if (!src->available()) return false;

  CanFrame f;
  if (!src->readFrame(f)) return false;

  // Activity indication.
  ledUntil = millis() + LED_BLINK_MS;

  // If this frame is DPP programming traffic on the ACTIVE programming bus, hand it to ISO-TP and do
  // NOT bridge it across to the other side.
  if (_tr && srcBusId == _progBus && f.id == DPP_CAN_REQ_ID) {
    _tr->feedCanFrame(f);
    return true;
  }

  // Apply the rule hook.
  MitmAction act = ruleHook(f, fromCan1);
  if (act == MITM_DROP) return true;        // consumed, not forwarded

  // Forward (PASS or REWRITE both transmit the current contents of f).
  dst->sendFrame(f);
  return true;
}

// ====================================================================================================
//  Service both directions + drive activity LEDs.
// ====================================================================================================
void Mitm::service() {
  // CAN1 -> CAN2  (vehicle to ECU)
  bridgeOne(_can1, _can2, /*fromCan1=*/true,  PROG_BUS_CAN1, PIN_LED_CAN1, _led1Until);
  // CAN2 -> CAN1  (ECU to vehicle)
  bridgeOne(_can2, _can1, /*fromCan1=*/false, PROG_BUS_CAN2, PIN_LED_CAN2, _led2Until);

  // Update activity LEDs (one-shot decay).
  uint32_t now = millis();
  digitalWrite(PIN_LED_CAN1, (now < _led1Until) ? LED_ON : LED_OFF);
  digitalWrite(PIN_LED_CAN2, (now < _led2Until) ? LED_ON : LED_OFF);
}
