using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using ScreenLister;
using System.IO;

namespace TestNET
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (openFileDialog1.ShowDialog() != DialogResult.OK)
                return;
            Image img = ScreenListerNET.GetScreenList(openFileDialog1.FileName, 5, 30, 50);
            pictureBox1.Image = img;
        }

        private void button2_Click(object sender, EventArgs e)
        {
            byte[] data = File.ReadAllBytes("H:\\ScreenLister\\x64\\Debug\\frameData.dat");
            byte[] extra = File.ReadAllBytes("H:\\ScreenLister\\x64\\Debug\\extraData.dat");

            Image img = ScreenListerNET.DecodeH264Frame(data, extra);
            pictureBox1.Image = img;
        }
    }
}
