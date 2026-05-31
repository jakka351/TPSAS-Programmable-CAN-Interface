# TP-CAN-2I — ESP32-S3 Firmware (`TesterPresent_CAN_Interface`)

**Tester Present Specialist Automotive Solutions** · © 2026 Jack Leighton · Designed in Australia

Production Arduino firmware for the **TP-CAN-2I Programmable Dual-CAN Inline Interface** — a rugged
automotive CAN man-in-the-middle (MITM) gateway built on the ESP32-S3-WROOM-1-N16R8 with two
galvanically-isolated MCP2518FD CAN-FD channels. It bridges/filters/rewrites CAN traffic between a
vehicle harness (CAN1/upstream) and an ECU/module (CAN2/downstream), and is field-programmable over
**CAN (J2534 ISO-TP)**, **USB-C CDC**, and **UART** using the Tester Present Device Programming
Protocol (TP-DPP), defined in [`../../Docs/Device_Programming_Protocol.md`](../../Docs/Device_Programming_Protocol.md).

---

## 1. Files

| File | Role |
|---|---|
| `TesterPresent_CAN_Interface.ino` | `setup()`/`loop()`: init, MITM bridge, transport→DPP dispatch, LEDs, OTA rollback confirm |
| `config.h` | Pin map, CAN IDs (0x7FE/0x7FA), firmware version + build date, feature flags, default config blob layout |
| `dpp.h` / `dpp.cpp` | Transport-agnostic TP-DPP engine: all services, DIDs, routines, OTA (esp_ota), config blob (NVS), CRC16/CRC32 |
| `isotp.h` / `isotp.cpp` | Minimal ISO 15765-2 (ISO-TP) over one classic-CAN channel (RX 0x7FE / TX 0x7FA) |
| `mcp2518fd.h` / `mcp2518fd.cpp` | Compact self-contained MCP2518FD SPI driver (two instances, shared FSPI bus) |
| `transports.h` / `transports.cpp` | USB-CDC + UART1 `TP`-framing (CRC16-CCITT, magic-resync) + CAN/ISO-TP glue; uniform `getPdu()`/`sendPdu()` |
| `mitm.h` / `mitm.cpp` | CAN1↔CAN2 bridge with per-frame rule hook honouring `mitm_mode` (passthru/filter/rewrite/block) |

---

## 2. GPIO Pin Map (ESP32-S3-WROOM-1-N16R8)

Matches [`../../Hardware/Schematic_Design.md`](../../Hardware/Schematic_Design.md) (Block 6/8) and the
Engineering Specification §5. Strapping-safe choices; the N16R8 octal-PSRAM pins (GPIO26–37) are left
untouched.

| Function | GPIO | Dir | Connected to |
|---|---:|:--:|---|
| USB D− | 19 | — | native USB CDC (fixed by silicon) |
| USB D+ | 20 | — | native USB CDC (fixed by silicon) |
| SPI2/FSPI SCK | 12 | OUT | both MCP2518FD SCK |
| SPI2/FSPI MOSI (SDI) | 11 | OUT | both MCP2518FD SDI |
| SPI2/FSPI MISO (SDO) | 13 | IN | both MCP2518FD SDO |
| CAN1 CS (U7) | 10 | OUT | MCP2518FD #1 nCS (10k pull-up) |
| CAN2 CS (U8) | 14 | OUT | MCP2518FD #2 nCS (10k pull-up) |
| CAN1 INT (U7) | 4 | IN | MCP2518FD #1 nINT (active-low) |
| CAN2 INT (U8) | 5 | IN | MCP2518FD #2 nINT (active-low) |
| UART1 TX | 17 | OUT | DT pin 10 + service header J3 |
| UART1 RX | 18 | IN | DT pin 11 + service header J3 |
| IGN / WAKE sense | 16 | IN | R2/R3 divider off DT pin 3 (RTC-capable) |
| LED Wi-Fi (blue, LED2) | 38 | OUT | → 1k → LED → GND |
| LED CAN1 (amber, LED3) | 39 | OUT | → 1k → LED → GND |
| LED CAN2 (amber, LED4) | 40 | OUT | → 1k → LED → GND |
| LED STATUS (blue, LED5) | 41 | OUT | → 1k → LED → GND |
| LED PWR (green, LED1) | — | — | hard-wired 3V3 → 1k → LED → GND |
| BOOT (SW1) | 0 | IN | strapping; tactile to GND |
| RESET (SW2) | EN | — | chip EN; tactile to GND |

- **SPI:** 10 MHz SCK, MODE0. **Oscillator:** 40 MHz on CLKIN of U7; U7 CLKO daisy-chains U8.
- **Reserved — do not use:** GPIO 26–37 (flash + octal PSRAM). **Avoid as outputs:** 0, 3, 45, 46.

---

## 3. Library Dependencies

The firmware uses **only the Arduino-ESP32 core** — no third-party CAN library is required (the
MCP2518FD driver is self-contained in `mcp2518fd.cpp`). Core components used:

- `SPI` (FSPI bus to the controllers)
- `WiFi` (lightweight SoftAP bring-up; off by default)
- `Preferences` (NVS storage for the config blob + programming-bus select)
- `esp_ota_ops` / `esp_partition` / `esp_system` / `esp_mac` (OTA to the inactive slot, rollback,
  identity) — all bundled with the ESP-IDF inside the Arduino-ESP32 core.

### Install (Arduino IDE — Boards Manager)
1. **File → Preferences → Additional boards manager URLs**, add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. **Tools → Board → Boards Manager…**, install **“esp32” by Espressif Systems** (v3.0.0 or newer,
   which is ESP-IDF v5.x; the OTA/partition API names used here match that line).

### Install (arduino-cli)
```bash
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
# Compile (OTA-capable 16 MB partition scheme; see below):
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app5M_fat24M_16MB,PSRAM=opi" Firmware/TesterPresent_CAN_Interface
# Upload:
arduino-cli upload -p COMx --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app5M_fat24M_16MB,PSRAM=opi" Firmware/TesterPresent_CAN_Interface
```

> Any partition scheme that provides **two app/OTA slots + an NVS partition** works. The default
> `Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)` also has dual OTA but is too small for a 16 MB
> board; prefer a 16 MB OTA layout. If you need a larger single app, generate a custom `partitions.csv`
> with `ota_0`, `ota_1`, `otadata`, and `nvs` and select **“Custom”**.

---

## 4. Arduino IDE Build Settings (Tools menu)

| Setting | Value |
|---|---|
| **Board** | ESP32S3 Dev Module |
| **USB CDC On Boot** | **Enabled** (the configurator + DPP serial transport use the native CDC port) |
| **USB Mode** | Hardware CDC and JTAG |
| **Upload Mode** | UART0 / Hardware CDC |
| **Flash Size** | **16MB (128Mb)** |
| **PSRAM** | OPI PSRAM (N16R8 has 8 MB octal PSRAM) |
| **Partition Scheme** | An **OTA-capable** scheme with **two app slots + NVS** (e.g. *16M Flash (3MB APP/9.9MB FATFS)* or a custom dual-OTA 16 MB CSV) |
| **CPU Frequency** | 240 MHz |
| **Core Debug Level** | None (or Error) |

Manual flashing: hold **BOOT (SW1)**, tap **RST (SW2)**, release BOOT (native USB also supports
auto-reset). After the first successful boot the firmware calls `esp_ota_mark_app_valid_cancel_rollback()`
(~5 s in) so a freshly-flashed OTA image is confirmed and the bootloader will not roll it back.

---

## 5. Feature Flags (`config.h`)

| Macro | Default | Meaning |
|---|---|---|
| `DPP_SECURITY` | `0` | SECURITY_ACCESS (0x27) open: seed = 0, any key accepted. Set `1` for a real seed/key + to gate REQUEST_DOWNLOAD. |
| `CANFD_ENABLE` | `0` | Both channels classic CAN 2.0B @ 500k. Set `1` to honour the config blob’s FD-enable/data-bitrate. |
| `WIFI_ENABLE` | `0` | `0` = radios off (lowest quiescent). `1` = quiet SoftAP `TP-CAN-2I-XXXX` + Wi-Fi LED on. |
| `BLE_ENABLE` | `0` | Reserved for a future field-config build. |

---

## 6. Mapping to the Device Programming Protocol

The byte layouts in `dpp.cpp` follow [`../../Docs/Device_Programming_Protocol.md`](../../Docs/Device_Programming_Protocol.md)
exactly. Quick map:

| TP-DPP item | Where implemented |
|---|---|
| Serial framing `54 50 | LEN | PDU | CRC16` + magic-resync (§2.2/2.3) | `transports.cpp` `sendSerial()` / `pumpSerial()` |
| CAN ISO-TP RX 0x7FE / TX 0x7FA, FRAME_PAD 0xAA (§2.1) | `isotp.cpp` |
| Application PDU + positive/negative responses (§3) | `dpp.cpp` `mkPositive()` / `mkNegative()` |
| Services 0x10/0x11/0x22/0x23/0x27/0x2E/0x31/0x34/0x36/0x37/0x3E (§4) | `dpp.cpp` `svc*()` + `dispatch()` |
| DIDs 0xF180/F181/F18A/F190/F1A0/F1B0/0x0100/0x0103 (§4.1) | `dpp.cpp` `readDidPayload()` / `svcWriteDid()` |
| Routines 0xFF01 erase / 0xFF02 verify / 0xFF03 activate / 0x0202 self-test (§4.2) | `dpp.cpp` `svcRoutine()` |
| NRCs incl. 0x78 responsePending for long ops (§5) | `dpp.cpp` NRC macros + `sendResponsePending()` |
| Flashing sequence 1–9, OTA to inactive slot + CRC32 verify + activate + rollback (§6) | `dpp.cpp` `otaBegin/Write/Finalize/Activate` + `markAppValid()` |
| Timing P2 = 50 ms, P2* = 5000 ms, S3 = 2000 ms (§7) | `config.h` `DPP_P2_MS` / `DPP_P2_STAR_MS` / `DPP_S3_MS` |
| Config blob 35-byte layout + CRC16 (§8) | `config.h` `CFG_OFF_*` + `dpp.cpp` `loadDefaultConfig()` / `validateConfig()` |

**Programming bus.** ISO-TP listens on **CAN1** by default; writing DID `0x0103 = 1` switches it to
**CAN2** at runtime (persisted to NVS, ISO-TP re-bound immediately). The MITM bridge never forwards
the DPP request ID (0x7FE) across the barrier on the active programming bus.

**OTA safety.** Firmware is streamed to the **inactive** OTA slot via `esp_ota_write`, validated at
`esp_ota_end` (no boot change), CRC32-verified by routine 0xFF02 against the image on flash, and only
then made bootable by routine 0xFF03 (`esp_ota_set_boot_partition`). The first boot of a new image runs
in `PENDING_VERIFY`; if it never self-confirms it is rolled back automatically.

---

## 7. Runtime behaviour (LEDs)

| LED | Behaviour |
|---|---|
| PWR (green) | On whenever 3V3 is present (hard-wired). |
| Wi-Fi (blue) | On when SoftAP is up (`WIFI_ENABLE=1`), else off. |
| CAN1 (amber) | Blinks on vehicle-side bus traffic. |
| CAN2 (amber) | Blinks on ECU-side bus traffic. |
| STATUS (blue) | Solid after both controllers init OK; ~1 Hz heartbeat while idle/alive. |
