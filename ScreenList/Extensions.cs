using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Security.Cryptography;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.InteropServices;

namespace ScreenLister
{
    public static class Extensions
    {
        public static void WriteRawSerialize(this BinaryWriter writer, object anything)
        {
            int rawsize = Marshal.SizeOf(anything);
            byte[] rawdata = new byte[rawsize];
            GCHandle handle = GCHandle.Alloc(rawdata, GCHandleType.Pinned);
            Marshal.StructureToPtr(anything, handle.AddrOfPinnedObject(), false);
            handle.Free();
            writer.Write(rawdata);
        }

        public static object ReadStruct(this BinaryReader reader, Type t)
        {
            byte[] buffer = new byte[Marshal.SizeOf(t)];
            reader.BaseStream.Read(buffer, 0, Marshal.SizeOf(t));
            GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            Object temp = Marshal.PtrToStructure(handle.AddrOfPinnedObject(), t);
            handle.Free();
            return temp;
        }

        public static byte[] ReadToEnd(this BinaryReader reader)
        {
            long size = reader.BaseStream.Length - reader.BaseStream.Position;
            if (size == 0)
                return null;
            else if (size < 0)
                throw new OutOfMemoryException("Размер буфера не может быть отрицателен");
            return reader.ReadBytes((int)size);

        }
    }
}
