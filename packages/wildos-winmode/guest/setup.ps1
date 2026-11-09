# Wild OS Windows mode: first sign-in setup (brief 5.4, steps 1, 2, 3, 5, 6,
# 7, 8 and 9). Windows Setup runs it once from the setup payload CD through
# FirstLogonCommands, as the local account, elevated. Each step writes
# C:\ProgramData\WildOS\stage.json, which the host reads through the QEMU
# guest agent, so progress is real rather than estimated. Steps that passed
# leave a marker, so running it again carries on where it stopped.
#
# Policies and switches are the supported ones (Group Policy registry values,
# the per-user values Settings itself writes, provisioned app removal), so
# Windows Update and activation are untouched. Several come from the old
# builder's tune.ps1 and firstlogon.ps1 in packages/wildos-winmode.

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$state = 'C:\ProgramData\WildOS'
New-Item -ItemType Directory -Force -Path $state | Out-Null
Start-Transcript -Path "$state\setup.log" -Append | Out-Null
$settings = Get-Content "$here\settings.json" -Raw | ConvertFrom-Json
$drive = $settings.drive
Remove-Item "$state\ready.json" -ErrorAction SilentlyContinue

function Stage($name, $status, $detail) {
    $line = @{ name = $name; state = $status; detail = $detail; time = (Get-Date).ToString('s') } | ConvertTo-Json -Compress
    Set-Content -Encoding UTF8 -Path "$state\stage.json" -Value $line
    Write-Host "[$((Get-Date).ToString('HH:mm:ss'))] $name $status $detail"
}

function Set-Value($path, $name, $value, $type = 'DWord') {
    try {
        if (-not (Test-Path $path)) { New-Item -Path $path -Force | Out-Null }
        New-ItemProperty -Path $path -Name $name -Value $value -PropertyType $type -Force | Out-Null
    } catch {
        Write-Host "warning: $path\$name not set: $($_.Exception.Message)"
    }
}

# Runs one step once. A failure in a step marked critical stops setup and
# leaves Windows as it is for the host to report.
function Step($name, [bool]$critical, [scriptblock]$body) {
    if (Test-Path "$state\done-$name") { Stage $name 'done' 'earlier'; return }
    Stage $name 'running' ''
    try {
        & $body
        New-Item -ItemType File -Force "$state\done-$name" | Out-Null
        Stage $name 'done' ''
    } catch {
        $why = $_.Exception.Message
        if ($critical) {
            Stage $name 'failed' $why
            Stop-Transcript | Out-Null
            exit 1
        }
        Write-Host "warning: $name : $why"
        New-Item -ItemType File -Force "$state\done-$name" | Out-Null
        Stage $name 'done' "with warnings: $why"
    }
}

function Find-Cd($file) {
    $d = Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path (Join-Path $_.Root $file) } | Select-Object -First 1
    if (-not $d) { throw "no CD has $file" }
    return $d.Root
}

# 1. virtio drivers and tools, the QEMU guest agent first of all, so the
# host can follow the rest.
Step 'guest-tools' $true {
    $cd = Find-Cd 'virtio-win-guest-tools.exe'
    # Trust each driver package's publisher, so the silent install does not
    # stop at a "Would you like to install this device software?" prompt.
    $store = New-Object Security.Cryptography.X509Certificates.X509Store('TrustedPublisher', 'LocalMachine')
    $store.Open('ReadWrite')
    foreach ($cat in Get-ChildItem "$cd*\w11\amd64\*.cat" -ErrorAction SilentlyContinue) {
        $cert = (Get-AuthenticodeSignature $cat.FullName).SignerCertificate
        if ($cert) { $store.Add($cert) }
    }
    $store.Close()
    $p = Start-Process -Wait -PassThru "$cd\virtio-win-guest-tools.exe" -ArgumentList '/install', '/quiet', '/norestart'
    Write-Host "virtio-win-guest-tools exit $($p.ExitCode)"
    if (-not (Get-Service QEMU-GA -ErrorAction SilentlyContinue)) {
        Start-Process -Wait msiexec.exe -ArgumentList '/i', "`"$cd\guest-agent\qemu-ga-x86_64.msi`"", '/qn', '/norestart'
    }
    Set-Service QEMU-GA -StartupType Automatic
    Start-Service QEMU-GA
}

# 2. The shared folders on Wild OS become drive Z:, through the RemoteApp
# connection itself (settings.json transport "rdp"). Testing on real
# hardware ruled virtiofs out; the other transports stay
# for setups made before.
Step 'shared-folder' $true {
    # The RemoteApp connection carries the folders (FreeRDP's drive
    # redirection) and wild-guest maps them to Z: whenever it connects, so
    # there is nothing to install. virtiofs failed on real hardware.
    if ($settings.transport -eq 'rdp') { return }
    if ($settings.transport -eq 'samba') {
        $share = "\\$($settings.host)\$($settings.samba.share)"
        # The share's password is kept by Windows' Credential Manager for this
        # account, so the drive comes back at every sign-in.
        cmdkey /add:$($settings.host) /user:$($settings.samba.user) /pass:$($settings.samba.password) | Out-Null
        net use "$($drive):" /delete /y 2>$null | Out-Null
        net use "$($drive):" $share /persistent:yes | Out-Null
    } else {
        $p = Start-Process -Wait -PassThru msiexec.exe -ArgumentList '/i', "`"$here\winfsp.msi`"", '/qn', '/norestart'
        if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { throw "WinFsp install exit $($p.ExitCode)" }
        Set-Value 'HKLM:\SOFTWARE\VirtIO-FS' 'MountPoint' "$($drive):" 'String'
        Set-Service VirtioFsSvc -StartupType Automatic
        Start-Service VirtioFsSvc
    }
    $deadline = (Get-Date).AddSeconds(90)
    while (-not (Test-Path "$($drive):\Documents") -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
    if (-not (Test-Path "$($drive):\Documents")) { throw "$($drive):\Documents did not appear" }
}

# wild-guest, the helper that lists windows and starts apps inside the
# one RemoteApp session (brief 5.11), as a service that restarts itself.
Step 'wild-guest' $true {
    $dir = 'C:\Program Files\Wild OS'
    New-Item -ItemType Directory -Force $dir | Out-Null
    Copy-Item -Force "$here\wild-guest.exe" "$dir\wild-guest.exe"
    if (-not (Get-Service WildGuest -ErrorAction SilentlyContinue)) {
        New-Service -Name WildGuest -DisplayName 'Wild OS guest helper' -BinaryPathName "`"$dir\wild-guest.exe`" --service" -StartupType Automatic | Out-Null
        sc.exe failure WildGuest reset= 60 actions= restart/2000/restart/5000/restart/30000 | Out-Null
    }
    Copy-Item "$here\settings.json" "$state\settings.json" -Force
    Start-Service WildGuest
}

# 3. Remote Desktop for RemoteApp: unlisted programs allowed, Network Level
# Authentication required, and reachable only from the Wild OS host.
Step 'remote-desktop' $true {
    $ts = 'HKLM:\SYSTEM\CurrentControlSet\Control\Terminal Server'
    Set-Value $ts 'fDenyTSConnections' 0
    Set-Value "$ts\WinStations\RDP-Tcp" 'UserAuthentication' 1
    # The brief's value, plus the two Windows actually reads: the allow-list
    # switch and its Group Policy counterpart.
    $list = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Terminal Server\TSAppAllowList'
    Set-Value $list 'fAllowUnlistedRemotePrograms' 1
    Set-Value $list 'fDisabledAllowList' 1
    $tsp = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows NT\Terminal Services'
    Set-Value $tsp 'fAllowUnlistedRemotePrograms' 1
    # When the last app closes, Windows keeps the session (disconnected)
    # rather than signing out, so the next app opens in about a second
    # instead of waiting for a new sign-in. Five days, the longest the
    # policy offers short of never.
    Set-Value $tsp 'RemoteAppLogoffTimeLimit' 432000000
    Set-Value $tsp 'MaxDisconnectionTime' 0
    # RemoteApp windows first drew properly (Windows 11 build 26300) with
    # the WDDM remote display driver off and FreeRDP 3.32; whether the driver
    # alone matters was not separated, so it stays off.
    Set-Value $tsp 'fEnableWddmDriver' 0
    # Windows' own Remote Desktop rules allow any address; they stay off and
    # one rule allows only the host.
    Get-NetFirewallRule -Group '@FirewallAPI.dll,-28752' -ErrorAction SilentlyContinue | Disable-NetFirewallRule
    foreach ($proto in 'TCP', 'UDP') {
        Remove-NetFirewallRule -Name "WildOS-RDP-$proto" -ErrorAction SilentlyContinue
        New-NetFirewallRule -Name "WildOS-RDP-$proto" -DisplayName "Wild OS: Remote Desktop from this PC ($proto)" `
            -Direction Inbound -Protocol $proto -LocalPort 3389 -RemoteAddress $settings.host -Action Allow -Profile Any | Out-Null
    }
    Restart-Service TermService -Force -ErrorAction SilentlyContinue
}

# The account must never expire or sign in on its own: an open console
# session would be taken over by the first RemoteApp connection.
Step 'account' $true {
    Set-LocalUser -Name $settings.account -PasswordNeverExpires $true
    $wl = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon'
    Set-Value $wl 'AutoAdminLogon' '0' 'String'
    Remove-ItemProperty $wl -Name 'DefaultPassword', 'AutoLogonCount' -ErrorAction SilentlyContinue
}

# 3. Speed and privacy. Diagnostic data goes to Required (1), the lowest Pro
# honours; 0 only applies to Enterprise and Education.
Step 'tuning' $false {
    $p = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows'
    Set-Value "$p\DataCollection" 'AllowTelemetry' 1
    Set-Value "$p\AdvertisingInfo" 'DisabledByGroupPolicy' 1
    Set-Value "$p\CloudContent" 'DisableWindowsConsumerFeatures' 1
    Set-Value "$p\CloudContent" 'DisableSoftLanding' 1
    Set-Value "$p\CloudContent" 'DisableTailoredExperiencesWithDiagnosticData' 1
    Set-Value "$p\CloudContent" 'DisableConsumerAccountStateContent' 1
    Set-Value "$p\Windows Search" 'DisableWebSearch' 1
    Set-Value "$p\Windows Search" 'ConnectedSearchUseWeb' 0
    Set-Value "$p\Windows Search" 'AllowCortana' 0
    Set-Value "$p\WindowsCopilot" 'TurnOffWindowsCopilot' 1
    Set-Value "$p\WindowsAI" 'DisableAIDataAnalysis' 1
    Set-Value "$p\OneDrive" 'DisableFileSyncNGSC' 1
    Set-Value 'HKLM:\SOFTWARE\Policies\Microsoft\Dsh' 'AllowNewsAndInterests' 0
    Set-Value 'HKCU:\Software\Policies\Microsoft\Windows\Explorer' 'DisableSearchBoxSuggestions' 1
    Set-Value 'HKCU:\Software\Policies\Microsoft\Windows\WindowsCopilot' 'TurnOffWindowsCopilot' 1
    Set-Value 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BingSearchEnabled' 0

    # The per-user switches behind Settings, Personalisation, Start and
    # Privacy, General.
    $cdm = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager'
    foreach ($n in 'SilentInstalledAppsEnabled', 'SystemPaneSuggestionsEnabled', 'SoftLandingEnabled', 'SubscribedContent-338388Enabled',
                   'SubscribedContent-338389Enabled', 'SubscribedContent-353694Enabled', 'SubscribedContent-353696Enabled',
                   'SubscribedContent-310093Enabled', 'RotatingLockScreenOverlayEnabled', 'PreInstalledAppsEnabled', 'OemPreInstalledAppsEnabled') {
        Set-Value $cdm $n 0
    }
    Set-Value 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo' 'Enabled' 0
    Set-Value 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'TailoredExperiencesWithDiagnosticDataEnabled' 0
    Set-Value 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'Start_IrisRecommendations' 0
    # Visual effects: best performance. Nobody sees the Windows desktop.
    Set-Value 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 2

    # Apps nobody asked for, removed for every account so they never come
    # back. The Store, Edge, Photos, Notepad and Calculator stay.
    $unwanted = 'Clipchamp.Clipchamp', 'Microsoft.BingNews', 'Microsoft.BingWeather', 'Microsoft.BingSearch', 'Microsoft.GamingApp',
                'Microsoft.MicrosoftSolitaireCollection', 'Microsoft.Xbox.TCUI', 'Microsoft.XboxGamingOverlay', 'Microsoft.XboxIdentityProvider',
                'Microsoft.XboxSpeechToTextOverlay', 'Microsoft.GetHelp', 'Microsoft.Getstarted', 'MicrosoftTeams', 'MSTeams', 'Microsoft.YourPhone',
                'Microsoft.OutlookForWindows', 'Microsoft.Todos', 'Microsoft.PowerAutomateDesktop', 'Microsoft.MicrosoftOfficeHub', 'Microsoft.People',
                'Microsoft.WindowsFeedbackHub', 'Microsoft.ZuneMusic', 'Microsoft.ZuneVideo', 'Microsoft.549981C3F5F10', 'Microsoft.Copilot',
                'Microsoft.Windows.DevHome', 'Microsoft.MicrosoftStickyNotes', 'Microsoft.WindowsMaps', 'Microsoft.WindowsAlarms'
    $provisioned = Get-AppxProvisionedPackage -Online
    foreach ($name in $unwanted) {
        $provisioned | Where-Object DisplayName -eq $name | ForEach-Object {
            Remove-AppxProvisionedPackage -Online -PackageName $_.PackageName -ErrorAction SilentlyContinue | Out-Null
        }
        Get-AppxPackage -Name $name -AllUsers -ErrorAction SilentlyContinue | Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue
    }

    # Indexing and SysMain cost disk and processor time for nothing here.
    foreach ($svc in 'WSearch', 'SysMain') {
        Stop-Service $svc -Force -ErrorAction SilentlyContinue
        Set-Service $svc -StartupType Disabled -ErrorAction SilentlyContinue
    }

    # Wild OS decides when Windows pauses or is saved; Windows must never
    # sleep or hibernate on its own.
    powercfg /hibernate off
    $high = '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
    if (-not (powercfg /list | Select-String $high)) { powercfg /duplicatescheme $high | Out-Null }
    powercfg /setactive $high
    foreach ($t in 'standby-timeout-ac', 'monitor-timeout-ac', 'disk-timeout-ac', 'hibernate-timeout-ac') { powercfg /change $t 0 }
}

# 5. Windows Update: no automatic restarts, monthly updates a week late,
# feature updates held at the release Setup installed, so Wild OS can test
# one before it arrives.
Step 'updates' $false {
    $release = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').DisplayVersion
    $wu = 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate'
    Set-Value $wu 'TargetReleaseVersion' 1
    Set-Value $wu 'ProductVersion' 'Windows 11' 'String'
    Set-Value $wu 'TargetReleaseVersionInfo' $release 'String'
    Set-Value $wu 'DeferQualityUpdates' 1
    Set-Value $wu 'DeferQualityUpdatesPeriodInDays' 7
    Set-Value "$wu\AU" 'NoAutoRebootWithLoggedOnUsers' 1
    Set-Value "$wu\AU" 'AUOptions' 2
}

# 6. Known folders point at the shared folders, so Save in any Windows app
# lands on Wild OS. These are the values Explorer's own Location tab writes.
# The drive only exists while a RemoteApp connection is open (wild-guest maps
# it on connect), and none is open during setup, so nothing here may touch it.
Step 'known-folders' $true {
    $usf = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders'
    $map = [ordered]@{
        'Documents' = @('Personal', '{F42EE2D3-909F-4907-8871-4C22FC0BF756}')
        'Downloads' = @('{374DE290-123F-4565-9164-39C4925E467B}', '{7D83EE9B-2244-4E70-B1F5-5393042AF1E4}')
        'Pictures'  = @('My Pictures', '{0DDD015D-B06C-45D5-8C4C-F59713854639}')
        'Desktop'   = @('Desktop', '{754AC886-DF64-4CBA-86B5-F7FBF4FBCEF5}')
    }
    foreach ($folder in $settings.folders) {
        $target = "$($drive):\$folder"
        foreach ($value in $map[$folder]) { Set-Value $usf $value $target 'ExpandString' }
    }
}

# 7. Office, once installed, saves to this PC rather than OneDrive and keeps
# its AutoRecover files beside the documents (brief 5.4 and 5.7). Policy
# values can be set before Office exists. wild-guest makes the AutoRecover
# folder once the drive is mapped.
Step 'office-policies' $false {
    $autorecover = "$($drive):\Documents\.autorecover"
    $op = 'HKCU:\Software\Policies\Microsoft\Office\16.0'
    Set-Value "$op\Common\General" 'PreferCloudSaveLocations' 0
    Set-Value "$op\Word\Options" 'DOC-PATH' "$($drive):\Documents" 'ExpandString'
    Set-Value "$op\Word\Options" 'AUTOSAVE-PATH' $autorecover 'ExpandString'
    Set-Value "$op\Excel\Options" 'DefaultPath' "$($drive):\Documents" 'ExpandString'
    Set-Value "$op\Excel\Options" 'AutoRecoverPath' $autorecover 'ExpandString'
    Set-Value "$op\PowerPoint\RecentFolderList" 'Default' "$($drive):\Documents" 'ExpandString'
    Set-Value 'HKLM:\SOFTWARE\Policies\Microsoft\Office\16.0\Common\OfficeUpdate' 'EnableAutomaticUpdates' 0
    Set-Value 'HKLM:\SOFTWARE\Policies\Microsoft\Office\16.0\Common\OfficeUpdate' 'HideEnableDisableUpdates' 1

    # Start-up speed (brief 5.5a): straight to a blank document with no
    # Start screen, no first-run or "what's new" pages, and Office's own
    # background tasks off, so an app opens as fast as it can.
    foreach ($app in 'Word', 'Excel', 'PowerPoint') {
        Set-Value "$op\$app\Options" 'DisableBootToOfficeStart' 1
    }
    Set-Value "$op\Firstrun" 'BootedRTM' 1
    Set-Value "$op\Firstrun" 'DisableMovie' 1
    Set-Value "$op\Common\General" 'ShownFirstRunOptin' 1
    Set-Value "$op\Common\General" 'DisableFirstRunExperience' 1
    Set-Value "$op\Common\Feedback" 'Enabled' 0
    Set-Value "$op\Common\PTWatson" 'PTWOptIn' 0
    Set-Value "$op\Common\LinkedIn" 'OfficeLinkedIn' 0
    Get-ScheduledTask -TaskPath '\Microsoft\Office\' -ErrorAction SilentlyContinue |
        Where-Object { $_.TaskName -match 'Telemetry|Feature Updates|Subscription Heartbeat' } |
        Disable-ScheduledTask -ErrorAction SilentlyContinue | Out-Null
}

# Windows start-up trimming (brief 5.5a): nothing starts with the session
# that Windows mode does not need, so a cold app has the whole machine.
Step 'startup-trim' $false {
    foreach ($run in 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run', 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run') {
        $values = Get-Item $run -ErrorAction SilentlyContinue
        if ($values) {
            foreach ($name in $values.GetValueNames()) {
                if ($name -notin 'SecurityHealth') { Remove-ItemProperty -Path $run -Name $name -ErrorAction SilentlyContinue }
            }
        }
    }
    $tasks = '\Microsoft\Windows\Application Experience\Microsoft Compatibility Appraiser',
             '\Microsoft\Windows\Application Experience\ProgramDataUpdater',
             '\Microsoft\Windows\Application Experience\StartupAppTask',
             '\Microsoft\Windows\Customer Experience Improvement Program\Consolidator',
             '\Microsoft\Windows\Customer Experience Improvement Program\UsbCeip',
             '\Microsoft\Windows\DiskDiagnostic\Microsoft-Windows-DiskDiagnosticDataCollector',
             '\Microsoft\Windows\Feedback\Siuf\DmClient',
             '\Microsoft\Windows\Feedback\Siuf\DmClientOnScenarioDownload',
             '\Microsoft\Windows\Maps\MapsUpdateTask',
             '\Microsoft\Windows\Maps\MapsToastTask',
             '\Microsoft\Windows\Windows Error Reporting\QueueReporting',
             '\Microsoft\XblGameSave\XblGameSaveTask'
    foreach ($t in $tasks) {
        $path = $t.Substring(0, $t.LastIndexOf('\') + 1)
        Disable-ScheduledTask -TaskPath $path -TaskName $t.Substring($t.LastIndexOf('\') + 1) -ErrorAction SilentlyContinue | Out-Null
    }
    # Telemetry upload, push routing for it, maps, Xbox, fax, the retail
    # demo and the compatibility assistant (which checks every program as it
    # starts). Defender, Windows Update and everything RemoteApp needs stay.
    foreach ($svc in 'DiagTrack', 'dmwappushservice', 'MapsBroker', 'XblAuthManager', 'XblGameSave', 'XboxNetApiSvc', 'XboxGipSvc',
                     'Fax', 'RetailDemo', 'PcaSvc', 'WerSvc') {
        Stop-Service $svc -Force -ErrorAction SilentlyContinue
        Set-Service $svc -StartupType Disabled -ErrorAction SilentlyContinue
    }
}

# 8. Wild OS's light or dark look and accent colour (brief 5.4 step 8):
# wild-guest applies theme.json in each session and keeps it up to date.
Step 'theme' $false {
    if ($settings.theme) {
        $settings.theme | ConvertTo-Json -Compress | Set-Content -Encoding UTF8 "$state\theme.json"
        $p = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
        $light = if ($settings.theme.dark) { 0 } else { 1 }
        Set-Value $p 'AppsUseLightTheme' $light
        Set-Value $p 'SystemUsesLightTheme' $light
    }
}

# 9. After the reboot a start-up task writes ready.json; the host waits for
# it through the guest agent.
Step 'ready-task' $true {
    $ready = @'
$state = 'C:\ProgramData\WildOS'
$settings = Get-Content "$state\settings.json" -Raw | ConvertFrom-Json
$deadline = (Get-Date).AddSeconds(180)
while ((Get-Date) -lt $deadline) {
    $rdp = Get-NetTCPConnection -LocalPort 3389 -State Listen -ErrorAction SilentlyContinue
    if ($rdp) { break }
    Start-Sleep -Seconds 2
}
$cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
@{
    state = 'ready'
    edition = $cv.EditionID
    build = "$($cv.CurrentBuild).$($cv.UBR)"
    display_version = $cv.DisplayVersion
    rdp_listening = [bool](Get-NetTCPConnection -LocalPort 3389 -State Listen -ErrorAction SilentlyContinue)
    shared_folder = [bool](Test-Path "$($settings.drive):\Documents")
    guest_agent = (Get-Service QEMU-GA).Status.ToString()
    boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToString('s')
} | ConvertTo-Json | Set-Content -Encoding UTF8 "$state\ready.json"
'@
    Set-Content -Encoding UTF8 -Path "$state\ready.ps1" -Value $ready
    Copy-Item "$here\settings.json" "$state\settings.json" -Force
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$state\ready.ps1`""
    $trigger = New-ScheduledTaskTrigger -AtStartup
    $principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
    $taskSettings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 10)
    Register-ScheduledTask -TaskName 'Wild OS ready' -Action $action -Trigger $trigger -Principal $principal -Settings $taskSettings -Force | Out-Null
}

Stage 'reboot' 'running' 'restarting to finish'
Stop-Transcript | Out-Null
shutdown.exe /r /t 5 /c "Wild OS is finishing Windows setup"
