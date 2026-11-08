using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;

namespace WildOS.Guest
{
    // Warm apps (brief 5.5a): the apps a person uses most, already started
    // but hidden in the session, so opening one only shows its window. Wild
    // OS says which, in order, and how many ("warm" message); a warm app is
    // used for the next launch, and once it has been closed a fresh one is
    // started hidden again.
    //
    // Office apps are started through their own automation objects
    // (Word.Application and so on), which start without any window and are
    // shown and handed to the person on launch. Other programs are not kept
    // warm: asked to start hidden, many show a window anyway, which would
    // appear on the desktop and keep Windows awake.
    //
    // Everything runs on one single-threaded apartment thread, as Office's
    // objects require.
    sealed class WarmPool
    {
        sealed class Entry
        {
            public string Path;
            public string Id;
            public object Office;
        }

        static readonly Dictionary<string, string> officeProgIds = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            { "winword", "Word.Application" }, { "excel", "Excel.Application" }, { "powerpnt", "PowerPoint.Application" },
        };

        const long MinFreeBytes = 1200L * 1024 * 1024;

        readonly List<Action> work = new List<Action>();
        readonly List<Entry> warm = new List<Entry>();
        List<Dictionary<string, object>> wanted = new List<Dictionary<string, object>>();
        int limit;
        DateTime lastLaunch = DateTime.MinValue;
        readonly DateTime created = DateTime.UtcNow;

        public WarmPool()
        {
            var thread = new Thread(Run) { IsBackground = true, Name = "warm apps" };
            thread.SetApartmentState(ApartmentState.STA);
            thread.Start();
        }

        // Runs fn on the pool's thread and waits for its result.
        T Call<T>(Func<T> fn, int timeoutMs)
        {
            T result = default(T);
            Exception error = null;
            var done = new ManualResetEvent(false);
            lock (work)
            {
                work.Add(delegate
                {
                    try { result = fn(); }
                    catch (Exception e) { error = e; }
                    done.Set();
                });
                Monitor.PulseAll(work);
            }
            if (!done.WaitOne(timeoutMs)) throw new TimeoutException("warm apps busy");
            if (error != null) throw error;
            return result;
        }

        public void SetWanted(List<object> apps, int max)
        {
            var list = new List<Dictionary<string, object>>();
            if (apps != null)
            {
                foreach (object a in apps)
                {
                    var d = a as Dictionary<string, object>;
                    if (d != null && !string.IsNullOrEmpty(Json.Str(d, "path"))) list.Add(d);
                }
            }
            lock (work)
            {
                work.Add(delegate
                {
                    wanted = list;
                    limit = Math.Max(0, Math.Min(max, 6));
                    Trim();
                });
                Monitor.PulseAll(work);
            }
        }

        public List<object> Describe()
        {
            return Call(delegate
            {
                var list = new List<object>();
                foreach (var e in warm)
                {
                    var d = new Dictionary<string, object>();
                    d["id"] = e.Id;
                    d["path"] = e.Path;
                    list.Add(d);
                }
                return list;
            }, 5000);
        }

        // A launch that a warm app can take: shows it, with the file if one
        // was given, and returns the launched reply; null when no warm app
        // fits, and the caller starts the program as usual.
        public Dictionary<string, object> TryLaunch(string path, List<string> args)
        {
            lastLaunch = DateTime.UtcNow;
            return Call(delegate { return Take(path, args); }, 15000);
        }

        void Run()
        {
            DateTime nextCheck = DateTime.UtcNow.AddSeconds(15);
            while (true)
            {
                Action next = null;
                lock (work)
                {
                    if (work.Count == 0) Monitor.Wait(work, 1000);
                    if (work.Count > 0)
                    {
                        next = work[0];
                        work.RemoveAt(0);
                    }
                }
                if (next != null)
                {
                    next();
                    continue;
                }
                if (DateTime.UtcNow >= nextCheck)
                {
                    nextCheck = DateTime.UtcNow.AddSeconds(10);
                    try { Refill(); }
                    catch (Exception e) { Log.Write("warm: " + e.Message); }
                }
            }
        }

        static string Stem(string path)
        {
            return System.IO.Path.GetFileNameWithoutExtension(path ?? "").ToLowerInvariant();
        }

        // Not while the session has just started or someone has just opened
        // something: a warm app must never slow down a real launch.
        void Refill()
        {
            if ((DateTime.UtcNow - created).TotalSeconds < 20 || (DateTime.UtcNow - lastLaunch).TotalSeconds < 15) return;
            warm.RemoveAll(e => !Alive(e));
            int count = 0;
            foreach (var app in wanted)
            {
                string path = Json.Str(app, "path");
                if (!officeProgIds.ContainsKey(Stem(path))) continue;
                if (count >= limit) break;
                count++;
                if (warm.Exists(e => string.Equals(e.Path, path, StringComparison.OrdinalIgnoreCase))) continue;
                if (!File.Exists(path) || FreeMemory() < MinFreeBytes) return;
                // Someone already has this app open; it is quick to open again.
                if (VisiblyRunning(path)) continue;
                var entry = Start(path, Json.Str(app, "id"));
                if (entry != null)
                {
                    warm.Add(entry);
                    Log.Write("warm: started " + entry.Id);
                }
                return;
            }
        }

        void Trim()
        {
            var keep = new List<string>();
            for (int i = 0; i < wanted.Count && i < limit; i++) keep.Add(Json.Str(wanted[i], "path").ToLowerInvariant());
            foreach (var e in warm.ToArray())
            {
                if (!keep.Contains(e.Path.ToLowerInvariant()))
                {
                    Quit(e);
                    warm.Remove(e);
                }
            }
        }

        Entry Start(string path, string id)
        {
            string progId;
            if (!officeProgIds.TryGetValue(Stem(path), out progId)) return null;
            Type t = Type.GetTypeFromProgID(progId);
            if (t == null) return null;
            return new Entry { Path = path, Id = id ?? Stem(path), Office = Activator.CreateInstance(t) };
        }

        bool Alive(Entry e)
        {
            try
            {
                Get(e.Office, "Name");
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        static bool VisiblyRunning(string path)
        {
            foreach (Dictionary<string, object> w in Windows.List())
            {
                if (string.Equals(w["image"] as string, path, StringComparison.OrdinalIgnoreCase)) return true;
            }
            return false;
        }

        Dictionary<string, object> Take(string path, List<string> args)
        {
            warm.RemoveAll(e => !Alive(e));
            var entry = warm.Find(e => string.Equals(e.Path, path, StringComparison.OrdinalIgnoreCase));
            if (entry == null) return null;
            warm.Remove(entry);
            var reply = new Dictionary<string, object>();
            reply["type"] = "launched";
            reply["warm"] = true;
            ShowOffice(entry.Office, Stem(path), args);
            reply["pid"] = 0L;
            return reply;
        }

        static object Get(object o, string name)
        {
            return o.GetType().InvokeMember(name, BindingFlags.GetProperty, null, o, null);
        }

        static void Set(object o, string name, object value)
        {
            o.GetType().InvokeMember(name, BindingFlags.SetProperty, null, o, new[] { value });
        }

        static object Invoke(object o, string name, params object[] args)
        {
            return o.GetType().InvokeMember(name, BindingFlags.InvokeMethod, null, o, args);
        }

        // Shows a warm Office app with the document asked for, or a new
        // blank one, and hands it to the person: with UserControl set it no
        // longer belongs to this helper and closes when they close it.
        static void ShowOffice(object app, string stem, List<string> args)
        {
            string file = args != null && args.Count > 0 ? args[0] : null;
            if (stem == "powerpnt")
            {
                object presentations = Get(app, "Presentations");
                if (file != null) Invoke(presentations, "Open", file);
                else Invoke(presentations, "Add");
            }
            else
            {
                // Shown first, so its window is on screen while the
                // document loads rather than after.
                Set(app, "Visible", true);
                object docs = Get(app, stem == "excel" ? "Workbooks" : "Documents");
                if (file != null) Invoke(docs, "Open", file);
                else Invoke(docs, "Add");
            }
            try { Set(app, "UserControl", true); } catch (Exception) { }
            try { Invoke(app, "Activate"); } catch (Exception) { }
            Marshal.ReleaseComObject(app);
        }

        static void Quit(Entry e)
        {
            try
            {
                Invoke(e.Office, "Quit");
                Marshal.ReleaseComObject(e.Office);
            }
            catch (Exception)
            {
            }
        }

        static long FreeMemory()
        {
            var status = new Native.MEMORYSTATUSEX();
            status.dwLength = (uint)Marshal.SizeOf(typeof(Native.MEMORYSTATUSEX));
            return Native.GlobalMemoryStatusEx(ref status) ? (long)status.ullAvailPhys : 0;
        }
    }
}
