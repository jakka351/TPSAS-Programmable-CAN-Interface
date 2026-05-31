// Tester Present Specialist Automotive Solutions — TP-CAN-2I Configurator — © 2026 Jack Leighton — Designed in Australia
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using J2534;
using TesterPresent.Configurator.Dpp;
using TesterPresent.Configurator.Editor;
using TesterPresent.Configurator.Fw;

namespace TesterPresent.Configurator
{
    /// <summary>
    /// Main window for the TP-CAN-2I Configurator. Professional engineering-tool
    /// layout built entirely in code (no .Designer.cs / .resx): MenuStrip + ToolStrip,
    /// a docked left Connection panel (transport / device / baud / connect), a centre
    /// TabControl (Firmware Editor, Flash, Console), and a bottom StatusStrip.
    /// </summary>
    public sealed class MainForm : Form
    {
        private enum TransportKind { J2534, Serial, UsbC }

        // ---- Transport / protocol ----
        private ITransport _transport;
        private DppClient _dpp;

        // ---- Connection panel controls ----
        private ComboBox _cboTransport;
        private ComboBox _cboDevice;
        private ComboBox _cboBaud;
        private Button _btnConnect;
        private Button _btnDisconnect;
        private Button _btnRefresh;
        private TextBox _txtIdentity;
        private Label _lblDeviceCaption;
        private Label _lblBaudCaption;

        // ---- Editor tab ----
        private CodeEditor _editor;
        private string _currentSourcePath;
        private TextBox _txtArduinoCli;

        // ---- Flash tab ----
        private TextBox _txtImagePath;
        private Label _lblImageInfo;
        private ProgressBar _progress;
        private TextBox _txtFlashLog;
        private Button _btnFlashWrite;
        private Button _btnFlashRead;
        private FirmwareImage _loadedImage;

        // ---- Console tab ----
        private TextBox _txtConsole;

        // ---- Status bar ----
        private StatusStrip _status;
        private ToolStripStatusLabel _statusConn;
        private ToolStripStatusLabel _statusVersion;

        private static readonly int[] BaudRates = { 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 };

        public MainForm()
        {
            Text = "Tester Present — TP-CAN-2I Configurator";
            StartPosition = FormStartPosition.CenterScreen;
            Width = 1180;
            Height = 760;
            MinimumSize = new Size(940, 600);
            Font = new Font("Segoe UI", 9f);

            // Add the Fill control (tabs) FIRST so it sits at the back of the
            // z-order; docked Top/Bottom/Left controls added afterwards each claim
            // their edge from the remaining space and the tabs fill the centre.
            BuildTabs();
            BuildConnectionPanel();
            BuildStatusBar();
            BuildToolStrip();
            BuildMenu();

            RefreshDevices();
            UpdateConnectionState(false);
        }

        // =====================================================================
        //  UI construction
        // =====================================================================

        private void BuildMenu()
        {
            var menu = new MenuStrip();

            var file = new ToolStripMenuItem("&File");
            file.DropDownItems.Add("&New firmware source", null, (s, e) => NewSource());
            file.DropDownItems.Add("&Open firmware source…", null, (s, e) => OpenSource());
            file.DropDownItems.Add("&Save firmware source", null, (s, e) => SaveSource());
            file.DropDownItems.Add(new ToolStripSeparator());
            file.DropDownItems.Add("Open .&bin / .hex image…", null, (s, e) => OpenImage());
            file.DropDownItems.Add(new ToolStripSeparator());
            file.DropDownItems.Add("E&xit", null, (s, e) => Close());

            var device = new ToolStripMenuItem("&Device");
            device.DropDownItems.Add("&Connect", null, (s, e) => Connect());
            device.DropDownItems.Add("&Disconnect", null, (s, e) => Disconnect());
            device.DropDownItems.Add(new ToolStripSeparator());
            device.DropDownItems.Add("Read &Identity", null, (s, e) => ReadIdentityAsync());
            device.DropDownItems.Add("Read Co&nfig", null, (s, e) => ReadConfigAsync());

            var help = new ToolStripMenuItem("&Help");
            help.DropDownItems.Add("&About", null, (s, e) => ShowAbout());

            menu.Items.Add(file);
            menu.Items.Add(device);
            menu.Items.Add(help);

            MainMenuStrip = menu;
            Controls.Add(menu);
        }

        private void BuildToolStrip()
        {
            var ts = new ToolStrip { GripStyle = ToolStripGripStyle.Hidden };
            ts.Items.Add(new ToolStripButton("New", null, (s, e) => NewSource()));
            ts.Items.Add(new ToolStripButton("Open", null, (s, e) => OpenSource()));
            ts.Items.Add(new ToolStripButton("Save", null, (s, e) => SaveSource()));
            ts.Items.Add(new ToolStripSeparator());
            ts.Items.Add(new ToolStripButton("Open Image", null, (s, e) => OpenImage()));
            ts.Items.Add(new ToolStripSeparator());
            ts.Items.Add(new ToolStripButton("Connect", null, (s, e) => Connect()));
            ts.Items.Add(new ToolStripButton("Disconnect", null, (s, e) => Disconnect()));
            ts.Items.Add(new ToolStripSeparator());
            ts.Items.Add(new ToolStripButton("Read Identity", null, (s, e) => ReadIdentityAsync()));
            Controls.Add(ts);
        }

        private void BuildStatusBar()
        {
            _status = new StatusStrip();
            _statusConn = new ToolStripStatusLabel("Disconnected") { Spring = true, TextAlign = ContentAlignment.MiddleLeft };
            _statusVersion = new ToolStripStatusLabel("No device") { TextAlign = ContentAlignment.MiddleRight };
            _status.Items.Add(_statusConn);
            _status.Items.Add(_statusVersion);
            Controls.Add(_status);
        }

        private void BuildConnectionPanel()
        {
            var panel = new Panel
            {
                Dock = DockStyle.Left,
                Width = 290,
                Padding = new Padding(10),
                BackColor = SystemColors.Control
            };

            var grp = new GroupBox
            {
                Text = "Connection",
                Dock = DockStyle.Top,
                Height = 360,
                Padding = new Padding(10)
            };

            int y = 26;
            const int rowH = 28;

            grp.Controls.Add(new Label { Text = "Transport", Left = 12, Top = y, Width = 110 });
            _cboTransport = new ComboBox
            {
                Left = 120, Top = y - 3, Width = 140, DropDownStyle = ComboBoxStyle.DropDownList
            };
            _cboTransport.Items.AddRange(new object[] { "J2534 (CAN)", "Serial UART", "USB-C" });
            _cboTransport.SelectedIndex = 0;
            _cboTransport.SelectedIndexChanged += (s, e) => { RefreshDevices(); UpdateBaudEnabled(); };
            grp.Controls.Add(_cboTransport);
            y += rowH;

            _lblDeviceCaption = new Label { Text = "Device", Left = 12, Top = y, Width = 110 };
            grp.Controls.Add(_lblDeviceCaption);
            _cboDevice = new ComboBox
            {
                Left = 120, Top = y - 3, Width = 140, DropDownStyle = ComboBoxStyle.DropDownList
            };
            grp.Controls.Add(_cboDevice);
            y += rowH;

            _lblBaudCaption = new Label { Text = "Baud", Left = 12, Top = y, Width = 110 };
            grp.Controls.Add(_lblBaudCaption);
            _cboBaud = new ComboBox
            {
                Left = 120, Top = y - 3, Width = 140, DropDownStyle = ComboBoxStyle.DropDownList
            };
            foreach (int b in BaudRates) _cboBaud.Items.Add(b);
            _cboBaud.SelectedItem = 921600;
            grp.Controls.Add(_cboBaud);
            y += rowH + 4;

            _btnRefresh = new Button { Text = "Refresh", Left = 12, Top = y, Width = 80 };
            _btnRefresh.Click += (s, e) => RefreshDevices();
            grp.Controls.Add(_btnRefresh);

            _btnConnect = new Button { Text = "Connect", Left = 100, Top = y, Width = 75 };
            _btnConnect.Click += (s, e) => Connect();
            grp.Controls.Add(_btnConnect);

            _btnDisconnect = new Button { Text = "Disconnect", Left = 181, Top = y, Width = 80 };
            _btnDisconnect.Click += (s, e) => Disconnect();
            grp.Controls.Add(_btnDisconnect);
            y += rowH + 6;

            grp.Controls.Add(new Label { Text = "Identity", Left = 12, Top = y, Width = 110 });
            y += 20;
            _txtIdentity = new TextBox
            {
                Left = 12, Top = y, Width = 248, Height = 150,
                Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical,
                BackColor = Color.White, Font = new Font("Consolas", 8.5f)
            };
            grp.Controls.Add(_txtIdentity);

            panel.Controls.Add(grp);
            Controls.Add(panel);
        }

        private void BuildTabs()
        {
            var tabs = new TabControl { Dock = DockStyle.Fill };

            tabs.TabPages.Add(BuildEditorTab());
            tabs.TabPages.Add(BuildFlashTab());
            tabs.TabPages.Add(BuildConsoleTab());

            Controls.Add(tabs);
        }

        private TabPage BuildEditorTab()
        {
            var page = new TabPage("Firmware Editor");

            var bar = new Panel { Dock = DockStyle.Top, Height = 34, Padding = new Padding(4) };
            var bNew = new Button { Text = "New", Left = 4, Top = 4, Width = 60 };
            bNew.Click += (s, e) => NewSource();
            var bOpen = new Button { Text = "Open", Left = 68, Top = 4, Width = 60 };
            bOpen.Click += (s, e) => OpenSource();
            var bSave = new Button { Text = "Save", Left = 132, Top = 4, Width = 60 };
            bSave.Click += (s, e) => SaveSource();
            var bBuild = new Button { Text = "Build (.bin)", Left = 200, Top = 4, Width = 90 };
            bBuild.Click += (s, e) => BuildWithArduinoCli();

            var lblCli = new Label { Text = "arduino-cli:", Left = 300, Top = 9, Width = 70, TextAlign = ContentAlignment.MiddleLeft };
            _txtArduinoCli = new TextBox { Left = 372, Top = 6, Width = 220, Text = "arduino-cli" };

            bar.Controls.AddRange(new Control[] { bNew, bOpen, bSave, bBuild, lblCli, _txtArduinoCli });

            _editor = new CodeEditor { Dock = DockStyle.Fill };
            _editor.Text = DefaultSketch();

            page.Controls.Add(_editor);
            page.Controls.Add(bar);
            return page;
        }

        private TabPage BuildFlashTab()
        {
            var page = new TabPage("Flash");

            var top = new Panel { Dock = DockStyle.Top, Height = 96, Padding = new Padding(8) };

            top.Controls.Add(new Label { Text = "Image:", Left = 8, Top = 12, Width = 50 });
            _txtImagePath = new TextBox { Left = 60, Top = 9, Width = 480, ReadOnly = true };
            top.Controls.Add(_txtImagePath);
            var bBrowse = new Button { Text = "Browse…", Left = 548, Top = 8, Width = 80 };
            bBrowse.Click += (s, e) => OpenImage();
            top.Controls.Add(bBrowse);

            _lblImageInfo = new Label { Left = 60, Top = 38, Width = 580, Text = "(no image loaded)" };
            top.Controls.Add(_lblImageInfo);

            _btnFlashRead = new Button { Text = "Read / Backup from device", Left = 60, Top = 60, Width = 200, Height = 28 };
            _btnFlashRead.Click += (s, e) => ReadBackupAsync();
            top.Controls.Add(_btnFlashRead);

            _btnFlashWrite = new Button { Text = "Write + Verify + Activate", Left = 270, Top = 60, Width = 200, Height = 28 };
            _btnFlashWrite.Click += (s, e) => FlashWriteAsync();
            top.Controls.Add(_btnFlashWrite);

            var progPanel = new Panel { Dock = DockStyle.Top, Height = 30, Padding = new Padding(8, 4, 8, 4) };
            _progress = new ProgressBar { Dock = DockStyle.Fill, Minimum = 0, Maximum = 100 };
            progPanel.Controls.Add(_progress);

            _txtFlashLog = new TextBox
            {
                Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                ScrollBars = ScrollBars.Both, WordWrap = false,
                Font = new Font("Consolas", 8.5f), BackColor = Color.White
            };

            page.Controls.Add(_txtFlashLog);
            page.Controls.Add(progPanel);
            page.Controls.Add(top);
            return page;
        }

        private TabPage BuildConsoleTab()
        {
            var page = new TabPage("Console");

            var bar = new Panel { Dock = DockStyle.Top, Height = 32, Padding = new Padding(4) };
            var bClear = new Button { Text = "Clear", Left = 4, Top = 3, Width = 70 };
            bClear.Click += (s, e) => _txtConsole.Clear();
            bar.Controls.Add(bClear);

            _txtConsole = new TextBox
            {
                Dock = DockStyle.Fill, Multiline = true, ReadOnly = true,
                ScrollBars = ScrollBars.Both, WordWrap = false,
                Font = new Font("Consolas", 8.5f),
                BackColor = Color.FromArgb(24, 24, 24), ForeColor = Color.Gainsboro
            };

            page.Controls.Add(_txtConsole);
            page.Controls.Add(bar);
            return page;
        }

        // =====================================================================
        //  Device enumeration / connection
        // =====================================================================

        private TransportKind SelectedTransport
        {
            get
            {
                switch (_cboTransport.SelectedIndex)
                {
                    case 1: return TransportKind.Serial;
                    case 2: return TransportKind.UsbC;
                    default: return TransportKind.J2534;
                }
            }
        }

        private void UpdateBaudEnabled()
        {
            bool serial = SelectedTransport != TransportKind.J2534;
            _cboBaud.Enabled = serial;
            _lblBaudCaption.Enabled = serial;
            _lblDeviceCaption.Text = serial ? "COM Port" : "Device";
        }

        private void RefreshDevices()
        {
            try
            {
                _cboDevice.Items.Clear();
                if (SelectedTransport == TransportKind.J2534)
                {
                    var devices = J2534DeviceFinder.FindInstalledJ2534DLLs();
                    foreach (var d in devices) _cboDevice.Items.Add(d);
                    if (_cboDevice.Items.Count > 0) _cboDevice.SelectedIndex = 0;
                    else _cboDevice.Items.Add("(no J2534 devices found)");
                }
                else
                {
                    string[] ports = SerialPort.GetPortNames();
                    Array.Sort(ports);
                    foreach (var p in ports) _cboDevice.Items.Add(p);
                    if (_cboDevice.Items.Count > 0) _cboDevice.SelectedIndex = 0;
                    else _cboDevice.Items.Add("(no COM ports found)");
                }
                if (_cboDevice.SelectedIndex < 0 && _cboDevice.Items.Count > 0)
                    _cboDevice.SelectedIndex = 0;
            }
            catch (Exception ex)
            {
                ConsoleLog("Device refresh error: " + ex.Message);
            }
            UpdateBaudEnabled();
        }

        private void Connect()
        {
            if (_transport != null && _transport.IsOpen)
            {
                ConsoleLog("Already connected — disconnect first.");
                return;
            }

            try
            {
                if (SelectedTransport == TransportKind.J2534)
                {
                    if (!(_cboDevice.SelectedItem is J2534Device dev))
                    {
                        MessageBox.Show("Select a J2534 device first.", "Connect",
                            MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                    var jt = new J2534Transport();
                    jt.Log += ConsoleLog;
                    jt.FrameTraffic += OnFrameTraffic;
                    _transport = jt;
                    if (!jt.Open(dev))
                    {
                        ConsoleLog("J2534 connect failed.");
                        _transport = null;
                        return;
                    }
                }
                else
                {
                    string port = _cboDevice.SelectedItem as string;
                    if (string.IsNullOrEmpty(port) || port.StartsWith("("))
                    {
                        MessageBox.Show("Select a COM port first.", "Connect",
                            MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                    int baud = _cboBaud.SelectedItem is int b ? b : 921600;
                    var st = new SerialTransport();
                    st.Log += ConsoleLog;
                    st.FrameTraffic += OnFrameTraffic;
                    _transport = st;
                    if (!st.Open(port, baud))
                    {
                        ConsoleLog("Serial connect failed.");
                        _transport = null;
                        return;
                    }
                }

                _dpp = new DppClient(_transport);
                _dpp.Log += FlashLog;
                _dpp.Progress += OnProgress;

                UpdateConnectionState(true);
                ConsoleLog("Connected: " + _transport.Name);
                ReadIdentityAsync();
            }
            catch (Exception ex)
            {
                ConsoleLog("Connect error: " + ex.Message);
                MessageBox.Show("Connection error:\r\n" + ex.Message, "Connect",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                _transport = null;
            }
        }

        private void Disconnect()
        {
            try
            {
                _transport?.Close();
            }
            catch (Exception ex) { ConsoleLog("Disconnect error: " + ex.Message); }
            finally
            {
                _transport = null;
                _dpp = null;
                UpdateConnectionState(false);
                ConsoleLog("Disconnected.");
            }
        }

        private void UpdateConnectionState(bool connected)
        {
            _btnConnect.Enabled = !connected;
            _btnDisconnect.Enabled = connected;
            _cboTransport.Enabled = !connected;
            _cboDevice.Enabled = !connected;
            _cboBaud.Enabled = !connected && SelectedTransport != TransportKind.J2534;
            _btnRefresh.Enabled = !connected;
            _btnFlashWrite.Enabled = connected;
            _btnFlashRead.Enabled = connected;

            _statusConn.Text = connected ? "Connected — " + (_transport?.Name ?? "") : "Disconnected";
            if (!connected) { _statusVersion.Text = "No device"; _txtIdentity.Clear(); }
        }

        // =====================================================================
        //  Device operations (async wrappers keep the UI responsive)
        // =====================================================================

        private async void ReadIdentityAsync()
        {
            if (!EnsureConnected()) return;
            SetBusy(true);
            try
            {
                DeviceIdentity id = await Task.Run(() => _dpp.ReadIdentity());
                var sb = new StringBuilder();
                sb.AppendLine("Protocol : " + id.ProtocolVersion);
                sb.AppendLine("App ver  : " + id.AppVersion);
                sb.AppendLine("Serial   : " + id.Serial);
                if (id.ChipMac != null) sb.AppendLine("MAC      : " + BitConverter.ToString(id.ChipMac).Replace("-", ":"));
                sb.AppendLine("Run slot : " + id.RunningSlot);
                sb.AppendLine("Slot A   : " + id.SlotASize + " B");
                sb.AppendLine("Slot B   : " + id.SlotBSize + " B");
                sb.AppendLine("App used : " + id.AppUsed + " B");
                _txtIdentity.Text = sb.ToString();
                _statusVersion.Text = "FW " + id.AppVersion;
            }
            catch (Exception ex)
            {
                ConsoleLog("Read identity failed: " + ex.Message);
            }
            finally { SetBusy(false); }
        }

        private async void ReadConfigAsync()
        {
            if (!EnsureConnected()) return;
            SetBusy(true);
            try
            {
                DeviceConfig cfg = await Task.Run(() => _dpp.ReadConfig());
                var sb = new StringBuilder();
                sb.AppendLine("=== Device Config (DID 0x0100) ===");
                sb.AppendLine("Version       : " + cfg.Version);
                sb.AppendLine("MITM mode     : " + cfg.MitmMode);
                sb.AppendLine("CAN1 bitrate  : " + cfg.Can1Bitrate);
                sb.AppendLine("CAN2 bitrate  : " + cfg.Can2Bitrate);
                sb.AppendLine("CAN-FD enable : 0x" + cfg.CanFdEnable.ToString("X2"));
                sb.AppendLine("FD data rate  : " + cfg.CanFdDataBitrate);
                sb.AppendLine("Term1 (120R)  : " + cfg.Term1Enable);
                sb.AppendLine("Term2 (120R)  : " + cfg.Term2Enable);
                sb.AppendLine("Device name   : " + cfg.DeviceName);
                FlashLog(sb.ToString());
                ConsoleLog(sb.ToString());
            }
            catch (Exception ex)
            {
                ConsoleLog("Read config failed: " + ex.Message);
            }
            finally { SetBusy(false); }
        }

        private async void ReadBackupAsync()
        {
            if (!EnsureConnected()) return;

            uint addr = 0;
            int len;
            using (var dlg = new BackupDialog())
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                addr = dlg.Address;
                len = dlg.Length;
            }
            if (len <= 0) return;

            string savePath;
            using (var sfd = new SaveFileDialog { Filter = "Binary image (*.bin)|*.bin|All files (*.*)|*.*", FileName = "backup.bin" })
            {
                if (sfd.ShowDialog(this) != DialogResult.OK) return;
                savePath = sfd.FileName;
            }

            SetBusy(true);
            _progress.Value = 0;
            try
            {
                byte[] data = await Task.Run(() => _dpp.ReadMemory(addr, len));
                File.WriteAllBytes(savePath, data);
                FlashLog($"Backup saved: {savePath} ({data.Length:N0} bytes, CRC32 0x{Crc.Crc32(data):X8})");
            }
            catch (Exception ex)
            {
                FlashLog("Backup failed: " + ex.Message);
            }
            finally { SetBusy(false); }
        }

        private async void FlashWriteAsync()
        {
            if (!EnsureConnected()) return;
            if (_loadedImage == null)
            {
                MessageBox.Show("Load a .bin/.hex image first (Flash tab → Browse).", "Flash",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var confirm = MessageBox.Show(
                $"Flash {_loadedImage.Size:N0} bytes (CRC32 0x{_loadedImage.Crc32:X8}) to the device?\r\n\r\n" +
                "The image is written to the inactive OTA slot, verified, then activated on reset.",
                "Confirm flash", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (confirm != DialogResult.Yes) return;

            SetBusy(true);
            _progress.Value = 0;
            byte[] image = _loadedImage.Bytes;
            try
            {
                await Task.Run(() => _dpp.Program(image));
                _progress.Value = 100;
                FlashLog("Flash finished successfully.");
                MessageBox.Show("Flash complete — the unit is rebooting into the new image.",
                    "Flash", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                FlashLog("FLASH FAILED: " + ex.Message);
                MessageBox.Show("Flash failed:\r\n" + ex.Message, "Flash",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally { SetBusy(false); }
        }

        private bool EnsureConnected()
        {
            if (_dpp != null && _transport != null && _transport.IsOpen) return true;
            MessageBox.Show("Connect to the device first.", "Not connected",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        private void SetBusy(bool busy)
        {
            if (InvokeRequired) { BeginInvoke((Action)(() => SetBusy(busy))); return; }
            UseWaitCursor = busy;
            _btnFlashWrite.Enabled = !busy && _transport != null && _transport.IsOpen;
            _btnFlashRead.Enabled = !busy && _transport != null && _transport.IsOpen;
            _btnConnect.Enabled = !busy && (_transport == null || !_transport.IsOpen);
            _btnDisconnect.Enabled = !busy && _transport != null && _transport.IsOpen;
        }

        // =====================================================================
        //  Firmware source (editor) file ops
        // =====================================================================

        private void NewSource()
        {
            _editor.Text = DefaultSketch();
            _currentSourcePath = null;
            _editor.Modified = false;
        }

        private void OpenSource()
        {
            using (var ofd = new OpenFileDialog
            {
                Filter = "Firmware source (*.ino;*.c;*.cpp;*.h;*.hpp)|*.ino;*.c;*.cpp;*.h;*.hpp|All files (*.*)|*.*"
            })
            {
                if (ofd.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    _editor.Text = File.ReadAllText(ofd.FileName);
                    _currentSourcePath = ofd.FileName;
                    _editor.Modified = false;
                    ConsoleLog("Opened source: " + ofd.FileName);
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Open failed:\r\n" + ex.Message, "Open",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void SaveSource()
        {
            if (string.IsNullOrEmpty(_currentSourcePath))
            {
                using (var sfd = new SaveFileDialog
                {
                    Filter = "Arduino sketch (*.ino)|*.ino|C++ source (*.cpp)|*.cpp|All files (*.*)|*.*",
                    FileName = "firmware.ino"
                })
                {
                    if (sfd.ShowDialog(this) != DialogResult.OK) return;
                    _currentSourcePath = sfd.FileName;
                }
            }
            try
            {
                File.WriteAllText(_currentSourcePath, _editor.Text);
                _editor.Modified = false;
                ConsoleLog("Saved source: " + _currentSourcePath);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Save failed:\r\n" + ex.Message, "Save",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        // =====================================================================
        //  Build via arduino-cli (shell-out)
        // =====================================================================

        private void BuildWithArduinoCli()
        {
            // The sketch must be saved first (arduino-cli compiles a sketch folder).
            if (string.IsNullOrEmpty(_currentSourcePath))
            {
                SaveSource();
                if (string.IsNullOrEmpty(_currentSourcePath)) return;
            }
            else if (_editor.Modified)
            {
                SaveSource();
            }

            string cli = string.IsNullOrWhiteSpace(_txtArduinoCli.Text) ? "arduino-cli" : _txtArduinoCli.Text.Trim();

            // Probe for arduino-cli; if it's not on PATH / at the given path, guide the user.
            if (!ArduinoCliAvailable(cli))
            {
                MessageBox.Show(
                    "arduino-cli was not found.\r\n\r\n" +
                    "The Build feature shells out to arduino-cli to compile the firmware\r\n" +
                    "sketch into a .bin image. To enable it:\r\n\r\n" +
                    "  1. Download arduino-cli from https://arduino.github.io/arduino-cli/\r\n" +
                    "  2. Unzip arduino-cli.exe somewhere on your PATH (or set the full\r\n" +
                    "     path in the 'arduino-cli:' box on this tab).\r\n" +
                    "  3. Install the ESP32 core:  arduino-cli core install esp32:esp32\r\n\r\n" +
                    "Then click Build again.",
                    "arduino-cli not found",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            string sketchDir = Path.GetDirectoryName(_currentSourcePath);
            string fqbn = "esp32:esp32:esp32s3";
            string outDir = Path.Combine(sketchDir ?? ".", "build");

            ConsoleLog($"Building {sketchDir} for {fqbn}…");
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = cli,
                    Arguments = $"compile --fqbn {fqbn} --output-dir \"{outDir}\" \"{sketchDir}\"",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };
                using (var proc = Process.Start(psi))
                {
                    string stdout = proc.StandardOutput.ReadToEnd();
                    string stderr = proc.StandardError.ReadToEnd();
                    proc.WaitForExit();
                    if (!string.IsNullOrWhiteSpace(stdout)) ConsoleLog(stdout.TrimEnd());
                    if (!string.IsNullOrWhiteSpace(stderr)) ConsoleLog(stderr.TrimEnd());

                    if (proc.ExitCode == 0)
                    {
                        ConsoleLog("Build succeeded → " + outDir);
                        MessageBox.Show("Build succeeded.\r\nOutput: " + outDir, "Build",
                            MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    else
                    {
                        ConsoleLog("Build failed (exit code " + proc.ExitCode + ").");
                        MessageBox.Show("Build failed (exit " + proc.ExitCode + "). See Console tab.",
                            "Build", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                ConsoleLog("Build error: " + ex.Message);
                MessageBox.Show("Build error:\r\n" + ex.Message, "Build",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private static bool ArduinoCliAvailable(string cli)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = cli,
                    Arguments = "version",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };
                using (var p = Process.Start(psi))
                {
                    if (p == null) return false;
                    p.WaitForExit(4000);
                    return true;
                }
            }
            catch { return false; }
        }

        // =====================================================================
        //  Image load (Flash tab)
        // =====================================================================

        private void OpenImage()
        {
            using (var ofd = new OpenFileDialog
            {
                Filter = "Firmware image (*.bin;*.hex)|*.bin;*.hex|Binary (*.bin)|*.bin|Intel HEX (*.hex)|*.hex|All files (*.*)|*.*"
            })
            {
                if (ofd.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    _loadedImage = FirmwareImage.Load(ofd.FileName);
                    _txtImagePath.Text = ofd.FileName;
                    _lblImageInfo.Text = _loadedImage.Describe();
                    FlashLog("Loaded image: " + ofd.FileName + "  [" + _loadedImage.Describe() + "]");
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Failed to load image:\r\n" + ex.Message, "Open image",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        // =====================================================================
        //  Logging / events (marshalled to the UI thread)
        // =====================================================================

        private void OnProgress(int cur, int total)
        {
            if (InvokeRequired) { BeginInvoke((Action)(() => OnProgress(cur, total))); return; }
            int pct = total > 0 ? (int)(100L * cur / total) : 0;
            if (pct < 0) pct = 0; if (pct > 100) pct = 100;
            _progress.Value = pct;
            _statusConn.Text = $"Working… {pct}%";
        }

        private void OnFrameTraffic(string dir, uint id, byte[] data)
        {
            string hex = data == null ? "" : BitConverter.ToString(data).Replace("-", " ");
            string line = id != 0
                ? $"[{dir}] 0x{id:X3}  {hex}"
                : $"[{dir}] {hex}";
            ConsoleLog(line);
        }

        private void ConsoleLog(string msg)
        {
            if (string.IsNullOrEmpty(msg)) return;
            if (InvokeRequired) { BeginInvoke((Action)(() => ConsoleLog(msg))); return; }
            _txtConsole.AppendText(Stamp(msg) + Environment.NewLine);
        }

        private void FlashLog(string msg)
        {
            if (string.IsNullOrEmpty(msg)) return;
            if (InvokeRequired) { BeginInvoke((Action)(() => FlashLog(msg))); return; }
            _txtFlashLog.AppendText(msg + Environment.NewLine);
            // Mirror flash steps into the console too.
            _txtConsole.AppendText(Stamp(msg) + Environment.NewLine);
        }

        private static string Stamp(string msg) => DateTime.Now.ToString("HH:mm:ss.fff") + "  " + msg;

        // =====================================================================
        //  About
        // =====================================================================

        private void ShowAbout()
        {
            MessageBox.Show(
                "Tester Present — TP-CAN-2I Configurator\r\n" +
                "Programmable Dual-CAN Inline Interface\r\n\r\n" +
                "TP-DPP v1.0  (J2534 CAN · Serial UART · USB-C)\r\n\r\n" +
                "© 2026 Jack Leighton — Designed in Australia",
                "About",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            try { _transport?.Close(); } catch { /* ignore */ }
            base.OnFormClosing(e);
        }

        private static string DefaultSketch()
        {
            return
@"// TP-CAN-2I firmware sketch (ESP32-S3)
// Implements TP-DPP v1.0 over CAN1/CAN2 (ISO-TP), UART and USB-C CDC.
#include <Arduino.h>
#include <stdint.h>

// TP-DPP CAN identifiers
static const uint32_t DPP_TX_ID = 0x7FA; // device -> PC
static const uint32_t DPP_RX_ID = 0x7FE; // PC -> device

void setup() {
    Serial.begin(921600);
    // TODO: init TWAI (CAN), OTA partitions, USB-CDC
}

void loop() {
    // TODO: service TP-DPP requests (0x10/0x22/0x34/0x36/0x37/0x31 …)
}
";
        }

        // =================================================================
        //  Small modal dialog for the Read/Backup address + length.
        // =================================================================
        private sealed class BackupDialog : Form
        {
            private readonly TextBox _addr;
            private readonly TextBox _len;
            public uint Address { get; private set; }
            public int Length { get; private set; }

            public BackupDialog()
            {
                Text = "Read / Backup";
                FormBorderStyle = FormBorderStyle.FixedDialog;
                StartPosition = FormStartPosition.CenterParent;
                MinimizeBox = false; MaximizeBox = false;
                Width = 320; Height = 170;

                Controls.Add(new Label { Text = "Start address (hex):", Left = 12, Top = 16, Width = 130 });
                _addr = new TextBox { Left = 150, Top = 13, Width = 140, Text = "00000000" };
                Controls.Add(_addr);

                Controls.Add(new Label { Text = "Length (bytes):", Left = 12, Top = 48, Width = 130 });
                _len = new TextBox { Left = 150, Top = 45, Width = 140, Text = "65536" };
                Controls.Add(_len);

                var ok = new Button { Text = "Read", Left = 134, Top = 90, Width = 70, DialogResult = DialogResult.OK };
                ok.Click += (s, e) =>
                {
                    uint a;
                    int l;
                    if (!uint.TryParse(_addr.Text.Trim(), System.Globalization.NumberStyles.HexNumber,
                            System.Globalization.CultureInfo.InvariantCulture, out a))
                    { MessageBox.Show("Invalid address."); DialogResult = DialogResult.None; return; }
                    if (!int.TryParse(_len.Text.Trim(), out l) || l <= 0)
                    { MessageBox.Show("Invalid length."); DialogResult = DialogResult.None; return; }
                    Address = a; Length = l;
                };
                Controls.Add(ok);

                var cancel = new Button { Text = "Cancel", Left = 210, Top = 90, Width = 70, DialogResult = DialogResult.Cancel };
                Controls.Add(cancel);

                AcceptButton = ok; CancelButton = cancel;
            }
        }
    }
}
