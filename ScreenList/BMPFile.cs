using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;

namespace ScreenLister
{
    public class BMPFile
    {
        public enum BitmapCompressionMode : uint
        {
            BI_RGB = 0,
            BI_RLE8 = 1,
            BI_RLE4 = 2,
            BI_BITFIELDS = 3,
            BI_JPEG = 4,
            BI_PNG = 5
        }

        [StructLayout(LayoutKind.Sequential, Pack = 2)]
        public struct BITMAPFILEHEADER
        {
            public ushort bfType;
            public uint bfSize;
            public ushort bfReserved1;
            public ushort bfReserved2;
            public uint bfOffBits;

            public void Init()
            {
                bfType = 19778;
                bfReserved1 = 0;
                bfReserved2 = 0;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct BITMAPINFOHEADER
        {
            public uint biSize;
            public int biWidth;
            public int biHeight;
            public ushort biPlanes;
            public ushort biBitCount;
            public BitmapCompressionMode biCompression;
            public uint biSizeImage;
            public int biXPelsPerMeter;
            public int biYPelsPerMeter;
            public uint biClrUsed;
            public uint biClrImportant;

            public void Init()
            {
                biSize = (uint)Marshal.SizeOf(this);
                biPlanes = 1;
                biCompression = BitmapCompressionMode.BI_RGB;
                biClrUsed = 0;
                biClrImportant = 0;
                biXPelsPerMeter = 0;
                biYPelsPerMeter = 0;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct RGBQUAD
        {
            public byte rgbBlue;
            public byte rgbGreen;
            public byte rgbRed;
            public byte rgbReserved;

            public RGBQUAD(byte B, byte G, byte R, byte A)
            {
                rgbBlue = B;
                rgbGreen = G;
                rgbRed = R;
                rgbReserved = A;
            }
        }


        public BITMAPFILEHEADER bmf;
        public BITMAPINFOHEADER bmi;
        public int originalBitCount;
        public int originalType;

        public byte[] imageData;
        public List<RGBQUAD> palette;
        public string filename;
        public BMPFile(int width, int height, int bitCount)
        {
            bmi.Init();
            bmf.Init();
            bmf.bfOffBits = 54;
            bmi.biWidth = width;
            bmi.biHeight = height;
            bmi.biBitCount = (ushort)bitCount;
            palette = new List<RGBQUAD>();
        }

        public BMPFile()
        {
            palette = new List<RGBQUAD>();
        }


        public byte[] CreateFromBuffer(byte[] imgBuffer)
        {
            //bmf.bfSize = (uint)(bmf.bfOffBits + +bmi.biWidth * bmi.biHeight * 24 / 8);
            //bmi.biBitCount = 24;



            using (MemoryStream ms = new MemoryStream())
            {
                BinaryWriter writer = new BinaryWriter(ms);
                bmf.bfType = 19778;
                bmf.bfSize = 40 + (uint)imgBuffer.Length;
                bmi.biSizeImage = (uint)imgBuffer.Length;
                //bmi.biBitCount = 24;
                bmf.bfOffBits = 54;
                writer.WriteRawSerialize(bmf);
                writer.WriteRawSerialize(bmi);
                writer.Write(imgBuffer);
                byte[] data = ms.ToArray();
                writer.Close();
                return data;
            }
        }


        public void SaveImage(string FilePath, byte[] imgBuffer)
        {
            FileStream fs = new FileStream(FilePath, FileMode.Create);
            BinaryWriter writer = new BinaryWriter(fs);
            bmf.bfType = 19778;
            if (originalType == 1 && bmi.biBitCount == 8)
            {
                bmf.bfSize = 1064 + (uint)imgBuffer.Length;
                bmi.biSizeImage = (uint)imgBuffer.Length;
                bmf.bfOffBits = 1078;
                writer.WriteRawSerialize(bmf);
                writer.WriteRawSerialize(bmi);
                foreach (var item in palette)
                {
                    writer.WriteRawSerialize(item);
                }
            }
            else
            {
                bmf.bfSize = 40 + (uint)imgBuffer.Length;
                bmi.biSizeImage = (uint)imgBuffer.Length;
                bmf.bfOffBits = 54;
                writer.WriteRawSerialize(bmf);
                writer.WriteRawSerialize(bmi);
            }
            writer.Write(imgBuffer);
            writer.Close();
        }

        public void SaveImage(string FilePath)
        {
            SaveImage(FilePath, imageData);
        }

        public void LoadImage(string FilePath)
        {
            filename = FilePath;
            FileStream fs = new FileStream(FilePath, FileMode.Open);
            BinaryReader reader = new BinaryReader(fs);
            bmf = (BITMAPFILEHEADER)reader.ReadStruct(typeof(BITMAPFILEHEADER));
            bmi = (BITMAPINFOHEADER)reader.ReadStruct(typeof(BITMAPINFOHEADER));
            if (bmi.biBitCount == 8)
            {
                for (int i = 0; i < 256; i++)
                {
                    palette.Add((RGBQUAD)reader.ReadStruct(typeof(RGBQUAD)));
                }
            }
            imageData = reader.ReadToEnd();
        }

        /// <summary>
        /// Resize the image to the specified width and height.
        /// </summary>
        /// <param name="image">The image to resize.</param>
        /// <param name="width">The width to resize to.</param>
        /// <param name="height">The height to resize to.</param>
        /// <returns>The resized image.</returns>
        public static Bitmap ResizeImage(Image image, int width, int height, bool proportionally)
        {
            if (proportionally)
            {
                var ratioX = (double)width / image.Width;
                var ratioY = (double)height / image.Height;
                var ratio = Math.Min(ratioX, ratioY);

                width = (int)(image.Width * ratio);
                height = (int)(image.Height * ratio);
            }
            var destRect = new Rectangle(0, 0, width, height);
            var destImage = new Bitmap(width, height);

            destImage.SetResolution(image.HorizontalResolution, image.VerticalResolution);

            using (var graphics = Graphics.FromImage(destImage))
            {
                graphics.CompositingMode = CompositingMode.SourceCopy;
                graphics.CompositingQuality = CompositingQuality.HighQuality;
                graphics.InterpolationMode = InterpolationMode.Bicubic;
                graphics.SmoothingMode = SmoothingMode.HighQuality;
                graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;

                using (var wrapMode = new ImageAttributes())
                {
                    wrapMode.SetWrapMode(WrapMode.TileFlipXY);
                    graphics.DrawImage(image, destRect, 0, 0, image.Width, image.Height, GraphicsUnit.Pixel, wrapMode);
                }
            }

            return destImage;
        }
    }
}
