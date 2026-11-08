using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace WildOS.Guest
{
    // A program's own icon as an .ico file, built from its RT_GROUP_ICON and
    // RT_ICON resources, so every size it carries (256 px included) reaches
    // Wild OS. The Shell's ExtractAssociatedIcon gives 32 px only.
    static class Icons
    {
        public static byte[] FromExecutable(string path)
        {
            IntPtr module = Native.LoadLibraryEx(path, IntPtr.Zero, Native.LOAD_LIBRARY_AS_DATAFILE | Native.LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (module == IntPtr.Zero) return null;
            try
            {
                IntPtr group = IntPtr.Zero;
                Native.EnumResourceNames(module, Native.RT_GROUP_ICON, delegate (IntPtr m, IntPtr type, IntPtr name, IntPtr param)
                {
                    group = name;
                    return false;
                }, IntPtr.Zero);
                if (group == IntPtr.Zero) return null;
                byte[] dir = Resource(module, group, Native.RT_GROUP_ICON);
                if (dir == null || dir.Length < 6) return null;
                int count = BitConverter.ToUInt16(dir, 4);
                var images = new List<byte[]>();
                var entries = new List<byte[]>();
                for (int i = 0; i < count; i++)
                {
                    int at = 6 + i * 14;
                    if (at + 14 > dir.Length) break;
                    ushort id = BitConverter.ToUInt16(dir, at + 12);
                    byte[] image = Resource(module, new IntPtr(id), Native.RT_ICON);
                    if (image == null) continue;
                    var entry = new byte[12];
                    Array.Copy(dir, at, entry, 0, 12);
                    entries.Add(entry);
                    images.Add(image);
                }
                if (images.Count == 0) return null;
                // GRPICONDIRENTRY carries a resource id where ICONDIRENTRY
                // carries a file offset; everything else is the same.
                using (var ms = new MemoryStream())
                using (var w = new BinaryWriter(ms))
                {
                    w.Write((ushort)0);
                    w.Write((ushort)1);
                    w.Write((ushort)images.Count);
                    int offset = 6 + 16 * images.Count;
                    for (int i = 0; i < images.Count; i++)
                    {
                        w.Write(entries[i], 0, 8);
                        w.Write((uint)images[i].Length);
                        w.Write((uint)offset);
                        offset += images[i].Length;
                    }
                    foreach (byte[] image in images) w.Write(image);
                    w.Flush();
                    return ms.ToArray();
                }
            }
            finally
            {
                Native.FreeLibrary(module);
            }
        }

        static byte[] Resource(IntPtr module, IntPtr name, IntPtr type)
        {
            IntPtr info = Native.FindResource(module, name, type);
            if (info == IntPtr.Zero) return null;
            uint size = Native.SizeofResource(module, info);
            IntPtr data = Native.LoadResource(module, info);
            if (data == IntPtr.Zero || size == 0) return null;
            IntPtr ptr = Native.LockResource(data);
            if (ptr == IntPtr.Zero) return null;
            var bytes = new byte[size];
            Marshal.Copy(ptr, bytes, 0, (int)size);
            return bytes;
        }
    }
}
