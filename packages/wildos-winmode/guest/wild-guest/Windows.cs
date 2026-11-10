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
            w["title"] = title.ToString();
            w["class"] = cls.ToString();
            w["owner"] = owner.ToInt64();
            w["tool"] = (ex & Native.WS_EX_TOOLWINDOW) != 0;
            w["visible"] = visible && cloaked == 0;
            w["width"] = (long)width;
            w["height"] = (long)height;
            return w;
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
