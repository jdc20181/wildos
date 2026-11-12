using System;
using System.IO;

namespace WildOS.Guest
{
    // Which paths a request from Wild OS may name. Anything that would make
    // Windows reach over the network (\\server\share, which also sends the
    // account's or the computer's credentials to that server), a device
    // (\\.\, \\?\), a URL or another protocol handler is refused before it
    // gets to the file system or the Shell.
    static class Paths
    {
        // An absolute path on a drive letter, normalised, or null. The
        // drive may be Z:, the folders Wild OS itself shares into the
        // session; drive letters are per session, so the service, which has
        // none mapped, only ever sees the local disks.
        public static string Local(string path)
        {
            if (string.IsNullOrEmpty(path) || path.Length > 1024) return null;
            if (path.Length < 3 || !char.IsLetter(path[0]) || path[0] > 'z' || path[1] != ':' || (path[2] != '\\' && path[2] != '/')) return null;
            // A colon after the drive's is an alternate data stream.
            if (path.IndexOf(':', 2) >= 0) return null;
            foreach (char c in path)
            {
                if (c < ' ') return null;
            }
            string full;
            try { full = Path.GetFullPath(path); }
            catch (Exception) { return null; }
            if (full.Length < 3 || full[1] != ':' || full[2] != '\\' || full.StartsWith(@"\\", StringComparison.Ordinal)) return null;
            return full;
        }

        // A program named without any folder ("mspaint.exe"), which Windows
        // finds on the session's PATH and in App Paths, as Run would.
        public static bool BareName(string name)
        {
            if (string.IsNullOrEmpty(name) || name.Length > 260) return false;
            if (name.IndexOfAny(new[] { '\\', '/', ':', '"', '<', '>', '|', '?', '*' }) >= 0) return false;
            foreach (char c in name)
            {
                if (c < ' ') return false;
            }
            return name.Trim('.', ' ').Length > 0;
        }

        // Whether path is inside folder (both already from Local).
        public static bool Inside(string path, string folder)
        {
            if (path == null || string.IsNullOrEmpty(folder)) return false;
            string f = folder.TrimEnd(new[] { '\\' }) + "\\";
            return path.StartsWith(f, StringComparison.OrdinalIgnoreCase);
        }

        // A file the service may read on Wild OS's behalf: local, and not in
        // the profile of any account other than Windows mode's own. The
        // service runs as LocalSystem and can read everything.
        public static string ForService(string path, string account)
        {
            string full = Local(path);
            if (full == null) return null;
            string drive = Environment.GetEnvironmentVariable("SystemDrive") ?? "C:";
            string users = drive + @"\Users\";
            if (full.StartsWith(users, StringComparison.OrdinalIgnoreCase))
            {
                string rest = full.Substring(users.Length);
                int slash = rest.IndexOf('\\');
                string owner = slash < 0 ? rest : rest.Substring(0, slash);
                if (!string.Equals(owner, "Public", StringComparison.OrdinalIgnoreCase) &&
                    !(Account(account) && string.Equals(owner, account, StringComparison.OrdinalIgnoreCase))) return null;
            }
            return full;
        }

        // A plain Windows account name, safe to use as a folder name.
        public static bool Account(string account)
        {
            return !string.IsNullOrEmpty(account) && account.Length <= 20 && BareName(account) && account.Trim(new[] { '.' }) == account;
        }
    }
}
