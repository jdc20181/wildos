using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

namespace WildOS.Guest
{
    // The session helper: "wild-guest.exe --session", started by the
    // service in each session of the Windows mode account. It lists that
    // session's windows, starts programs inside it, so every app after the
    // first joins the one RemoteApp connection Wild OS holds, and applies
    // Wild OS's colours. It has no window of its own, so RemoteApp never
    // shows it and Windows still ends the session's apps as it normally
    // would.
    static class SessionHelper
    {
        // The account's own folder: the session runs with its ordinary,
        // non-elevated rights, which cannot write to ProgramData\WildOS.
        static readonly string SessionLog = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"WildOS\wild-guest-session.log");

        static Lines lines;
        static WarmPool pool;
        static string lastFingerprint;

        public static int Run()
        {
            Log.FilePath = SessionLog;
            NamedPipeClientStream pipe = null;
            for (int attempt = 0; attempt < 60 && pipe == null; attempt++)
            {
                try
                {
                    var p = new NamedPipeClientStream(".", "wildos-guest", PipeDirection.InOut, PipeOptions.Asynchronous);
                    p.Connect(2000);
                    pipe = p;
                }
                catch (Exception)
                {
                    Thread.Sleep(1000);
                }
            }
            if (pipe == null) return 1;
            lines = new Lines(pipe);
            Log.Write("session helper connected");
            Send(Message("session-hello", "pid", (long)Process.GetCurrentProcess().Id));
            try { Theme.Apply(Theme.Load()); } catch (Exception e) { Log.Write("theme: " + e.Message); }
            pool = new WarmPool();
            new Thread(Watch) { IsBackground = true, Name = "windows" }.Start();
            try
            {
                string line;
                while ((line = lines.ReadLine()) != null)
                {
                    Dictionary<string, object> m;
                    try { m = Json.ParseObject(line); }
                    catch (Exception) { continue; }
                    if (m != null) Handle(m);
                }
            }
            catch (Exception e)
            {
                Log.Write("pipe closed: " + e.Message);
            }
            return 0;
        }

        static Dictionary<string, object> Message(string type, params object[] pairs)
        {
            var m = new Dictionary<string, object>();
            m["type"] = type;
            for (int i = 0; i + 1 < pairs.Length; i += 2) m[(string)pairs[i]] = pairs[i + 1];
            return m;
        }

        static void Send(Dictionary<string, object> m)
        {
            try { lines.Write(m); }
            catch (Exception) { Environment.Exit(0); }
        }

        // Once a second is plenty for noticing that the last app closed and
        // costs nothing measurable; Wild OS asks about a new window directly
        // the moment it sees one, so taskbar grouping does not wait for it.
        static void Watch()
        {
            OnConnect(false);
            long connected = ConnectTime();
            while (true)
            {
                long now = ConnectTime();
                if (now != connected)
                {
                    connected = now;
                    OnConnect(true);
                }
                try
                {
                    var list = Windows.List();
                    string fp = Windows.Fingerprint(list);
                    if (fp != lastFingerprint)
                    {
                        lastFingerprint = fp;
                        Send(Message("windows", "windows", list));
                    }
                }
                catch (Exception e)
                {
                    Log.Write("windows: " + e.Message);
                }
                Thread.Sleep(1000);
            }
        }

        static readonly object connecting = new object();

        // Windows does not send a reconnecting RemoteApp client the windows
        // already on screen, so they arrive blank until they next change.
        // Every window is redrawn whenever a client connects (the session's
        // connect time moves, which also catches a connection taking over one
        // Windows still thinks is open), and once more a moment later for
        // apps that paint late. Mapping the drive can take up to 15 seconds,
        // so all of it runs beside the window list, which must keep going:
        // Wild OS reads from it whether an app is still open.
        static void OnConnect(bool redraw)
        {
            var t = new Thread(delegate ()
            {
                lock (connecting)
                {
                    MapDrive();
                    if (!redraw) return;
                    Thread.Sleep(500);
                    RedrawAll();
                    Thread.Sleep(1500);
                    RedrawAll();
                }
            });
            t.IsBackground = true;
            t.Start();
        }

        // Z: is the folders Wild OS shares through the RemoteApp connection
        // (FreeRDP's /drive:wildos), which exists only while it is connected,
        // so it is mapped again on every connect. \\tsclient appears a moment
        // after the connection does.
        static void MapDrive()
        {
            string drive = "Z:";
            try
            {
                var settings = Json.ParseObject(File.ReadAllText(@"C:\ProgramData\WildOS\settings.json"));
                string d = Json.Str(settings, "drive");
                if (!string.IsNullOrEmpty(d)) drive = d + ":";
            }
            catch (Exception) { }
            string share = @"\\tsclient\wildos";
            var current = new StringBuilder(260);
            int length = current.Capacity;
            if (Native.WNetGetConnection(drive, current, ref length) == 0 &&
                string.Equals(current.ToString(), share, StringComparison.OrdinalIgnoreCase)) return;
            Native.WNetCancelConnection2(drive, 0, true);
            var resource = new Native.NETRESOURCE { dwType = Native.RESOURCETYPE_DISK, lpLocalName = drive, lpRemoteName = share };
            int result = 0;
            for (int attempt = 0; attempt < 15; attempt++)
            {
                result = Native.WNetAddConnection2(resource, null, null, 0);
                if (result == 0)
                {
                    Log.Write("mapped " + drive + " to " + share);
                    MakeAutoRecover(drive);
                    return;
                }
                Thread.Sleep(1000);
            }
            Log.Write("couldn't map " + drive + " to " + share + ": error " + result);
        }

        // Setup points Office's AutoRecover at this hidden folder but can't
        // make it, since the drive doesn't exist until a connection does.
        static void MakeAutoRecover(string drive)
        {
            try
            {
                string documents = drive + @"\Documents";
                if (!Directory.Exists(documents)) return;
                var dir = Directory.CreateDirectory(documents + @"\.autorecover");
                dir.Attributes |= FileAttributes.Hidden;
            }
            catch (Exception e)
            {
                Log.Write("autorecover folder: " + e.Message);
            }
        }

        static long ConnectTime()
        {
            IntPtr buffer;
            uint bytes;
            if (!Native.WTSQuerySessionInformation(IntPtr.Zero, Native.WTS_CURRENT_SESSION, Native.WTSSessionInfo, out buffer, out bytes)) return 0;
            try
            {
                if (bytes < Native.WTSINFO_ConnectTime + 8) return 0;
                long time;
                Native.CopyLong(out time, new IntPtr(buffer.ToInt64() + Native.WTSINFO_ConnectTime), 8UL);
                return time;
            }
            finally { Native.WTSFreeMemory(buffer); }
        }

        static void RedrawAll()
        {
            Native.EnumWindows((hwnd, param) =>
            {
                if (Native.IsWindowVisible(hwnd))
                    Native.RedrawWindow(hwnd, IntPtr.Zero, IntPtr.Zero,
                        Native.RDW_INVALIDATE | Native.RDW_ERASE | Native.RDW_FRAME | Native.RDW_ALLCHILDREN | Native.RDW_UPDATENOW);
                return true;
            }, IntPtr.Zero);
        }

        static void Handle(Dictionary<string, object> m)
        {
            object id;
            m.TryGetValue("id", out id);
            string type = Json.Str(m, "type");
            Dictionary<string, object> reply;
            try
            {
                switch (type)
                {
                    case "windows":
                        reply = Message("windows", "windows", Windows.List());
                        break;
                    case "window":
                        reply = Message("window", "window", Windows.Describe(new IntPtr(Json.Num(m, "hwnd", 0)), false));
                        break;
                    case "running":
                        reply = Running(m);
                        break;
                    case "launch":
                        reply = LaunchWarm(m) ?? Launch(m);
                        break;
                    case "warm":
                        pool.SetWanted(Json.List(m, "apps"), (int)Json.Num(m, "limit", 0));
                        reply = Message("warm", "ok", true);
                        break;
                    case "warm-list":
                        reply = Message("warm-list", "apps", pool.Describe());
                        break;
                    case "documents":
                        // Office busy past the wait still leaves the window
                        // titles to go by; office is then null, not empty.
                        List<object> office = null;
                        try { office = pool.OfficeDocuments(); }
                        catch (Exception e) { Log.Write("documents: " + e.Message); }
                        reply = Message("documents", "office", office, "windows", Windows.List());
                        break;
                    case "theme":
                        Theme.Apply(m);
                        reply = Message("theme", "ok", true, "applied", true);
                        break;
                    default:
                        reply = Message("error", "error", "unknown type " + type);
                        break;
                }
            }
            catch (Exception e)
            {
                reply = Message("error", "error", e.Message);
            }
            reply["id"] = id;
            Send(reply);
        }

        static Dictionary<string, object> LaunchWarm(Dictionary<string, object> m)
        {
            if (pool == null || !string.IsNullOrEmpty(Json.Str(m, "arguments"))) return null;
            var args = new List<string>();
            var list = Json.List(m, "args");
            if (list != null) foreach (object a in list) args.Add(a as string ?? "");
            try { return pool.TryLaunch(Json.Str(m, "path"), args); }
            catch (Exception e)
            {
                Log.Write("warm launch: " + e.Message);
                return null;
            }
        }

        public static int StartOnce(string encoded)
        {
            Log.FilePath = SessionLog;
            try
            {
                var m = Json.ParseObject(Encoding.UTF8.GetString(Convert.FromBase64String(encoded)));
                var result = Launch(m);
                if (Json.Str(result, "type") == "error") Log.Write("start: " + Json.Str(result, "error"));
                return Json.Str(result, "type") == "launched" ? 0 : 1;
            }
            catch (Exception e)
            {
                Log.Write("start: " + e.Message);
                return 1;
            }
        }

        // When each program was last started, kept beside the session's log:
        // a program started through Remote Desktop's own request (StartOnce,
        // a process of its own) and one started here both leave it, so
        // "running" can say a program started and has already ended.
        static string StartedFile(string path)
        {
            return Path.Combine(Path.GetDirectoryName(SessionLog), "started-" + Path.GetFileNameWithoutExtension(path ?? "").ToLowerInvariant());
        }

        static void Started(string path)
        {
            try { File.WriteAllText(StartedFile(path), DateTime.UtcNow.Ticks.ToString()); }
            catch (Exception) { }
        }

        static bool StartedSince(string path, DateTime since)
        {
            try { return new DateTime(long.Parse(File.ReadAllText(StartedFile(path))), DateTimeKind.Utc) >= since; }
            catch (Exception) { return false; }
        }

        // Whether a program started since a moment (milliseconds since 1970,
        // UTC) is still running in this session: by its file name, so a
        // launcher that hands over to the real program of the same name
        // (System32's notepad.exe, an app alias) still counts.
        static Dictionary<string, object> Running(Dictionary<string, object> m)
        {
            string stem = Path.GetFileNameWithoutExtension(Json.Str(m, "path") ?? "");
            DateTime since = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).AddMilliseconds(Json.Num(m, "since", 0)).AddSeconds(-2);
            int session = Process.GetCurrentProcess().SessionId;
            long count = 0;
            if (stem.Length > 0)
            {
                foreach (var p in Process.GetProcessesByName(stem))
                {
                    try
                    {
                        if (p.SessionId == session && p.StartTime.ToUniversalTime() >= since) count++;
                    }
                    catch (Exception) { }
                }
            }
            return Message("running", "running", count > 0, "count", count, "started", StartedSince(Json.Str(m, "path"), since));
        }

        static Dictionary<string, object> Launch(Dictionary<string, object> m)
        {
            string path = Json.Str(m, "path");
            if (string.IsNullOrEmpty(path)) return Message("error", "error", "no program given");
            var args = new StringBuilder();
            var list = Json.List(m, "args");
            if (list != null)
            {
                foreach (object a in list)
                {
                    if (args.Length > 0) args.Append(' ');
                    args.Append(Quote(a as string ?? ""));
                }
            }
            string raw = Json.Str(m, "arguments");
            if (!string.IsNullOrEmpty(raw)) args.Insert(0, raw + (args.Length > 0 ? " " : ""));
            var start = new ProcessStartInfo(path, args.ToString())
            {
                UseShellExecute = true,
                WorkingDirectory = Json.Str(m, "cwd") ?? (Path.GetDirectoryName(path) ?? ""),
            };
            try
            {
                var p = Process.Start(start);
                Started(path);
                return Message("launched", "pid", p == null ? 0L : (long)p.Id, "warm", false);
            }
            catch (Win32Exception e)
            {
                return Message("error", "error", e.Message, "code", (long)e.NativeErrorCode);
            }
        }

        // The quoting CommandLineToArgvW undoes, so each argument reaches
        // the program as one, spaces and all.
        public static string Quote(string arg)
        {
            if (arg.Length > 0 && arg.IndexOfAny(new[] { ' ', '\t', '"' }) < 0) return arg;
            var sb = new StringBuilder("\"");
            int slashes = 0;
            foreach (char c in arg)
            {
                if (c == '\\') { slashes++; continue; }
                if (c == '"') sb.Append('\\', slashes * 2 + 1).Append('"');
                else sb.Append('\\', slashes).Append(c);
                slashes = 0;
            }
            sb.Append('\\', slashes * 2).Append('"');
            return sb.ToString();
        }
    }
}
