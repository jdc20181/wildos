using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.Win32.SafeHandles;

namespace WildOS.Guest
{
    // The virtio-serial port as a stream, with overlapped ReadFile and
    // WriteFile of its own. A read and a write can each be waiting at the
    // same time without sharing any state, a write can give up after a
    // while, and either can be cancelled on its own. (A FileStream cannot
    // be read and written from two threads, and .NET will not put two on
    // one handle.)
    sealed class Port : Stream
    {
        const int ERROR_IO_PENDING = 997;
        const uint WAIT_OBJECT_0 = 0;

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool ReadFile(SafeFileHandle file, IntPtr buffer, int count, IntPtr read, IntPtr overlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool WriteFile(SafeFileHandle file, IntPtr buffer, int count, IntPtr written, IntPtr overlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetOverlappedResult(SafeFileHandle file, IntPtr overlapped, out int transferred, bool wait);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool CancelIoEx(SafeFileHandle file, IntPtr overlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        public static Port Open(string name, out int error)
        {
            var handle = Native.CreateFile(name, Native.GENERIC_READ | Native.GENERIC_WRITE, 0, IntPtr.Zero, Native.OPEN_EXISTING,
                Native.FILE_ATTRIBUTE_NORMAL | 0x40000000, IntPtr.Zero);
            error = handle.IsInvalid ? Marshal.GetLastWin32Error() : 0;
            return handle.IsInvalid ? null : new Port(handle);
        }

        readonly SafeFileHandle handle;

        Port(SafeFileHandle handle)
        {
            this.handle = handle;
        }

        // One transfer: returns the bytes moved, or -1 when it did not finish
        // within timeoutMs and was cancelled.
        int Transfer(bool write, byte[] data, int offset, int count, int timeoutMs)
        {
            // OVERLAPPED is 32 bytes on 64-bit Windows, 20 on 32-bit; its
            // event handle is the last field.
            int size = IntPtr.Size == 8 ? 32 : 20;
            IntPtr overlapped = Marshal.AllocHGlobal(size);
            var done = new ManualResetEvent(false);
            GCHandle pin = GCHandle.Alloc(data, GCHandleType.Pinned);
            try
            {
                for (int i = 0; i < size; i++) Marshal.WriteByte(overlapped, i, 0);
                Marshal.WriteIntPtr(overlapped, size - IntPtr.Size, done.SafeWaitHandle.DangerousGetHandle());
                IntPtr buffer = new IntPtr(pin.AddrOfPinnedObject().ToInt64() + offset);
                bool ok = write ? WriteFile(handle, buffer, count, IntPtr.Zero, overlapped) : ReadFile(handle, buffer, count, IntPtr.Zero, overlapped);
                if (!ok && Marshal.GetLastWin32Error() != ERROR_IO_PENDING)
                    throw new IOException((write ? "write" : "read") + " failed (" + Marshal.GetLastWin32Error() + ")");
                bool cancelled = false;
                if (timeoutMs >= 0 && WaitForSingleObject(done.SafeWaitHandle.DangerousGetHandle(), (uint)timeoutMs) != WAIT_OBJECT_0)
                {
                    CancelIoEx(handle, overlapped);
                    cancelled = true;
                }
                int moved;
                // Waits for the operation, or for its cancellation, to end:
                // the buffer and OVERLAPPED must outlive it.
                if (!GetOverlappedResult(handle, overlapped, out moved, true))
                {
                    if (cancelled) return -1;
                    throw new IOException((write ? "write" : "read") + " ended (" + Marshal.GetLastWin32Error() + ")");
                }
                return moved;
            }
            finally
            {
                pin.Free();
                done.Close();
                Marshal.FreeHGlobal(overlapped);
            }
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            return Transfer(false, buffer, offset, count, -1);
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            if (!TryWrite(buffer, offset, count, -1)) throw new IOException("write cancelled");
        }

        public bool TryWrite(byte[] buffer, int offset, int count, int timeoutMs)
        {
            while (count > 0)
            {
                int moved = Transfer(true, buffer, offset, count, timeoutMs);
                if (moved < 0) return false;
                offset += moved;
                count -= moved;
            }
            return true;
        }

        // Ends a read that is waiting, so the port can be opened afresh.
        public void CancelAll()
        {
            CancelIoEx(handle, IntPtr.Zero);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) handle.Dispose();
            base.Dispose(disposing);
        }

        public override bool CanRead { get { return true; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return true; } }
        public override long Length { get { throw new NotSupportedException(); } }
        public override long Position { get { throw new NotSupportedException(); } set { throw new NotSupportedException(); } }
        public override void Flush() { }
        public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
        public override void SetLength(long value) { throw new NotSupportedException(); }
    }
}
