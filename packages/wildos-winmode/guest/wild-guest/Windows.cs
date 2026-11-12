using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace WildOS.Guest
{
    // The top-level windows of this session that RemoteApp shows on Wild OS,
    // with the program behind each. Wild OS matches them to its own windows
    // by handle: FreeRDP names each RemoteApp window "RAIL:<handle>".
    static class Windows
    {
        // Parts of Windows that own windows in every RemoteApp session but are
        // never an app the person opened.
        static readonly List<string> shell = new List<string>
        {
            "rdpshell", "rdpinit", "rdpclip", "wild-guest", "textinputhost", "ctfmon", "dwm", "explorer",
            "shellexperiencehost", "startmenuexperiencehost", "searchhost", "lockapp", "applicationframehost", "shellappruntime",
        };

        // Except these windows of theirs, which the person did open: a folder
        // window, a copy in progress and Explorer's dialogs, and a packaged
        // app's frame (Calculator, Settings; a suspended one is cloaked and
        // left out already). Counted as apps, Windows is never paused under
        // them.
        static readonly List<string> openedClasses = new List<string>
        {
            "CabinetWClass", "OperationStatusWindow", "#32770", "ApplicationFrameWindow",
        };

        public static List<object> List()
        {
            var result = new List<object>();
            Native.EnumWindows(delegate (IntPtr hwnd, IntPtr param)
            {
                var w = Describe(hwnd, true);
                if (w != null) result.Add(w);
                return true;
            }, IntPtr.Zero);
            return result;
        }

        // filter: leave out what RemoteApp does not show or what belongs to
        // Windows itself. A direct query about one handle reports it anyway.
        public static Dictionary<string, object> Describe(IntPtr hwnd, bool filter)
        {
            if (!Native.IsWindow(hwnd)) return null;
            bool visible = Native.IsWindowVisible(hwnd);
            int cloaked = 0;
            try { Native.DwmGetWindowAttribute(hwnd, Native.DWMWA_CLOAKED, out cloaked, 4); } catch (DllNotFoundException) { }
            long ex = Native.GetWindowLong(hwnd, Native.GWL_EXSTYLE);
            IntPtr owner = Native.GetWindow(hwnd, Native.GW_OWNER);
            Native.RECT rect;
            Native.GetWindowRect(hwnd, out rect);
            int width = rect.Right - rect.Left, height = rect.Bottom - rect.Top;
            if (filter && (!visible || cloaked != 0 || width <= 1 || height <= 1)) return null;

            uint pid;
            Native.GetWindowThreadProcessId(hwnd, out pid);
            string image = ImagePath(pid);
            string app = image == null ? "" : Path.GetFileNameWithoutExtension(image).ToLowerInvariant();
            var cls = new StringBuilder(256);
            Native.GetClassName(hwnd, cls, cls.Capacity);
            if (filter && shell.Contains(app)
                && !((app == "explorer" || app == "applicationframehost") && openedClasses.Contains(cls.ToString()))) return null;

            int length = Native.GetWindowTextLength(hwnd);
            var title = new StringBuilder(Math.Max(length + 1, 1));
            Native.GetWindowText(hwnd, title, title.Capacity);

            var w = new Dictionary<string, object>();
            w["hwnd"] = hwnd.ToInt64();
            w["pid"] = (long)pid;
            w["image"] = image;
            w["app"] = app;
            // A frame-hosted Store app (Calculator, Settings) has its frame
            // owned by ApplicationFrameHost; the AUMID says which app it is.
            if (app == "applicationframehost")
            {
                string id = HostedAumid(hwnd, pid);
                if (id != null) w["aumid"] = id;
            }
            w["title"] = title.ToString();
            w["class"] = cls.ToString();
            w["owner"] = owner.ToInt64();
            w["tool"] = (ex & Native.WS_EX_TOOLWINDOW) != 0;
            w["visible"] = visible && cloaked == 0;
            w["width"] = (long)width;
            w["height"] = (long)height;
            return w;
        }

        // The app drawing inside a packaged app's frame: its window (a
        // CoreWindow) from a process of its own, whose package names the
        // app. Windows keeps that window inside the frame only while the
        // frame is active, and as a top-level window of the same title
        // otherwise.
        static string HostedAumid(IntPtr frame, uint framePid)
        {
            string found = null;
            Native.EnumChildWindows(frame, delegate (IntPtr child, IntPtr param)
            {
                uint pid;
                Native.GetWindowThreadProcessId(child, out pid);
                if (pid != 0 && pid != framePid) found = ProcessAumid(pid);
                return found == null;
            }, IntPtr.Zero);
            if (found != null) return found;
            string title = Title(frame);
            if (title.Length == 0) return null;
            Native.EnumWindows(delegate (IntPtr hwnd, IntPtr param)
            {
                var cls = new StringBuilder(64);
                Native.GetClassName(hwnd, cls, cls.Capacity);
                if (cls.ToString() != "Windows.UI.Core.CoreWindow" || Title(hwnd) != title) return true;
                uint pid;
                Native.GetWindowThreadProcessId(hwnd, out pid);
                if (pid != 0 && pid != framePid) found = ProcessAumid(pid);
                return found == null;
            }, IntPtr.Zero);
            return found;
        }

        static string Title(IntPtr hwnd)
        {
            var title = new StringBuilder(Math.Max(Native.GetWindowTextLength(hwnd) + 1, 1));
            Native.GetWindowText(hwnd, title, title.Capacity);
            return title.ToString();
        }

        static string ProcessAumid(uint pid)
        {
            IntPtr process = Native.OpenProcess(Native.PROCESS_QUERY_LIMITED_INFORMATION, false, pid);
            if (process == IntPtr.Zero) return null;
            try
            {
                uint length = 256;
                var id = new StringBuilder((int)length);
                return Native.GetApplicationUserModelId(process, ref length, id) == 0 && Packages.IsAumid(id.ToString()) ? id.ToString() : null;
            }
            finally
            {
                Native.CloseHandle(process);
            }
        }

        public static string ImagePath(uint pid)
        {
            IntPtr process = Native.OpenProcess(Native.PROCESS_QUERY_LIMITED_INFORMATION, false, pid);
            if (process == IntPtr.Zero) return null;
            try
            {
                var name = new StringBuilder(1024);
                uint size = (uint)name.Capacity;
                return Native.QueryFullProcessImageName(process, 0, name, ref size) ? name.ToString() : null;
            }
            finally
            {
                Native.CloseHandle(process);
            }
        }

        // A short fingerprint, so a change is only reported when something
        // Wild OS cares about changed.
        public static string Fingerprint(List<object> windows)
        {
            var sb = new StringBuilder();
            foreach (Dictionary<string, object> w in windows)
            {
                sb.Append(w["hwnd"]).Append('|').Append(w["title"]).Append('|').Append(w["owner"]).Append('\n');
            }
            return sb.ToString();
        }
    }
}
