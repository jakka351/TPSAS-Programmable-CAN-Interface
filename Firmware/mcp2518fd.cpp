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
//  mcp2518fd.cpp  -  MCP2518FD register-level driver. Register addresses + instruction encodings per
//                    the Microchip MCP2517FD/MCP2518FD External CAN FD Controller datasheet
//                    (DS20005688). 40 MHz CLKIN -> SYSCLK 40 MHz (PLL bypassed). One TX FIFO (FIFO1)
//                    and one RX FIFO (FIFO2) in the 2 KB controller RAM.
// ----------------------------------------------------------------------------------------------------

#include "mcp2518fd.h"

// ---- SPI instructions (4-bit opcode in the top nibble, 12-bit address) -----------------------------
#define CMD_RESET        0x00
#define CMD_READ         0x03   // 0b0011
#define CMD_WRITE        0x02   // 0b0010

// ---- SFR register addresses (C1 = CAN module instance) ---------------------------------------------
#define REG_OSC          0xE00  // oscillator control
#define REG_IOCON        0xE04  // I/O control
#define REG_C1CON        0x000  // CAN control
#define REG_C1NBTCFG     0x004  // nominal bit-time config
#define REG_C1DBTCFG     0x008  // data bit-time config
#define REG_C1TDC        0x00C  // transmitter delay compensation
#define REG_C1TBC        0x010  // time base counter
#define REG_C1TXQCON     0x050  // TXQ control (unused here)
#define REG_C1FIFOCON1   0x05C  // FIFO 1 control  (TX)
#define REG_C1FIFOSTA1   0x060  // FIFO 1 status
#define REG_C1FIFOUA1    0x064  // FIFO 1 user address
#define REG_C1FIFOCON2   0x068  // FIFO 2 control  (RX)
#define REG_C1FIFOSTA2   0x06C  // FIFO 2 status
#define REG_C1FIFOUA2    0x070  // FIFO 2 user address
#define REG_C1FLTCON0    0x1D0  // filter control 0 (FLT0..3)
#define REG_C1FLTOBJ0    0x1F0  // filter object 0
#define REG_C1MASK0      0x1F4  // mask 0

// Message-object RAM region begins at 0x400 (relative to controller base 0x000).
#define RAM_BASE         0x400

// ---- OSC bits --------------------------------------------------------------------------------------
#define OSC_PLLEN        (1u << 0)
#define OSC_OSCDIS       (1u << 2)
#define OSC_SCLKDIV      (1u << 4)
#define OSC_CLKODIV_10   (0u << 5)   // CLKO = SYSCLK/1 (00); we daisy-chain CLKO to U8
#define OSC_PLLRDY       (1u << 8)
#define OSC_OSCRDY       (1u << 10)

// ---- C1CON fields ----------------------------------------------------------------------------------
#define C1CON_REQOP_SHIFT   24
#define C1CON_OPMOD_SHIFT   21
#define C1CON_OPMOD_MASK    0x7u
#define C1CON_TXQEN         (1u << 20)  // we disable TXQ (use FIFOs only)
#define C1CON_STEF          (1u << 19)  // store-in-TEF disabled
#define C1CON_ISOCRCEN      (1u << 5)   // ISO CRC (FD) enable

// ---- FIFOCON fields --------------------------------------------------------------------------------
#define FIFOCON_TXEN     (1u << 7)   // this FIFO is a TX FIFO
#define FIFOCON_UINC     (1u << 8)   // increment head/tail
#define FIFOCON_TXREQ    (1u << 9)   // request transmit
#define FIFOCON_FRESET   (1u << 10)  // FIFO reset
#define FIFOSTA_TFNRFNIF (1u << 0)   // TX not full / RX not empty
#define FIFOSTA_TFERFFIF (1u << 1)   // TX empty / RX full

MCP2518FD::MCP2518FD(SPIClass *spi, uint8_t csPin, uint8_t intPin)
  : _spi(spi), _cs(csPin), _int(intPin), _fd(false),
    _spiset(MCP2518FD_SPI_HZ, MSBFIRST, SPI_MODE0) {}

// ====================================================================================================
//  Low-level SPI access
// ====================================================================================================
void MCP2518FD::reset() {
  _spi->beginTransaction(_spiset);
  csLow();
  _spi->transfer(CMD_RESET);   // opcode 0x0, addr 0x000 -> two zero bytes
  _spi->transfer(0x00);
  csHigh();
  _spi->endTransaction();
  delay(2);
}

uint8_t MCP2518FD::readByte(uint16_t addr) {
  _spi->beginTransaction(_spiset);
  csLow();
  _spi->transfer((CMD_READ << 4) | ((addr >> 8) & 0x0F));
  _spi->transfer(addr & 0xFF);
  uint8_t v = _spi->transfer(0x00);
  csHigh();
  _spi->endTransaction();
  return v;
}

void MCP2518FD::writeByte(uint16_t addr, uint8_t val) {
  _spi->beginTransaction(_spiset);
  csLow();
  _spi->transfer((CMD_WRITE << 4) | ((addr >> 8) & 0x0F));
  _spi->transfer(addr & 0xFF);
  _spi->transfer(val);
  csHigh();
  _spi->endTransaction();
}

uint32_t MCP2518FD::readWord(uint16_t addr) {
  uint8_t b[4];
  readBuffer(addr, b, 4);
  return ((uint32_t)b[0]) | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void MCP2518FD::writeWord(uint16_t addr, uint32_t val) {
  uint8_t b[4];
  b[0] = val & 0xFF;
  b[1] = (val >> 8) & 0xFF;
  b[2] = (val >> 16) & 0xFF;
  b[3] = (val >> 24) & 0xFF;
  writeBuffer(addr, b, 4);
}

void MCP2518FD::readBuffer(uint16_t addr, uint8_t *buf, uint16_t len) {
  _spi->beginTransaction(_spiset);
  csLow();
  _spi->transfer((CMD_READ << 4) | ((addr >> 8) & 0x0F));
  _spi->transfer(addr & 0xFF);
  for (uint16_t i = 0; i < len; i++) buf[i] = _spi->transfer(0x00);
  csHigh();
  _spi->endTransaction();
}

void MCP2518FD::writeBuffer(uint16_t addr, const uint8_t *buf, uint16_t len) {
  _spi->beginTransaction(_spiset);
  csLow();
  _spi->transfer((CMD_WRITE << 4) | ((addr >> 8) & 0x0F));
  _spi->transfer(addr & 0xFF);
  for (uint16_t i = 0; i < len; i++) _spi->transfer(buf[i]);
  csHigh();
  _spi->endTransaction();
}

// ====================================================================================================
//  DLC <-> byte-count (FD)
// ====================================================================================================
uint8_t MCP2518FD::lenToDlc(uint8_t len) {
  if (len <= 8)  return len;
  if (len <= 12) return 9;
  if (len <= 16) return 10;
  if (len <= 20) return 11;
  if (len <= 24) return 12;
  if (len <= 32) return 13;
  if (len <= 48) return 14;
  return 15; // up to 64
}

uint8_t MCP2518FD::dlcToLen(uint8_t dlc) {
  static const uint8_t tbl[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
  return tbl[dlc & 0x0F];
}

// ====================================================================================================
//  Bit timing.  SYSCLK = 40 MHz (CLKIN direct, PLL off).
//  Nominal (500k):  Tq = SYSCLK/(BRP+1).  Total Tq/bit = SYSCLK/bitrate.
//  We pick BRP=0 (40 MHz Tq), so 80 Tq/bit at 500k -> SJW/TSEG split below.
// ====================================================================================================
bool MCP2518FD::configureBitTiming(uint32_t nominalBitrate, uint32_t dataBitrate, bool fdMode) {
  // ----- Nominal bit time (C1NBTCFG): BRP[31:24] SJW[6:0] TSEG2[14:8] TSEG1[23:16] -----
  // With BRP=0, total Tq = 40e6 / nominalBitrate. Layout SYNC(1)+TSEG1+TSEG2 = totalTq.
  uint32_t totalTq = MCP2518FD_OSC_HZ / nominalBitrate;     // e.g. 80 @ 500k
  if (totalTq < 8 || totalTq > 385) return false;            // sanity (controller limits)
  uint32_t tseg2 = totalTq / 5;                              // ~20% -> sample point ~80%
  if (tseg2 < 2) tseg2 = 2;
  uint32_t tseg1 = totalTq - tseg2 - 1;                      // remove SYNC segment
  uint32_t sjw   = (tseg2 < 4) ? tseg2 : 4;

  uint32_t nbt = 0;
  nbt |= ((uint32_t)(0)        & 0xFFu) << 24;   // BRP   = 0  (Tq = 1 SYSCLK)
  nbt |= ((uint32_t)(tseg1 - 1) & 0xFFu) << 16;  // TSEG1 (encoded N-1)
  nbt |= ((uint32_t)(tseg2 - 1) & 0x7Fu) << 8;   // TSEG2 (encoded N-1)
  nbt |= ((uint32_t)(sjw   - 1) & 0x7Fu) << 0;   // SJW   (encoded N-1)
  writeWord(REG_C1NBTCFG, nbt);

  // ----- Data bit time (C1DBTCFG) for FD: only matters when BRS frames are sent -----
  if (fdMode) {
    uint32_t dTotal = MCP2518FD_OSC_HZ / dataBitrate;        // e.g. 20 @ 2 Mbit
    if (dTotal < 4) dTotal = 4;
    uint32_t dtseg2 = dTotal / 5; if (dtseg2 < 1) dtseg2 = 1;
    uint32_t dtseg1 = dTotal - dtseg2 - 1;
    uint32_t dsjw   = (dtseg2 < 4) ? dtseg2 : 4;
    uint32_t dbt = 0;
    dbt |= ((uint32_t)(0)         & 0xFFu) << 24;  // BRP = 0
    dbt |= ((uint32_t)(dtseg1 - 1) & 0x1Fu) << 16; // DTSEG1
    dbt |= ((uint32_t)(dtseg2 - 1) & 0x0Fu) << 8;  // DTSEG2
    dbt |= ((uint32_t)(dsjw   - 1) & 0x0Fu) << 0;  // DSJW
    writeWord(REG_C1DBTCFG, dbt);

    // Transmitter Delay Compensation: auto mode, TDCO = DTSEG1 (typical).
    uint32_t tdc = 0;
    tdc |= (2u << 16);                       // TDCMOD = auto
    tdc |= (((dtseg1 - 1) & 0x3Fu) << 8);    // TDCO
    writeWord(REG_C1TDC, tdc);
  }
  return true;
}

// ====================================================================================================
//  FIFO config.  FIFO1 = TX (8 messages), FIFO2 = RX (16 messages).  Payload size 64 bytes so the
//  same RAM layout serves classic and FD frames.
// ====================================================================================================
void MCP2518FD::configureFifos() {
  // PLSIZE field encodes payload bytes: 7 = 64 bytes. FSIZE = (n-1).
  const uint32_t PLSIZE_64 = 7u;

  // ---- FIFO1 : transmit ----
  uint32_t f1 = 0;
  f1 |= FIFOCON_TXEN;                 // TX FIFO
  f1 |= (PLSIZE_64 << 29);            // payload 64 bytes
  f1 |= ((8u - 1u) << 24);           // FSIZE = 8 messages
  // TXAT (retransmit attempts) unlimited, TXPRI default.
  writeWord(REG_C1FIFOCON1, f1);

  // ---- FIFO2 : receive ----
  uint32_t f2 = 0;
  // TXEN = 0 -> RX FIFO
  f2 |= (PLSIZE_64 << 29);            // payload 64 bytes
  f2 |= ((16u - 1u) << 24);          // FSIZE = 16 messages
  f2 |= (1u << 0);                   // RXTSEN: timestamp (harmless); RFNEIE not needed (polled)
  writeWord(REG_C1FIFOCON2, f2);
}

// ====================================================================================================
//  Accept-all filter on FIFO2.  (Higher-level ISO-TP / MITM do ID matching in software.)
// ====================================================================================================
void MCP2518FD::configureFilters() {
  // Disable filter 0 before editing.
  writeByte(REG_C1FLTCON0, 0x00);

  // Object 0: don't care (mask all zero = accept everything).
  writeWord(REG_C1FLTOBJ0, 0x00000000);  // SID/EID = 0
  writeWord(REG_C1MASK0,   0x00000000);  // mask 0 -> match any ID, std + ext

  // Point filter 0 at FIFO2 (RX) and enable it. FLTEN0 = bit7, F0BP = FIFO index in low bits.
  writeByte(REG_C1FLTCON0, 0x80 | 0x02); // enable, buffer pointer = FIFO2
}

// ====================================================================================================
//  Mode change with readback
// ====================================================================================================
bool MCP2518FD::setMode(uint8_t mode) {
  uint32_t con = readWord(REG_C1CON);
  con &= ~((uint32_t)0x7 << C1CON_REQOP_SHIFT);
  con |=  ((uint32_t)(mode & 0x7) << C1CON_REQOP_SHIFT);
  writeWord(REG_C1CON, con);

  for (int i = 0; i < 50; i++) {
    uint32_t c = readWord(REG_C1CON);
    if (((c >> C1CON_OPMOD_SHIFT) & C1CON_OPMOD_MASK) == (mode & 0x7)) return true;
    delay(1);
  }
  return false;
}

// ====================================================================================================
//  Bring-up
// ====================================================================================================
bool MCP2518FD::begin(uint32_t nominalBitrate, uint32_t dataBitrate, bool fdMode) {
  _fd = fdMode;

  pinMode(_cs, OUTPUT);
  csHigh();
  pinMode(_int, INPUT_PULLUP);

  reset();

  // ----- Oscillator: 40 MHz CLKIN direct (PLL OFF), SCLKDIV=0, CLKO = SYSCLK -----
  // OSC = 0 -> PLL disabled, oscillator enabled, divide-by-1.
  writeWord(REG_OSC, 0x00000000);
  bool oscOk = false;
  for (int i = 0; i < 100; i++) {
    uint32_t osc = readWord(REG_OSC);
    if (osc & OSC_OSCRDY) { oscOk = true; break; }
    delay(1);
  }
  if (!oscOk) return false;

  // ----- Enter configuration mode before touching timing / FIFOs -----
  if (!setMode(MODE_CONFIG)) return false;

  if (!configureBitTiming(nominalBitrate, dataBitrate, fdMode)) return false;
  configureFifos();
  configureFilters();

  // ----- C1CON: enable ISO CRC for FD, disable TXQ/TEF, keep RTXAT default -----
  uint32_t con = readWord(REG_C1CON);
  con &= ~C1CON_TXQEN;          // no TX queue (FIFOs only)
  con &= ~C1CON_STEF;           // no TEF
  if (fdMode) con |= C1CON_ISOCRCEN;
  writeWord(REG_C1CON, con);

  // ----- Go operational -----
  uint8_t runMode = fdMode ? MODE_NORMAL_FD : MODE_NORMAL_CLASSIC;
  return setMode(runMode);
}

// ====================================================================================================
//  Transmit one frame (load object into FIFO1 RAM, set UINC + TXREQ)
// ====================================================================================================
bool MCP2518FD::sendFrame(const CanFrame &f) {
  // Check FIFO1 not full.
  uint32_t sta = readWord(REG_C1FIFOSTA1);
  if (!(sta & FIFOSTA_TFNRFNIF)) return false;   // TFNRFNIF=0 -> full

  // Get the RAM address of the next free TX object.
  uint16_t ua = (uint16_t)(readWord(REG_C1FIFOUA1) & 0xFFFF);
  uint16_t objAddr = RAM_BASE + ua;

  // Build the 8-byte object header (T0 = ID, T1 = control).
  uint8_t hdr[8];
  uint32_t t0 = 0, t1 = 0;

  if (f.extended) {
    // SID[10:0] in [10:0], EID[17:0] in [28:11].
    uint32_t id = f.id & 0x1FFFFFFFu;
    uint32_t sid = (id >> 18) & 0x7FF;
    uint32_t eid = id & 0x3FFFF;
    t0 = (sid) | (eid << 11);
    t1 |= (1u << 4);   // IDE
  } else {
    t0 = f.id & 0x7FF;
  }

  uint8_t dlc = lenToDlc(f.len);
  t1 |= (dlc & 0x0F);
  if (f.fd)  t1 |= (1u << 7);   // FDF
  if (f.brs) t1 |= (1u << 6);   // BRS

  hdr[0] = t0 & 0xFF;  hdr[1] = (t0 >> 8) & 0xFF;  hdr[2] = (t0 >> 16) & 0xFF;  hdr[3] = (t0 >> 24) & 0xFF;
  hdr[4] = t1 & 0xFF;  hdr[5] = (t1 >> 8) & 0xFF;  hdr[6] = (t1 >> 16) & 0xFF;  hdr[7] = (t1 >> 24) & 0xFF;

  writeBuffer(objAddr, hdr, 8);

  // Payload (pad to the DLC byte count; CAN-FD frames must write the full slot length).
  uint8_t n = dlcToLen(dlc);
  uint8_t payload[64];
  memset(payload, 0, sizeof(payload));
  memcpy(payload, f.data, (f.len <= 64) ? f.len : 64);
  // Round up to 4-byte word boundary for the RAM write.
  uint8_t wlen = (n + 3) & ~0x03;
  if (wlen == 0) wlen = 4;
  writeBuffer(objAddr + 8, payload, wlen);

  // Set UINC then TXREQ on FIFO1.
  uint32_t con = readWord(REG_C1FIFOCON1);
  con |= FIFOCON_UINC;
  writeWord(REG_C1FIFOCON1, con);
  con |= FIFOCON_TXREQ;
  writeWord(REG_C1FIFOCON1, con);
  return true;
}

// ====================================================================================================
//  Receive one frame (read object out of FIFO2 RAM, set UINC)
// ====================================================================================================
bool MCP2518FD::available() {
  uint32_t sta = readWord(REG_C1FIFOSTA2);
  return (sta & FIFOSTA_TFNRFNIF) != 0;   // RX not empty
}

bool MCP2518FD::readFrame(CanFrame &f) {
  uint32_t sta = readWord(REG_C1FIFOSTA2);
  if (!(sta & FIFOSTA_TFNRFNIF)) return false;   // empty

  uint16_t ua = (uint16_t)(readWord(REG_C1FIFOUA2) & 0xFFFF);
  uint16_t objAddr = RAM_BASE + ua;

  uint8_t hdr[8];
  readBuffer(objAddr, hdr, 8);
  uint32_t r0 = ((uint32_t)hdr[0]) | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
  uint32_t r1 = ((uint32_t)hdr[4]) | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);

  f.extended = (r1 & (1u << 4)) != 0;
  if (f.extended) {
    uint32_t sid = r0 & 0x7FF;
    uint32_t eid = (r0 >> 11) & 0x3FFFF;
    f.id = (sid << 18) | eid;
  } else {
    f.id = r0 & 0x7FF;
  }
  f.fd  = (r1 & (1u << 7)) != 0;
  f.brs = (r1 & (1u << 6)) != 0;
  uint8_t dlc = r1 & 0x0F;
  f.len = dlcToLen(dlc);
  if (f.len > 64) f.len = 64;

  uint8_t wlen = (f.len + 3) & ~0x03;
  if (wlen == 0) wlen = 4;
  uint8_t payload[64];
  readBuffer(objAddr + 8, payload, wlen);
  memcpy(f.data, payload, f.len);

  // Advance the RX FIFO tail.
  uint32_t con = readWord(REG_C1FIFOCON2);
  con |= FIFOCON_UINC;
  writeWord(REG_C1FIFOCON2, con);
  return true;
}

bool MCP2518FD::interruptAsserted() {
  return digitalRead(_int) == LOW;   // nINT is active-low
}

// ====================================================================================================
//  Internal-loopback self-test (used by routine 0x0202).  Switches to internal loopback, sends a
//  known frame, confirms it round-trips, then restores normal mode.
// ====================================================================================================
bool MCP2518FD::loopbackSelfTest() {
  uint8_t restore = _fd ? MODE_NORMAL_FD : MODE_NORMAL_CLASSIC;
  if (!setMode(MODE_INTERNAL_LOOP)) return false;

  CanFrame tx;
  memset(&tx, 0, sizeof(tx));
  tx.id = 0x123;
  tx.len = 4;
  tx.data[0] = 0xDE; tx.data[1] = 0xAD; tx.data[2] = 0xBE; tx.data[3] = 0xEF;
  bool ok = false;
  if (sendFrame(tx)) {
    for (int i = 0; i < 50; i++) {
      if (available()) {
        CanFrame rx;
        if (readFrame(rx) && rx.id == 0x123 && rx.len == 4 &&
            rx.data[0] == 0xDE && rx.data[3] == 0xEF) { ok = true; }
        break;
      }
      delay(1);
    }
  }
  setMode(restore);
  return ok;
}
