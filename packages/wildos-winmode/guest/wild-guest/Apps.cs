using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;

namespace WildOS.Guest
{
    // The programs in Windows' Start menu, for Wild OS's Start entries.
    // Shortcuts are read through the Shell's own WScript.Shell object, the
    // same reading Explorer does. The skip list is the old builder's.
    // Microsoft Store apps have no shortcut; the session helper lists them
    // (Packages) and they join the list here.
    static class Apps
    {
        static readonly Regex skip = new Regex(
            "uninstall|setup|readme|help|documentation|license|licence|website|support|release notes|administrative tools|windows tools|" +
            "accessibility|system tools|database compare|spreadsheet compare|telemetry|language preferences|office tools",
            RegexOptions.IgnoreCase);

        public static List<object> List(string account, bool icons)
        {
            var roots = new List<string>
            {
                Environment.ExpandEnvironmentVariables(@"%ProgramData%\Microsoft\Windows\Start Menu\Programs"),
            };
            if (Paths.Account(account))
            {
                string drive = Environment.GetEnvironmentVariable("SystemDrive") ?? "C:";
                roots.Add(drive + @"\Users\" + account + @"\AppData\Roaming\Microsoft\Windows\Start Menu\Programs");
            }
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            object shell = shellType == null ? null : Activator.CreateInstance(shellType);
            var seen = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            var result = new List<object>();
            string windir = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
            string systemProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            foreach (string root in roots)
            {
                if (!Directory.Exists(root)) continue;
                string[] links;
                try { links = Directory.GetFiles(root, "*.lnk", SearchOption.AllDirectories); }
                catch (Exception) { continue; }
                foreach (string lnk in links)
                {
                    string name = Path.GetFileNameWithoutExtension(lnk);
                    if (skip.IsMatch(name) || seen.ContainsKey(name) || shell == null) continue;
                    string target, arguments;
                    try
                    {
                        object link = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { lnk });
                        target = (string)link.GetType().InvokeMember("TargetPath", BindingFlags.GetProperty, null, link, null);
                        arguments = (string)link.GetType().InvokeMember("Arguments", BindingFlags.GetProperty, null, link, null);
                    }
                    catch (Exception)
                    {
                        continue;
                    }
                    // A shortcut kept relative to the user's own folders (an
                    // app installed just for the account, like Termius)
                    // resolves against this service's profile, SYSTEM's;
                    // it belongs in the account's.
                    if (!string.IsNullOrEmpty(target) && Paths.Account(account) &&
                        target.StartsWith(systemProfile, StringComparison.OrdinalIgnoreCase))
                    {
                        string sysDrive = Environment.GetEnvironmentVariable("SystemDrive") ?? "C:";
                        target = sysDrive + @"\Users\" + account + target.Substring(systemProfile.Length, target.Length - systemProfile.Length);
                    }
                    // Any program can put a shortcut in the account's Start
                    // menu, and this runs as LocalSystem: a target on
                    // \server\share would have the computer sign in to that
                    // server just to look at it.
                    target = Paths.ForService(target, account);
                    if (target == null || !target.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) || !File.Exists(target)) continue;
                    if (target.StartsWith(windir, StringComparison.OrdinalIgnoreCase)) continue;
                    seen[name] = true;
                    var app = new Dictionary<string, object>();
                    app["name"] = name;
                    app["target"] = target;
                    app["arguments"] = arguments ?? "";
                    app["shortcut"] = lnk;
                    if (icons)
                    {
                        byte[] ico = null;
                        try { ico = Icons.FromExecutable(target); } catch (Exception) { }
                        app["icon"] = ico == null ? null : Convert.ToBase64String(ico);
                    }
                    result.Add(app);
                }
            }
            return result;
        }

        // The session helper's Store apps, after the shortcuts: one with the
        // same name as a shortcut is the same app. Only the fields Wild OS
        // reads go on, each checked, since the list comes from a program
        // running as the account.
        public static void AddPackaged(List<object> result, List<object> packaged)
        {
            var seen = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            foreach (object o in result)
            {
                var d = o as Dictionary<string, object>;
                string name = d == null ? null : Json.Str(d, "name");
                if (name != null) seen[name] = true;
            }
            foreach (object o in packaged)
            {
                var d = o as Dictionary<string, object>;
                if (d == null) continue;
                string name = (Json.Str(d, "name") ?? "").Trim();
                string id = Json.Str(d, "aumid");
                string target = Json.Str(d, "target");
                if (name.Length == 0 || name.Length > 200 || skip.IsMatch(name) || seen.ContainsKey(name) || !Packages.IsAumid(id)) continue;
                if (target == null || !(target == Packages.AppsFolder + id || Paths.Local(target) != null)) continue;
                seen[name] = true;
                var app = new Dictionary<string, object>();
                app["name"] = name;
                app["target"] = target;
                app["arguments"] = "";
                app["aumid"] = id;
                app["package"] = Packages.Family(id);
                if (d.ContainsKey("icon")) app["icon"] = Json.Str(d, "icon");
                result.Add(app);
            }
        }
    }
}
