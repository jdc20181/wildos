using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;

namespace WildOS.Guest
{
    // Microsoft Store apps (packaged apps: the new Outlook, WhatsApp, Teams,
    // Microsoft 365 from the Store). They have no Start menu shortcut; the
    // Start menu lists them from the Shell's Applications folder
    // (shell:AppsFolder), and so does this, in the session helper: the
    // folder and the package APIs only answer for the account that is
    // signed in. Each is started by its Application User Model ID through
    // Windows' own activation, as Start does: a packaged program started by
    // its path runs without its package and breaks.
    static class Packages
    {
        public const string AppsFolder = @"shell:AppsFolder\";

        // Family name (name, underscore, 13 character publisher id), "!",
        // and the app's id within the package, as the manifest rules allow.
        static readonly Regex aumid = new Regex(@"^([A-Za-z0-9.\-]{3,50}_[a-z0-9]{13})!([A-Za-z][A-Za-z0-9.\-]{0,63})$",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        // A package's folder: Name_Version_Architecture_ResourceId_PublisherId.
        static readonly Regex packageFolder = new Regex(@"^([A-Za-z0-9.\-]{3,50})_[0-9.]+_[a-z0-9]*_[A-Za-z0-9.\-~]*_([a-z0-9]{13})$",
            RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

        public static bool IsAumid(string id)
        {
            return id != null && aumid.IsMatch(id);
        }

        // The AUMID a launch path names: "shell:AppsFolder\<AUMID>", as
        // Start's own pins do, or a program inside an installed package.
        // Null for anything else.
        public static string AumidFor(string path)
        {
            if (string.IsNullOrEmpty(path)) return null;
            if (path.StartsWith(AppsFolder, StringComparison.OrdinalIgnoreCase))
            {
                string id = path.Substring(AppsFolder.Length);
                return IsAumid(id) ? id : null;
            }
            string full = Paths.Local(path);
            return full == null ? null : AumidForProgram(full);
        }

        // A program inside a package folder, by the package's family: the
        // folder's version may be one the Store has since replaced.
        static string AumidForProgram(string full)
        {
            string dir = Path.GetDirectoryName(full);
            for (int depth = 0; dir != null && depth < 8; depth++, dir = Path.GetDirectoryName(dir))
            {
                Match m = packageFolder.Match(Path.GetFileName(dir) ?? "");
                if (!m.Success) continue;
                string family = m.Groups[1].Value + "_" + m.Groups[2].Value;
                string relative = full.Substring(dir.Length).TrimStart(new[] { '\\' });
                foreach (string root in Roots(family))
                {
                    // The app Start lists, when the program also serves one
                    // it doesn't (a background task, a share target).
                    Manifest.App pick = null;
                    foreach (Manifest.App app in Manifest.Read(root).Apps)
                    {
                        if (!string.Equals((app.Executable ?? "").Replace('/', '\\'), relative, StringComparison.OrdinalIgnoreCase)) continue;
                        if (pick == null || (!pick.Listed && app.Listed)) pick = app;
                    }
                    if (pick != null) return family + "!" + pick.Id;
                }
                return null;
            }
            return null;
        }

        // Starts a packaged app, as Start does. Returns the process id.
        public static uint Activate(string id, string arguments)
        {
            if (!IsAumid(id)) throw new ArgumentException("not an app id");
            var manager = (IApplicationActivationManager)Activator.CreateInstance(Type.GetTypeFromCLSID(ActivationManagerClsid));
            try
            {
                uint pid;
                int hr = manager.ActivateApplication(id, string.IsNullOrEmpty(arguments) ? null : arguments, 0, out pid);
                if (hr < 0) Marshal.ThrowExceptionForHR(hr);
                return pid;
            }
            finally
            {
                Marshal.ReleaseComObject(manager);
            }
        }

        // The folders the account's installed package of this family is in,
        // the main package first; empty when it is not installed.
        public static List<string> Roots(string family)
        {
            var roots = new List<string>();
            foreach (string name in FullNames(family))
            {
                string root = PathOf(name);
                if (root != null && !roots.Contains(root)) roots.Add(root);
            }
            return roots;
        }

        public static string Family(string id)
        {
            Match m = aumid.Match(id ?? "");
            return m.Success ? m.Groups[1].Value : null;
        }

        static List<string> FullNames(string family)
        {
            var names = new List<string>();
            uint count = 0, length = 0;
            int error = Native.GetPackagesByPackageFamily(family, ref count, IntPtr.Zero, ref length, IntPtr.Zero);
            if (error != Native.ERROR_INSUFFICIENT_BUFFER || count == 0 || length == 0 || count > 64 || length > 65536) return names;
            IntPtr pointers = Marshal.AllocHGlobal((int)count * IntPtr.Size);
            IntPtr buffer = Marshal.AllocHGlobal((int)length * 2);
            try
            {
                if (Native.GetPackagesByPackageFamily(family, ref count, pointers, ref length, buffer) != 0) return names;
                for (int i = 0; i < count; i++) names.Add(Marshal.PtrToStringUni(Marshal.ReadIntPtr(pointers, i * IntPtr.Size)));
            }
            finally
            {
                Marshal.FreeHGlobal(pointers);
                Marshal.FreeHGlobal(buffer);
            }
            // The main package before its resource packages, whose names
            // carry a resource id ("split.scale-200") after the architecture.
            names.Sort(delegate (string a, string b) { return Rank(a).CompareTo(Rank(b)); });
            return names;
        }

        static int Rank(string fullName)
        {
            string[] parts = fullName.Split(new[] { '_' });
            return parts.Length == 5 && parts[3].Length > 0 && parts[3] != "~" ? 1 : 0;
        }

        static string PathOf(string fullName)
        {
            uint length = 0;
            Native.GetPackagePathByFullName(fullName, ref length, null);
            if (length == 0 || length > 4096) return null;
            var path = new StringBuilder((int)length);
            if (Native.GetPackagePathByFullName(fullName, ref length, path) != 0) return null;
            return Paths.Local(path.ToString());
        }

        // Start's packaged apps for the account this runs as, each as
        // {name, target, arguments, aumid, package, icon}. target is the
        // program the app runs, so its windows and processes are known as
        // the app's, and a launch of it starts the app; when two apps share
        // one program, the app's own shell:AppsFolder address instead.
        public static List<object> List(bool icons)
        {
            var result = new List<object>();
            string windir = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
            var byProgram = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var found = new List<Dictionary<string, object>>();
            foreach (KeyValuePair<string, string> item in Shell.AppsFolderItems())
            {
                string id = item.Key, name = (item.Value ?? "").Trim();
                string family = Family(id);
                if (family == null || name.Length == 0) continue;
                List<string> roots = Roots(family);
                // Not installed for this account (a desktop program's own
                // AUMID can look like one), or part of Windows itself.
                if (roots.Count == 0 || Paths.Inside(roots[0], windir)) continue;
                Manifest manifest = Manifest.Read(roots[0]);
                Manifest.App app = manifest.Find(id.Substring(family.Length + 1));
                if (app == null || manifest.Framework || manifest.Resource) continue;
                string program = null;
                if (!string.IsNullOrEmpty(app.Executable))
                {
                    string p = Paths.Local(Path.Combine(roots[0], app.Executable.Replace('/', '\\')));
                    if (p != null && Paths.Inside(p, roots[0])) program = p;
                }
                var entry = new Dictionary<string, object>();
                entry["name"] = name;
                entry["target"] = program;
                entry["arguments"] = "";
                entry["aumid"] = id;
                entry["package"] = family;
                if (icons)
                {
                    byte[] png = null;
                    try { png = Logo(roots, app.Logo); } catch (Exception) { }
                    entry["icon"] = png == null ? null : Convert.ToBase64String(Icons.FromPng(png));
                }
                if (program != null) byProgram[program] = byProgram.ContainsKey(program) ? byProgram[program] + 1 : 1;
                found.Add(entry);
            }
            foreach (var entry in found)
            {
                string program = entry["target"] as string;
                if (program == null || byProgram[program] > 1) entry["target"] = AppsFolder + entry["aumid"];
                result.Add(entry);
            }
            return result;
        }

        // The app's logo at the largest size up to 256 px, preferring the
        // plain one Windows shows on the taskbar ("altform-unplated") to
        // the one drawn on a coloured plate. Windows keeps each size and
        // scale beside the name the manifest gives (Assets\Logo.png becomes
        // Assets\Logo.targetsize-256_altform-unplated.png, or
        // Assets\scale-200\Logo.png), in the main package or a resource
        // package of the same family.
        static byte[] Logo(List<string> roots, string logo)
        {
            if (string.IsNullOrEmpty(logo)) return null;
            logo = logo.Replace('/', '\\').TrimStart(new[] { '\\' });
            if (!logo.EndsWith(".png", StringComparison.OrdinalIgnoreCase)) return null;
            string folder = Path.GetDirectoryName(logo) ?? "";
            string stem = Path.GetFileNameWithoutExtension(logo);
            string best = null;
            long bestScore = long.MinValue;
            foreach (string root in roots)
            {
                string dir = Paths.Local(Path.Combine(root, folder));
                if (dir == null || !(Paths.Inside(dir, root) || string.Equals(dir, root, StringComparison.OrdinalIgnoreCase)) || !Directory.Exists(dir)) continue;
                foreach (string file in Directory.GetFiles(dir, stem + "*.png", SearchOption.AllDirectories))
                {
                    string full = Paths.Local(file);
                    if (full == null || !Paths.Inside(full, root)) continue;
                    string fileName = Path.GetFileName(full);
                    if (!string.Equals(fileName, stem + ".png", StringComparison.OrdinalIgnoreCase) &&
                        !fileName.StartsWith(stem + ".", StringComparison.OrdinalIgnoreCase)) continue;
                    string qualifiers = full.Substring(dir.Length).ToLowerInvariant();
                    if (qualifiers.Contains("contrast-") || qualifiers.Contains("lightunplated")) continue;
                    int size = PngSize(full);
                    if (size <= 0) continue;
                    long score = (long)Math.Min(size, 256) * 4 + (qualifiers.Contains("altform-unplated") ? 2 : 0) + (size <= 256 ? 1 : 0);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = full;
                    }
                }
            }
            if (best == null) return null;
            var info = new FileInfo(best);
            return info.Length > 2 * 1024 * 1024 ? null : File.ReadAllBytes(best);
        }

        // A PNG's width, from its header; 0 for anything that isn't one.
        static int PngSize(string file)
        {
            var head = new byte[24];
            using (var f = File.OpenRead(file))
            {
                if (f.Read(head, 0, 24) != 24) return 0;
            }
            if (head[0] != 0x89 || head[1] != (byte)'P' || head[2] != (byte)'N' || head[3] != (byte)'G') return 0;
            return (head[16] << 24) | (head[17] << 16) | (head[18] << 8) | head[19];
        }

        // Every process of this session running a program from the
        // package of this app: a packaged app's process may be any program
        // in it (Calculator runs CalculatorApp.exe).
        public static int Running(string id, DateTime since)
        {
            string family = Family(id);
            if (family == null) return 0;
            List<string> roots = Roots(family);
            if (roots.Count == 0) return 0;
            int session = System.Diagnostics.Process.GetCurrentProcess().SessionId;
            int count = 0;
            foreach (var p in System.Diagnostics.Process.GetProcesses())
            {
                try
                {
                    if (p.SessionId != session || p.StartTime.ToUniversalTime() < since) continue;
                    string image = Windows.ImagePath((uint)p.Id);
                    if (image != null && Paths.Inside(image, roots[0])) count++;
                }
                catch (Exception) { }
            }
            return count;
        }

        static readonly Guid ActivationManagerClsid = new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C");

        [ComImport, Guid("2e941141-7f97-4756-ba1d-9decde894a3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IApplicationActivationManager
        {
            [PreserveSig]
            int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string appUserModelId, [MarshalAs(UnmanagedType.LPWStr)] string arguments,
                int options, out uint processId);
        }
    }

    // What wild-guest needs from a package's AppxManifest.xml. The file is
    // read with no DTDs and no external entities.
    sealed class Manifest
    {
        public sealed class App
        {
            public string Id, Executable, Logo;
            public bool Listed;
        }

        public readonly List<App> Apps = new List<App>();
        public bool Framework, Resource;

        public App Find(string id)
        {
            foreach (App a in Apps)
            {
                if (string.Equals(a.Id, id, StringComparison.OrdinalIgnoreCase)) return a;
            }
            return null;
        }

        public static Manifest Read(string root)
        {
            var manifest = new Manifest();
            string file = Path.Combine(root, "AppxManifest.xml");
            if (!File.Exists(file) || new FileInfo(file).Length > 4 * 1024 * 1024) return manifest;
            var settings = new XmlReaderSettings { DtdProcessing = DtdProcessing.Prohibit, XmlResolver = null };
            var doc = new XmlDocument();
            doc.XmlResolver = null;
            try
            {
                using (var reader = XmlReader.Create(file, settings)) doc.Load(reader);
            }
            catch (Exception)
            {
                return manifest;
            }
            foreach (XmlNode node in doc.DocumentElement.ChildNodes)
            {
                if (node.LocalName == "Properties")
                {
                    foreach (XmlNode p in node.ChildNodes)
                    {
                        if (p.LocalName == "Framework" && p.InnerText.Trim() == "true") manifest.Framework = true;
                        if (p.LocalName == "ResourcePackage" && p.InnerText.Trim() == "true") manifest.Resource = true;
                    }
                }
                if (node.LocalName != "Applications") continue;
                foreach (XmlNode a in node.ChildNodes)
                {
                    if (a.LocalName != "Application") continue;
                    var app = new App { Id = Attr(a, "Id"), Executable = Attr(a, "Executable"), Listed = true };
                    foreach (XmlNode v in a.ChildNodes)
                    {
                        if (v.LocalName != "VisualElements") continue;
                        app.Listed = !string.Equals(Attr(v, "AppListEntry"), "none", StringComparison.OrdinalIgnoreCase);
                        app.Logo = Attr(v, "Square44x44Logo") ?? Attr(v, "Square30x30Logo") ?? Attr(v, "SmallLogo") ??
                                   Attr(v, "Square150x150Logo") ?? Attr(v, "Logo");
                    }
                    if (!string.IsNullOrEmpty(app.Id)) manifest.Apps.Add(app);
                }
            }
            return manifest;
        }

        static string Attr(XmlNode node, string name)
        {
            if (node.Attributes == null) return null;
            foreach (XmlAttribute a in node.Attributes)
            {
                if (a.LocalName == name) return a.Value;
            }
            return null;
        }
    }

    // The Shell's Applications folder, the list the Start menu shows.
    static class Shell
    {
        static readonly Guid AppsFolderId = new Guid("1e87508d-89c2-42f0-8a7e-645a0f50ca58");
        static readonly Guid EnumItems = new Guid("94f60519-2850-4924-aa5a-d15e84868039");
        static readonly Guid IShellItemId = new Guid("43826d1e-e718-42ee-bc55-a1e261c37bfe");
        static readonly Guid IEnumShellItemsId = new Guid("70629033-e363-4a28-a567-0db78006e6d7");

        [StructLayout(LayoutKind.Sequential)]
        struct PropertyKey
        {
            public Guid Format;
            public uint Id;
        }

        // System.AppUserModel.ID
        static PropertyKey AppUserModelId = new PropertyKey { Format = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3"), Id = 5 };

        [ComImport, Guid("43826d1e-e718-42ee-bc55-a1e261c37bfe"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IShellItem
        {
            [PreserveSig] int BindToHandler(IntPtr bindContext, [In] ref Guid handler, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int GetParent(out IntPtr parent);
            [PreserveSig] int GetDisplayName(uint form, out IntPtr name);
            [PreserveSig] int GetAttributes(uint mask, out uint attributes);
            [PreserveSig] int Compare(IntPtr other, uint hint, out int order);
        }

        [ComImport, Guid("7e9fb0d3-919f-4307-ab2e-9b1860310c93"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IShellItem2
        {
            [PreserveSig] int BindToHandler(IntPtr bindContext, [In] ref Guid handler, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int GetParent(out IntPtr parent);
            [PreserveSig] int GetDisplayName(uint form, out IntPtr name);
            [PreserveSig] int GetAttributes(uint mask, out uint attributes);
            [PreserveSig] int Compare(IntPtr other, uint hint, out int order);
            [PreserveSig] int GetPropertyStore(int flags, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int GetPropertyStoreWithCreateObject(int flags, IntPtr createObject, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int GetPropertyStoreForKeys(IntPtr keys, uint count, int flags, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int GetPropertyDescriptionList([In] ref PropertyKey type, [In] ref Guid iid, out IntPtr result);
            [PreserveSig] int Update(IntPtr bindContext);
            [PreserveSig] int GetProperty([In] ref PropertyKey key, IntPtr value);
            [PreserveSig] int GetCLSID([In] ref PropertyKey key, out Guid clsid);
            [PreserveSig] int GetFileTime([In] ref PropertyKey key, out long time);
            [PreserveSig] int GetInt32([In] ref PropertyKey key, out int value);
            [PreserveSig] int GetString([In] ref PropertyKey key, out IntPtr value);
        }

        [ComImport, Guid("70629033-e363-4a28-a567-0db78006e6d7"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        interface IEnumShellItems
        {
            [PreserveSig] int Next(uint count, out IntPtr item, out uint fetched);
        }

        [DllImport("shell32.dll")]
        static extern int SHGetKnownFolderItem([In] ref Guid folder, uint flags, IntPtr token, [In] ref Guid iid, out IntPtr item);

        static string TakeString(IntPtr p)
        {
            if (p == IntPtr.Zero) return null;
            try { return Marshal.PtrToStringUni(p); }
            finally { Marshal.FreeCoTaskMem(p); }
        }

        // Each item's AUMID and the name Start shows for it.
        public static List<KeyValuePair<string, string>> AppsFolderItems()
        {
            var items = new List<KeyValuePair<string, string>>();
            Guid folderId = AppsFolderId, itemIid = IShellItemId, handler = EnumItems, enumIid = IEnumShellItemsId;
            IntPtr folderPtr, enumPtr;
            int hr = SHGetKnownFolderItem(ref folderId, 0, IntPtr.Zero, ref itemIid, out folderPtr);
            if (hr < 0) Marshal.ThrowExceptionForHR(hr);
            var folder = (IShellItem)Marshal.GetObjectForIUnknown(folderPtr);
            Marshal.Release(folderPtr);
            try
            {
                hr = folder.BindToHandler(IntPtr.Zero, ref handler, ref enumIid, out enumPtr);
                if (hr < 0) Marshal.ThrowExceptionForHR(hr);
                var list = (IEnumShellItems)Marshal.GetObjectForIUnknown(enumPtr);
                Marshal.Release(enumPtr);
                try
                {
                    for (int n = 0; n < 2000; n++)
                    {
                        IntPtr itemPtr;
                        uint fetched;
                        if (list.Next(1, out itemPtr, out fetched) != 0 || fetched == 0 || itemPtr == IntPtr.Zero) break;
                        object o = Marshal.GetObjectForIUnknown(itemPtr);
                        Marshal.Release(itemPtr);
                        try
                        {
                            var item = o as IShellItem2;
                            if (item == null) continue;
                            IntPtr p;
                            PropertyKey key = AppUserModelId;
                            string id = item.GetString(ref key, out p) >= 0 ? TakeString(p) : null;
                            if (id == null || !Packages.IsAumid(id)) continue;
                            // SIGDN_NORMALDISPLAY
                            string name = item.GetDisplayName(0, out p) >= 0 ? TakeString(p) : null;
                            items.Add(new KeyValuePair<string, string>(id, name));
                        }
                        finally
                        {
                            Marshal.ReleaseComObject(o);
                        }
                    }
                }
                finally
                {
                    Marshal.ReleaseComObject(list);
                }
            }
            finally
            {
                Marshal.ReleaseComObject(folder);
            }
            return items;
        }
    }
}
