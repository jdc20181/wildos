using System;
using System.ServiceProcess;
using System.Threading;

[assembly: System.Reflection.AssemblyTitle("Wild OS guest helper")]
[assembly: System.Reflection.AssemblyProduct("Wild OS")]
[assembly: System.Reflection.AssemblyVersion("1.0.0.0")]

namespace WildOS.Guest
{
    static class Program
    {
        // wild-guest.exe --service   the Windows service (setup.ps1 registers it)
        // wild-guest.exe --session   the session helper (the service starts it)
        // wild-guest.exe --start B64  start a program, then end: the first
        //                            program of a RemoteApp connection, with
        //                            its path and arguments as base64 JSON
        //                            ({"path","args","cwd"}), since FreeRDP's
        //                            /app option cannot carry commas or quotes
        // wild-guest.exe --console   the service in the foreground, for testing
        //
        // Built as a windowed program with no window, so nothing it does
        // ever opens a console that RemoteApp would show.
        // The session helper starts programs through ShellExecute, which
        // needs a single-threaded apartment and can hang without one.
        [STAThread]
        static int Main(string[] args)
        {
            AppDomain.CurrentDomain.UnhandledException += delegate (object sender, UnhandledExceptionEventArgs e)
            {
                Log.Write("unhandled: " + e.ExceptionObject);
            };
            string mode = args.Length > 0 ? args[0] : "--service";
            switch (mode)
            {
                case "--session":
                    return SessionHelper.Run();
                case "--start":
                    return args.Length > 1 ? SessionHelper.StartOnce(args[1]) : 2;
                case "--console":
                    new GuestService().Start();
                    Thread.Sleep(Timeout.Infinite);
                    return 0;
                default:
                    ServiceBase.Run(new GuestService());
                    return 0;
            }
        }
    }
}
