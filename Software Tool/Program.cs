// Tester Present Specialist Automotive Solutions — TP-CAN-2I Configurator — © 2026 Jack Leighton — Designed in Australia
using System;
using System.Windows.Forms;

namespace TesterPresent.Configurator
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            try
            {
                Application.Run(new MainForm());
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    "A fatal error occurred:\r\n\r\n" + ex,
                    "TP-CAN-2I Configurator",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }
    }
}
