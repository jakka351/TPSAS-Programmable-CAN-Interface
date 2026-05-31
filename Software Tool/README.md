<p align="right"><a href="https://testerpresent.com.au">
<img align="right" src="https://github.com/user-attachments/assets/b5a91787-4dfc-4ea2-94ae-2bb878f5af6c" height="30%" width="30%"/>
</a></p>


# TP-CAN-2I Configurator

Windows PC configurator for the **TP-CAN-2I Programmable Dual-CAN Inline Interface**
by *Tester Present Specialist Automotive Solutions*.

It reads, writes, backs up, and flashes the device's firmware/config over **three
transports**, all speaking the same transport-agnostic **TP-DPP v1.0** application
protocol (`Docs/Device_Programming_Protocol.md`):

<img width="1163" height="753" alt="image" src="https://github.com/user-attachments/assets/f39820d9-7286-4f6f-bc12-dff9b868b1ba" />


| Transport | How |
|---|---|
| **J2534 (CAN)** | SAE J2534 PassThru, ISO 15765-2 (ISO-TP) @ 500 kbit/s, tool-TX `0x7FE` / device-RX `0x7FA`. Reuses the existing `J2534` stack. |
| **Serial UART** | COM port @ 921600 8N1, framed `54 50 | LEN(2 BE) | PDU | CRC16-CCITT(2 BE)` with magic resync. |
| **USB-C** | The ESP32-S3 native USB CDC virtual COM port — same serial framing. |

## Features

- **Firmware editor** — IDE-style syntax-highlighting code editor (C/C++/Arduino),
  Consolas font, light theme, line-number gutter, debounced re-highlighting. New /
  Open / Save and a **Build (.bin)** button that shells out to `arduino-cli`.
- **Flash tab** — load a `.bin` or Intel `.hex`, **Read / Backup from device**, and
  **Write + Verify + Activate** with a live progress bar + log, following the §6
  canonical flashing sequence (session → erase → request-download → transfer loop →
  transfer-exit → CRC verify → activate → reset).
- **Console tab** — live transport log + CAN/serial frame traffic.
- **Connection panel** — transport / device (J2534 devices or COM ports) / baud,
  Connect / Disconnect, and a read-only identity readout (app version, serial,
  partition map, running OTA slot).

## Build

Requires the **.NET Framework 4.8.1** targeting pack (the .NET SDK 9 toolchain on
this machine already has it). From the repository root:

```
dotnet build Software/TPCANConfigurator/TPCANConfigurator.csproj -c Release
```

The output `TPCANConfigurator.exe` lands in
`Software/TPCANConfigurator/bin/Release/net481/`.

### Opening in Visual Studio

Open `Software/TPCANConfigurator/TPCANConfigurator.csproj` directly (it's an
SDK-style project). The entire UI is built in code — there is **no** `.Designer.cs`
and **no** `.resx`, so the WinForms designer is intentionally not used (this avoids
designer/resx build pitfalls). Edit `MainForm.cs` and run.

## Project layout

```
TPCANConfigurator/
  TPCANConfigurator.csproj   SDK-style net481 WinForms; includes ..\J2534\**\*.cs
  Program.cs                 [STAThread] entry point
  MainForm.cs                Whole UI (menu, toolbar, panels, tabs, status bar)
  Dpp/
    ITransport.cs            Transport abstraction
    J2534Transport.cs        CAN via J2534Manager (0x7FE/0x7FA)
    SerialTransport.cs       Serial + USB-C (54 50 | LEN | PDU | CRC16 framing)
    DppClient.cs             All TP-DPP services, DIDs, routines, §6 flash flow
    Crc.cs                   CRC-16/CCITT-FALSE + CRC-32/IEEE
  Fw/
    IntelHex.cs              Intel .hex -> byte[]
    FirmwareImage.cs         .bin / .hex loader (bytes + CRC32 + size)
  Editor/
    CodeEditor.cs            RichTextBox + gutter syntax editor
  Compat/
    ELM327Transport.cs       Compile-time stub for a J2534-stack dependency (see note)
```

## Transport notes

- **CAN/J2534**: the adapter does ISO-TP segmentation/reassembly, so the PC side is
  just `J2534Manager.SendReceive(0x7FE, pdu)`. Pick a J2534 device in the Connection
  panel (enumerated from the registry via `J2534DeviceFinder`).
- **Serial / USB-C**: pick a COM port. Baud defaults to 921600 (ignored by USB-CDC,
  honoured by the UART header). The framing layer handles CRC validation and magic
  resync automatically.
- **responsePending (NRC 0x78)**: long operations (erase / write / verify) are
  handled by waiting up to **P2\*** (default 5000 ms, refined from the session
  control response).

## arduino-cli requirement (Build feature)

The editor's **Build (.bin)** button compiles the open sketch by shelling out to
[`arduino-cli`](https://arduino.github.io/arduino-cli/). It is **not bundled**. If it
isn't found on `PATH` (or at the path entered in the *arduino-cli:* box), the app
shows install guidance instead of failing silently:

1. Download `arduino-cli` and put `arduino-cli.exe` on your `PATH`.
2. Install the ESP32 core: `arduino-cli core install esp32:esp32`.
3. Click **Build** — output `.bin` lands in the sketch's `build/` folder
   (`--fqbn esp32:esp32:esp32s3`).

## Note on the J2534 stack

The reused `Software/J2534/*` sources reference an `ELM327Transport` class whose
definition was **not** shipped with those three files. To build without modifying the
read-only J2534 sources, a minimal compile-time stub lives in
`Compat/ELM327Transport.cs`. The Configurator never selects an ELM327 adapter (it
uses J2534 PassThru / Serial / USB-C), so that code path is never exercised.

---

© 2026 Jack Leighton — Designed in Australia
