<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<!--
  Wild OS wording for Calamares' own strings, merged over Calamares' own
  catalogue because Calamares asks its own translations before any branding
  one. Rebuild after editing (overrides first, so they win):
    lrelease wildos-overrides.ts <calamares-3.3.14>/lang/calamares_en_GB.ts -qm calamares_en_GB.qm
    lrelease wildos-overrides.ts <calamares-3.3.14>/lang/calamares_en.ts -qm calamares_en.qm
  The .qm files belong in /usr/share/calamares/lang/, which Calamares reads
  before its built-in catalogue.
-->
<TS version="2.1" language="en_GB">
<context>
    <name>ChoicePage</name>
    <message>
        <source>Select storage de&amp;vice:</source>
        <comment>@label</comment>
        <translation>&amp;Disk</translation>
    </message>
    <message>
        <source>EFI system partition:</source>
        <comment>@label</comment>
        <translation>Start-up partition:</translation>
    </message>
    <message>
        <source>This storage device has one of its partitions &lt;strong&gt;mounted&lt;/strong&gt;.</source>
        <comment>@info</comment>
        <translation>&lt;span style=&quot;font-size:14px; color:#c9cdd8;&quot;&gt;Part of this disk is in use by the live session, so Wild OS cannot change it. Pick another disk.&lt;/span&gt;</translation>
    </message>
    <message>
        <source>This storage device is a part of an &lt;strong&gt;inactive RAID&lt;/strong&gt; device.</source>
        <comment>@info</comment>
        <translation>&lt;span style=&quot;font-size:14px; color:#c9cdd8;&quot;&gt;This disk belongs to a RAID set that is not running, so Wild OS cannot change it. Pick another disk.&lt;/span&gt;</translation>
    </message>
    <message>
        <source>No swap</source>
        <comment>@label</comment>
        <translation>No swap</translation>
    </message>
    <message>
        <source>Swap to file</source>
        <comment>@label</comment>
        <translation>Swap file</translation>
    </message>
    <message>
        <source>Bootloader location:</source>
        <comment>@label</comment>
        <translation>Install the boot loader on:</translation>
    </message>
</context>
<context>
    <name>EncryptWidget</name>
    <message>
        <source>En&amp;crypt system</source>
        <translation>&amp;Encrypt Wild OS</translation>
    </message>
    <message>
        <source>Your system does not seem to support encryption well enough to encrypt the entire system. You may enable encryption, but performance may suffer.</source>
        <translation>This computer lacks hardware support for encryption, so it may run a little slower with it on.</translation>
    </message>
    <message>
        <source>Passphrase</source>
        <translation>Disk password</translation>
    </message>
    <message>
        <source>Confirm passphrase</source>
        <translation>Confirm disk password</translation>
    </message>
    <message>
        <source>Please enter the same passphrase in both boxes.</source>
        <comment>@tooltip</comment>
        <translation>The two disk passwords do not match.</translation>
    </message>
    <message>
        <source>Password must be a minimum of %1 characters.</source>
        <comment>@tooltip</comment>
        <translation>Use at least %1 characters.</translation>
    </message>
</context>
<context>
    <name>PartitionViewStep</name>
    <message>
        <source>Partitions</source>
        <comment>@label</comment>
        <translation>Where to install</translation>
    </message>
    <message>
        <source>Install %1 &lt;strong&gt;alongside&lt;/strong&gt; another operating system</source>
        <comment>@label</comment>
        <translation>Install %1 alongside the system already here</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Erase&lt;/strong&gt; disk and install %1</source>
        <comment>@label</comment>
        <translation>Replace everything on the disk with %1</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Replace&lt;/strong&gt; a partition with %1</source>
        <comment>@label</comment>
        <translation>Replace one partition with %1</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Manual&lt;/strong&gt; partitioning</source>
        <comment>@label</comment>
        <translation>Your own partition layout</translation>
    </message>
    <message>
        <source>Install %1 &lt;strong&gt;alongside&lt;/strong&gt; another operating system on disk &lt;strong&gt;%2&lt;/strong&gt; (%3)</source>
        <comment>@info</comment>
        <translation>Install %1 alongside the system already on &lt;strong&gt;%2&lt;/strong&gt; (%3)</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Erase&lt;/strong&gt; disk &lt;strong&gt;%2&lt;/strong&gt; (%3) and install %1</source>
        <comment>@info</comment>
        <translation>Replace everything on &lt;strong&gt;%2&lt;/strong&gt; (%3) with %1</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Replace&lt;/strong&gt; a partition on disk &lt;strong&gt;%2&lt;/strong&gt; (%3) with %1</source>
        <comment>@info</comment>
        <translation>Replace one partition on &lt;strong&gt;%2&lt;/strong&gt; (%3) with %1</translation>
    </message>
    <message>
        <source>&lt;strong&gt;Manual&lt;/strong&gt; partitioning on disk &lt;strong&gt;%1&lt;/strong&gt; (%2)</source>
        <comment>@info</comment>
        <translation>Your own partition layout on &lt;strong&gt;%1&lt;/strong&gt; (%2)</translation>
    </message>
    <message>
        <source>Current:</source>
        <comment>@label</comment>
        <translation>Now</translation>
    </message>
    <message>
        <source>After:</source>
        <comment>@label</comment>
        <translation>After</translation>
    </message>
    <message>
        <source>There are no partitions to install on.</source>
        <translation>There is no disk Wild OS can be installed on.</translation>
    </message>
</context>
<context>
    <name>Calamares::ViewManager</name>
    <message>
        <source>Continue with Installation?</source>
        <comment>@title</comment>
        <translation>Install Wild OS now?</translation>
    </message>
    <message>
        <source>The %1 installer is about to make changes to your disk in order to install %2.&lt;br/&gt;&lt;strong&gt;You will not be able to undo these changes.&lt;/strong&gt;</source>
        <comment>%1 is short product name, %2 is short product name with version</comment>
        <translation>%1 is about to change your disk to install %2.&lt;br/&gt;&lt;strong&gt;This cannot be undone.&lt;/strong&gt;</translation>
    </message>
    <message>
        <source>&amp;Install Now</source>
        <comment>@button</comment>
        <translation>Install</translation>
    </message>
    <message>
        <source>Go &amp;Back</source>
        <comment>@button</comment>
        <translation>Go back</translation>
    </message>
    <message>
        <source>Cancel Installation?</source>
        <comment>@title</comment>
        <translation>Close the installer?</translation>
    </message>
    <message>
        <source>Do you really want to cancel the current install process?
The installer will quit and all changes will be lost.</source>
        <translation>Nothing you chose here is kept, and your disk stays as it is.</translation>
    </message>
</context>
</TS>
