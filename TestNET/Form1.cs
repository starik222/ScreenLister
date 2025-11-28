using ScreenLister;
using System.Windows.Forms;

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
            OpenFileDialog openFileDialog1 = new OpenFileDialog();
            if (openFileDialog1.ShowDialog() != DialogResult.OK)
                return;
            Image img = ScreenListerNET.GetScreenList(openFileDialog1.FileName, 8, 30, 50);
            pictureBox1.Image = img;
        }

        private void button2_Click(object sender, EventArgs e)
        {
            byte[] data = File.ReadAllBytes("H:\\ScreenLister\\x64\\Debug\\frameData.dat");
            byte[] extra = File.ReadAllBytes("H:\\ScreenLister\\x64\\Debug\\extraData.dat");

            Image img = ScreenListerNET.DecodeH264Frame(data, extra);
            pictureBox1.Image = img;
        }

        private void button3_Click(object sender, EventArgs e)
        {
        }

        private void button4_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog1 = new OpenFileDialog();
            if (openFileDialog1.ShowDialog() != DialogResult.OK)
                return;
            Image img = ScreenListerNET.GetImageFromVideoFile(openFileDialog1.FileName, 0);
            pictureBox1.Image = img;
        }

        private void button5_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog1 = new OpenFileDialog();
            if (openFileDialog1.ShowDialog() != DialogResult.OK)
                return;
            for (int i = 0; i < 1000; i++)
            {
                Image img = ScreenListerNET.GetImageFromVideoFile(openFileDialog1.FileName, 0);
                pictureBox1.Image = img;
            }
        }
    }
}
