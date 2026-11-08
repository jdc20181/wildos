"""Wild OS Windows mode: the host side.

winmoded        the system service (system.py), root: libvirt, the guest
                channels, the lifecycle state machine and idle policy
wildos-winmode  the per-user agent (agent.py) and its command line
                (cli.py): the one RemoteApp connection, window grouping,
                Start entries, file types and theme
wild-run        the router for Windows programs (run.py, compat.py)
"""

VERSION = "0.2.0"
