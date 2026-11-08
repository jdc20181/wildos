"""The lifecycle state machine and idle policy (brief 5.6), with no I/O so
it can be tested on its own. The system service feeds it what it sees and
carries out the action it returns.

States: NotInstalled, SettingUp, Running, Paused, Saved, Stopped, Error.

Running means at least one Windows app window is open, or setup,
maintenance or a launch is under way. When the last of those ends, the
idle timer starts; when it runs out with still nothing open, the idle
policy decides between pausing and saving to disk. The RemoteApp
connection does not count: it stays open, persistent, while Windows is
paused or saved, costing nothing.
"""

NOT_INSTALLED = "NotInstalled"
SETTING_UP = "SettingUp"
RUNNING = "Running"
PAUSED = "Paused"
SAVED = "Saved"
STOPPED = "Stopped"
ERROR = "Error"

PAUSE = "pause"
SAVE = "save"

# The host must keep at least this share of its memory free for a paused
# guest to be worth keeping in memory (brief 5.6, Automatic).
MEMORY_FREE_SHARE = 0.25


def state_from_vm(vm_state, setting_up=False, error=None):
    if error:
        return ERROR
    if vm_state == "":
        return SETTING_UP if setting_up else NOT_INSTALLED
    if setting_up:
        return SETTING_UP
    return {
        "running": RUNNING,
        "paused": PAUSED,
        "pmsuspended": PAUSED,
        "saved": SAVED,
        "shut off": STOPPED,
        "crashed": ERROR,
    }.get(vm_state, ERROR)


def idle_action(policy, on_battery, mem_available, mem_total, mem_pressure=False):
    """What to do with an idle guest: PAUSE (keep in memory) or SAVE (to
    disk, freeing all its memory)."""
    if policy == "keep":
        return PAUSE
    if policy == "save":
        return SAVE
    if on_battery or mem_pressure:
        return SAVE
    if mem_total > 0 and mem_available / mem_total < MEMORY_FREE_SHARE:
        return SAVE
    return PAUSE


class Lifecycle:
    """Tracks what keeps the guest busy and when it went idle. now is a
    monotonic clock in seconds, passed in so tests can move time."""

    def __init__(self, idle_seconds=60):
        self.idle_seconds = idle_seconds
        self.vm_state = ""
        self.windows = 0
        self.connection = False
        self.busy_reasons = set()
        self.idle_since = None

    def busy(self):
        return self.windows > 0 or bool(self.busy_reasons)

    def update(self, now, vm_state=None, windows=None, connection=None):
        if vm_state is not None:
            self.vm_state = vm_state
        if windows is not None:
            self.windows = windows
        if connection is not None:
            self.connection = connection
        self._retime(now)

    def hold(self, now, reason, on=True):
        """Setup, maintenance and a launch in progress keep the guest
        running whatever the window count says."""
        if on:
            self.busy_reasons.add(reason)
        else:
            self.busy_reasons.discard(reason)
        self._retime(now)

    def _retime(self, now):
        if self.vm_state != "running" or self.busy():
            self.idle_since = None
        elif self.idle_since is None:
            self.idle_since = now

    def deadline(self):
        return None if self.idle_since is None else self.idle_since + self.idle_seconds

    def due(self, now):
        """True once the guest has been idle for the full idle time."""
        d = self.deadline()
        return d is not None and now >= d

    def reconsider_paused(self, policy, on_battery, mem_available, mem_total, mem_pressure=False):
        """A paused guest under Automatic is saved to disk when the host goes
        on battery or runs short of memory; nothing else changes it."""
        if self.vm_state != "paused" or policy != "automatic":
            return None
        if idle_action(policy, on_battery, mem_available, mem_total, mem_pressure) == SAVE:
            return SAVE
        return None
