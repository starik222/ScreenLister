using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Drawing;
using System.IO;
using System.Drawing.Drawing2D;


namespace ScreenLister
{
    public class ScreenListerNET
    {

        public static Image DecodeH264Frame(byte[] frameBuffer, byte[] extraData, int codecFormat = 27)
        {
            IntPtr memBuffer = Marshal.AllocHGlobal(frameBuffer.Length);
            IntPtr memExtraData = IntPtr.Zero;
            Marshal.Copy(frameBuffer, 0, memBuffer, frameBuffer.Length);
           // frameBuffer = null;
            if (extraData.Length > 0)
            {
                memExtraData = Marshal.AllocHGlobal(extraData.Length);
                Marshal.Copy(extraData, 0, memExtraData, extraData.Length);
            }
            Ext.ImageBuf res = Ext.DecodeFrame(codecFormat, memBuffer, frameBuffer.Length, memExtraData, extraData.Length);
            Marshal.FreeHGlobal(memBuffer);
            if (extraData.Length > 0)
                Marshal.FreeHGlobal(memExtraData);
            if (res.BufSize > 0)
            {
                //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                BMPFile bMPFile = new BMPFile(res.Width, res.Height, 32);
                //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                byte[] imgBuffer = new byte[res.BufSize];
                Marshal.Copy(res.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                {
                    using (Image img = Image.FromStream(ms))
                    {
                        img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                        ms.Close();
                        Ext.FreeImageBuffer(res);
                        return (Image)img.Clone();
                    }
                }
            }
            return null;
        }

        public static byte[] DecodeH264Frame(IntPtr frameBuffer, int frameBufferSize, IntPtr extraData, int extraDataSize = 0, int codecFormat = 27)
        {
            Ext.ImageBuf res = Ext.DecodeFrame(codecFormat, frameBuffer, frameBufferSize, extraData, extraDataSize);
            //Marshal.FreeHGlobal(frameBuffer);
            //if (extraData.Length > 0)
                //Marshal.FreeHGlobal(memExtraData);
            if (res.BufSize > 0)
            {
                //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                BMPFile bMPFile = new BMPFile(res.Width, res.Height, 32);
                //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                byte[] imgBuffer = new byte[res.BufSize];
                Marshal.Copy(res.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                byte[] resData = null;
                using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                {
                    
                    using (Image img = Image.FromStream(ms))
                    {
                        img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                        ms.Close();
                        Ext.FreeImageBuffer(res);
                        //return (Image)img.Clone();
                        MemoryStream nMem = new MemoryStream();
                        img.Save(nMem, System.Drawing.Imaging.ImageFormat.Jpeg);
                        resData = nMem.ToArray();
                        nMem.Close();
                        ms.Close();
                    }
                }
                return resData;
            }
            return null;
        }
        public static Image GetImageFromVideoBuffer(byte[] videoData, long imageTime)
        {
            IntPtr memBuffer = Marshal.AllocHGlobal(videoData.Length);
            Marshal.Copy(videoData, 0, memBuffer, videoData.Length);
            Ext.ImageBuf res = Ext.GetImageFromVideoBuffer(memBuffer, videoData.Length, imageTime);
            Marshal.FreeHGlobal(memBuffer);
            if (res.BufSize > 0)
            {
                //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                BMPFile bMPFile = new BMPFile(res.Width, res.Height, 32);
                //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                byte[] imgBuffer = new byte[res.BufSize];
                Marshal.Copy(res.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                {
                    using (Image img = Image.FromStream(ms))
                    {
                        img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                        ms.Close();
                        Ext.FreeImageBuffer(res);
                        return (Image)img.Clone();
                    }
                }
            }
            return null;
        }

        public static Image GetImageFromVideoFile(string fileName, long imageTime)
        {
            Ext.ImageBuf res = Ext.GetImageFromVideoFile(fileName, imageTime);
            if (res.BufSize > 0)
            {
                //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                BMPFile bMPFile = new BMPFile(res.Width, res.Height, 32);
                //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                byte[] imgBuffer = new byte[res.BufSize];
                Marshal.Copy(res.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                {
                    using (Image img = Image.FromStream(ms))
                    {
                        img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                        ms.Close();
                        Ext.FreeImageBuffer(res);
                        return (Image)img.Clone();
                    }
                }
            }
            return null;
        }


        public static Image GetScreenList(string videoPath, int inLineCount, int allCount, int PreviewImageResizePercent)
        {

            Ext.ScreenList list = Ext.GetImagesV2(videoPath, allCount, PreviewImageResizePercent);
            //Ext.ScreenList list = Marshal.PtrToStructure<Ext.ScreenList>(IntPtr.Zero);
            List<Ext.ImageBuf> bufInfo = new List<Ext.ImageBuf>();
            List<KeyValuePair<TimeSpan, Image>> imgs = new List<KeyValuePair<TimeSpan, Image>>();
            for (int i = 0; i < list.ImgCount; i++)
            {
                Ext.ImageBuf m_img = Marshal.PtrToStructure<Ext.ImageBuf>(list.bufs + (Marshal.SizeOf<Ext.ImageBuf>()) * i);
                if (m_img.BufSize > 0)
                {
                    //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                    BMPFile bMPFile = new BMPFile(list.Width, list.Height, 32);
                    //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                    byte[] imgBuffer = new byte[m_img.BufSize];
                    Marshal.Copy(m_img.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                    using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                    {
                        using (Image img = Image.FromStream(ms))
                        {
                            img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                            ms.Close();
                            imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(m_img.ImageTime), (Image)img.Clone()));
                        }
                    }
                }
                else
                {
                    imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(-1), null));
                }
                 
            }
            Ext.FreeImageList(list);
            //Marshal.FreeHGlobal(list.bufs);
            return GenerateScreenList(imgs, inLineCount, allCount, list.Period, list.Width, list.Height);

        }

        public static List<KeyValuePair<TimeSpan, Image>> GetImages(string videoPath, int inLineCount, int allCount, int PreviewImageResizePercent)
        {

            Ext.ScreenList list = Ext.GetImagesV2(videoPath, allCount, PreviewImageResizePercent);
            //Ext.ScreenList list = Marshal.PtrToStructure<Ext.ScreenList>(IntPtr.Zero);
            List<Ext.ImageBuf> bufInfo = new List<Ext.ImageBuf>();
            List<KeyValuePair<TimeSpan, Image>> imgs = new List<KeyValuePair<TimeSpan, Image>>();
            for (int i = 0; i < list.ImgCount; i++)
            {
                Ext.ImageBuf m_img = Marshal.PtrToStructure<Ext.ImageBuf>(list.bufs + (Marshal.SizeOf<Ext.ImageBuf>()) * i);
                if (m_img.BufSize > 0)
                {
                    //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                    BMPFile bMPFile = new BMPFile(list.Width, list.Height, 32);
                    //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                    byte[] imgBuffer = new byte[m_img.BufSize];
                    Marshal.Copy(m_img.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                    using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                    {
                        using (Image img = Image.FromStream(ms))
                        {
                            img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                            ms.Close();
                            imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(m_img.ImageTime), (Image)img.Clone()));
                        }
                    }
                }
                else
                {
                    imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(-1), null));
                }

            }
            Ext.FreeImageList(list);
            //Marshal.FreeHGlobal(list.bufs);
            return imgs;

        }

        public static ImageData GetScreenListWithImages(string videoPath, int inLineCount, int allCount, int PreviewImageResizePercent)
        {

            Ext.ScreenList list = Ext.GetImages(videoPath, inLineCount, allCount, PreviewImageResizePercent);
            //Ext.ScreenList list = Marshal.PtrToStructure<Ext.ScreenList>(IntPtr.Zero);
            List<Ext.ImageBuf> bufInfo = new List<Ext.ImageBuf>();
            List<KeyValuePair<TimeSpan, Image>> imgs = new List<KeyValuePair<TimeSpan, Image>>();
            for (int i = 0; i < list.ImgCount; i++)
            {
                Ext.ImageBuf m_img = Marshal.PtrToStructure<Ext.ImageBuf>(list.bufs + (Marshal.SizeOf<Ext.ImageBuf>()) * i);
                if (m_img.BufSize > 0)
                {
                    //BMPFile bMPFile = new BMPFile(m_pCodecCtx.width, m_pCodecCtx.height, 32);
                    BMPFile bMPFile = new BMPFile(list.Width, list.Height, 32);
                    //byte[] imgBuffer = new byte[m_pCodecCtx.width * m_pCodecCtx.height * 32 / 8];
                    byte[] imgBuffer = new byte[m_img.BufSize];
                    Marshal.Copy(m_img.ImgBuf, imgBuffer, 0, imgBuffer.Length);
                    using (MemoryStream ms = new MemoryStream(bMPFile.CreateFromBuffer(imgBuffer)))
                    {
                        using (Image img = Image.FromStream(ms))
                        {
                            img.RotateFlip(RotateFlipType.RotateNoneFlipY);
                            ms.Close();
                            imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(m_img.ImageTime), (Image)img.Clone()));
                        }
                    }
                }
                else
                {
                    imgs.Add(new KeyValuePair<TimeSpan, Image>(TimeSpan.FromSeconds(-1), null));
                }

            }
            Ext.FreeImageList(list);
            //Marshal.FreeHGlobal(list.bufs);
            List<Bitmap> images = new List<Bitmap>();
            foreach (var item in imgs)
            {
                if (item.Value != null)
                    images.Add(new Bitmap(item.Value));
                else 
                    images.Add(null);
            }
            return new ImageData() { ScreenList = GenerateScreenList(imgs, inLineCount, allCount, list.Period, list.Width, list.Height), AllImages = images };

        }



        private static Image GenerateScreenList(List<KeyValuePair<TimeSpan, Image>> images, int inLineCount, int allCount, double period, int vWdth, int vHeight)
        {
            int p = 0;
            int col = allCount;
            //int period = 1000;
            //int period = Convert.ToInt32(ts.TotalMilliseconds) / col;
            int w = 0, h = 0, column = inLineCount;
            using (Image mm = DrawWithTime(images, vWdth, vHeight, column, period * 1000))
            {
                //Image mm = Draw(images, videoWidth, videoHeight, column);
                //mm = ResizeImg(mm, PreviewImageResizePercent);
                return (Image)mm.Clone();
            }
        }

        private static Image DrawWithTime(List<KeyValuePair<TimeSpan, Image>> images, int Width, int Height, int ColumnsCount, double period)
        {
            TimeSpan wrongTs = TimeSpan.FromSeconds(-1);
            Font drawFont = new Font("Arial", 16, FontStyle.Bold);
            FontFamily fFamily = new FontFamily("Arial");
            SolidBrush drawBrush = new SolidBrush(Color.White);
            SolidBrush fillBrush = new SolidBrush(Color.Black);
            StringFormat drawFormat = new StringFormat();
            //drawFormat.FormatFlags = StringFormatFlags.;
            TimeSpan span = TimeSpan.FromMilliseconds(period);
            int num = 0;
            int otstup = 20;
            int ResWidth = ColumnsCount * Width + ColumnsCount * otstup;
            int rows = images.Count / ColumnsCount;
            int ost = images.Count % ColumnsCount;
            if (ost > 0)
                rows++;
            int ResHeight = rows * Height + rows * otstup;
            int pol = 0;
            using (Bitmap bitmap = new Bitmap(ResWidth, ResHeight))
            {
                using (Graphics g = Graphics.FromImage(bitmap))
                {
                    //g.Clear(Color.Black);
                    g.InterpolationMode = InterpolationMode.NearestNeighbor;
                    g.FillRectangle(fillBrush, new RectangleF(0,0, ResWidth, ResHeight));
                    foreach (var img in images)
                    {
                        Image tmp = img.Value;
                        if (num == ColumnsCount)
                        {
                            pol += Height + otstup;
                            num = 0;
                        }
                        if (tmp != null)
                        {

                            g.DrawImageUnscaled(tmp, Width * num + otstup, pol);
                        }
                        //GraphicsPath gp = new GraphicsPath();
                        //gp.AddString(span.ToString(@"hh\:mm\:ss"), fFamily, (int)FontStyle.Bold, g.DpiY * 20 / 72, new Point(Width * num + otstup, pol), drawFormat);
                        //g.DrawPath(Pens.Black, gp);
                        g.FillRectangle(fillBrush, new Rectangle(new Point(Width * num + otstup, pol), new Size(98, 24)));
                        if (img.Key == wrongTs)
                            g.DrawString(span.ToString(@"hh\:mm\:ss"), drawFont, drawBrush, Width * num + otstup, pol, drawFormat);
                        else
                            g.DrawString(img.Key.ToString(@"hh\:mm\:ss"), drawFont, drawBrush, Width * num + otstup, pol, drawFormat);

                        span = span.Add(TimeSpan.FromMilliseconds(period));
                        num++;
                        if (tmp != null)
                            tmp.Dispose();
                    }
                    g.Dispose();
                }

                return (Bitmap)bitmap.Clone();
            }
        }

        public class ImageData
        {
            public Image ScreenList;
            public List<Bitmap> AllImages;
        }

    }
}
