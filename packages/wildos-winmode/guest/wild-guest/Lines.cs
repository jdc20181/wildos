using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace WildOS.Guest
{
    // One JSON object per line over a stream, both ways. Reading and writing
    // happen on different threads; the stream must be opened for overlapped
    // I/O, or Windows would hold every write until the pending read ends.
    sealed class Lines
    {
        // An icon list is the largest thing that travels; anything bigger is
        // not something Wild OS sent.
        const int MaxLine = 8 * 1024 * 1024;

        readonly Stream stream;
        readonly Stream output;
        readonly object writeLock = new object();
        readonly byte[] buffer = new byte[65536];
        readonly List<byte> pending = new List<byte>();
        int start, end;

        public Lines(Stream stream) : this(stream, stream)
        {
        }

        // A FileStream is not safe to read and write from two threads at
        // once, so the port gets one stream for each direction.
        public Lines(Stream input, Stream output)
        {
            stream = input;
            this.output = output;
        }

        public string ReadLine()
        {
            while (true)
            {
                while (start < end)
                {
                    byte b = buffer[start++];
                    if (b == (byte)'\n')
                    {
                        string line = Encoding.UTF8.GetString(pending.ToArray()).TrimEnd(new[] { '\r' });
                        pending.Clear();
                        return line;
                    }
                    if (pending.Count < MaxLine) pending.Add(b);
                }
                start = 0;
                end = stream.Read(buffer, 0, buffer.Length);
                if (end <= 0) return null;
            }
        }

        public void Write(object message)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(Json.Write(message) + "\n");
            lock (writeLock)
            {
                output.Write(bytes, 0, bytes.Length);
                output.Flush();
            }
        }

        // For the virtio-serial port: a write to a port nobody on Wild OS has
        // open waits in the driver for ever, so one that has not finished
        // in time is cancelled and reported, and the port is opened again.
        public bool Write(object message, int timeoutMs)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(Json.Write(message) + "\n");
            var port = output as Port;
            lock (writeLock)
            {
                if (port != null) return port.TryWrite(bytes, 0, bytes.Length, timeoutMs);
                output.Write(bytes, 0, bytes.Length);
                output.Flush();
                return true;
            }
        }
    }

    // Writes to Wild OS through a queue on a thread of its own, so a host
    // that is not reading never stalls the helper. Nothing is written until
    // Wild OS has spoken on this port in the last minute (it says hello on
    // connecting and pings while connected), since a write nobody reads
    // only waits. What is dropped meanwhile is state Wild OS asks for again
    // when it connects.
    sealed class QueuedWriter
    {
        readonly List<object> queue = new List<object>();
        // The newest window list of each session, kept while Wild OS is not
        // listening and sent the moment it is, so a list that changed while
        // the channel was reconnecting is not lost.
        readonly Dictionary<string, object> held = new Dictionary<string, object>();
        Lines target;
        Port port;
        DateTime heard = DateTime.MinValue;

        public QueuedWriter()
        {
            var thread = new Thread(Run) { IsBackground = true, Name = "host writer" };
            thread.Start();
        }

        public void SetTarget(Lines lines, Port current)
        {
            lock (queue)
            {
                target = lines;
                port = current;
                heard = DateTime.MinValue;
                queue.Clear();
                Monitor.PulseAll(queue);
            }
        }

        public void Heard()
        {
            lock (queue)
            {
                bool was = Listening();
                heard = DateTime.UtcNow;
                if (!was && target != null)
                {
                    foreach (object m in held.Values) queue.Add(m);
                    held.Clear();
                }
                Monitor.PulseAll(queue);
            }
        }

        public double SilentSeconds()
        {
            lock (queue)
            {
                return (DateTime.UtcNow - heard).TotalSeconds;
            }
        }

        bool Listening()
        {
            return target != null && (DateTime.UtcNow - heard).TotalSeconds < 60;
        }

        public void Send(object message)
        {
            lock (queue)
            {
                if (!Listening())
                {
                    var m = message as Dictionary<string, object>;
                    if (m != null && Json.Str(m, "type") == "windows") held[Convert.ToString(m.ContainsKey("session") ? m["session"] : "")] = m;
                    return;
                }
                if (queue.Count >= 500) queue.RemoveAt(0);
                queue.Add(message);
                Monitor.PulseAll(queue);
            }
        }

        void Run()
        {
            while (true)
            {
                object message;
                Lines lines;
                Port p;
                lock (queue)
                {
                    while (queue.Count == 0 || !Listening()) Monitor.Wait(queue, 5000);
                    message = queue[0];
                    queue.RemoveAt(0);
                    lines = target;
                    p = port;
                }
                bool ok;
                try { ok = lines.Write(message, 5000); }
                catch (Exception e)
                {
                    Log.Write("write to host failed: " + e.Message);
                    ok = false;
                }
                if (!ok)
                {
                    lock (queue)
                    {
                        if (target == lines)
                        {
                            heard = DateTime.MinValue;
                            queue.Clear();
                            if (p != null) p.CancelAll();
                        }
                    }
                }
            }
        }
    }

    static class Log
    {
        static readonly object gate = new object();
        public static string FilePath = @"C:\ProgramData\WildOS\wild-guest.log";

        public static void Write(string line)
        {
            lock (gate)
            {
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
                    var info = new FileInfo(FilePath);
                    if (info.Exists && info.Length > 1024 * 1024)
                    {
                        File.Copy(FilePath, FilePath + ".1", true);
                        File.Delete(FilePath);
                    }
                    File.AppendAllText(FilePath, DateTime.Now.ToString("s") + " " + line + Environment.NewLine);
                }
                catch (Exception)
                {
                }
            }
        }
    }
}
