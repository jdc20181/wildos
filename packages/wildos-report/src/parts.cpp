#include "parts.h"

#include <QFileInfo>
#include <QVariantMap>

namespace Parts
{
const QList<Part> &all()
{
    static const QList<Part> parts{
        {QStringLiteral("files"), QStringLiteral("Files"), {QStringLiteral("wildos-files")}, QStringLiteral("wildos-files")},
        {QStringLiteral("settings"), QStringLiteral("Settings"), {QStringLiteral("wildos-settings")}, QStringLiteral("wildos-settings")},
        {QStringLiteral("store"), QStringLiteral("Store"), {QStringLiteral("wildos-store"), QStringLiteral("packagekitd"), QStringLiteral("flatpak")},
         QStringLiteral("wildos-store")},
        {QStringLiteral("mail"), QStringLiteral("Mail"),
         {QStringLiteral("wildos-mail"), QStringLiteral("evolution-source-registry"), QStringLiteral("evolution-addressbook-factory")},
         QStringLiteral("wildos-mail")},
        {QStringLiteral("calendar"), QStringLiteral("Calendar"),
         {QStringLiteral("wildos-calendar"), QStringLiteral("wildos-mail"), QStringLiteral("evolution-calendar-factory")}, QStringLiteral("wildos-mail")},
        {QStringLiteral("remote"), QStringLiteral("Remote"), {QStringLiteral("wildos-remote")}, QStringLiteral("wildos-remote")},
        {QStringLiteral("pdf"), QStringLiteral("PDF"), {QStringLiteral("wildos-pdf")}, QStringLiteral("wildos-pdf")},
        {QStringLiteral("notes"), QStringLiteral("Notes"), {QStringLiteral("wildos-notes")}, QStringLiteral("wildos-notes")},
        {QStringLiteral("capture"), QStringLiteral("Capture"), {QStringLiteral("wildos-capture"), QStringLiteral("wildos-screenshot")},
         QStringLiteral("wildos-capture")},
        {QStringLiteral("music"), QStringLiteral("Music"), {QStringLiteral("wildos-music")}, QStringLiteral("wildos-music")},
        {QStringLiteral("videos"), QStringLiteral("Videos"), {QStringLiteral("wildos-videos")}, QStringLiteral("wildos-videos")},
        {QStringLiteral("taskmanager"), QStringLiteral("Task Manager"), {QStringLiteral("wildos-taskmanager")}, QStringLiteral("wildos-taskmanager")},
        {QStringLiteral("terminal"), QStringLiteral("Terminal"), {QStringLiteral("wildos-terminal")}, QStringLiteral("wildos-terminal")},
        {QStringLiteral("office"), QStringLiteral("Write and Tables"), {QStringLiteral("soffice.bin"), QStringLiteral("oosplash")},
         QStringLiteral("wildos-office")},
        {QStringLiteral("windows"), QStringLiteral("Windows mode"),
         {QStringLiteral("wildos-winmode"), QStringLiteral("wildos-winmode-agent"), QStringLiteral("wildos-rdp"), QStringLiteral("wild-run"),
          QStringLiteral("wildos-appbridge")},
         QStringLiteral("wildos-winmode")},
        {QStringLiteral("desktop"), QStringLiteral("Desktop, Start and taskbar"),
         {QStringLiteral("plasmashell"), QStringLiteral("kwin_wayland"), QStringLiteral("ksmserver"), QStringLiteral("startplasma-wayland")},
         QStringLiteral("wildos-shell")},
        {QStringLiteral("signin"), QStringLiteral("Signing in and the lock screen"),
         {QStringLiteral("sddm"), QStringLiteral("sddm-greeter-qt6"), QStringLiteral("kscreenlocker_greet")}, QStringLiteral("wildos-branding")},
        {QStringLiteral("network"), QStringLiteral("Wi-Fi and network"),
         {QStringLiteral("NetworkManager"), QStringLiteral("wpa_supplicant"), QStringLiteral("nm-applet")}, {}},
        {QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"), {QStringLiteral("bluetoothd")}, {}},
        {QStringLiteral("sound"), QStringLiteral("Sound"),
         {QStringLiteral("pipewire"), QStringLiteral("wireplumber"), QStringLiteral("pipewire-pulse")}, {}},
        {QStringLiteral("updates"), QStringLiteral("Updates"),
         {QStringLiteral("packagekitd"), QStringLiteral("unattended-upgrade"), QStringLiteral("apt"), QStringLiteral("apt-get"),
          QStringLiteral("wildos-store")},
         {}},
        {QStringLiteral("welcome"), QStringLiteral("Welcome"), {QStringLiteral("wildos-welcome")}, QStringLiteral("wildos-welcome")},
        {QStringLiteral("system"), QStringLiteral("Starting up, sleep or shutting down"), {}, {}},
        {QStringLiteral("other"), QStringLiteral("Something else"), {}, {}},
    };
    return parts;
}

const Part &byId(const QString &id)
{
    for (const Part &part : all()) {
        if (part.id == id) {
            return part;
        }
    }
    return all().last();
}

bool known(const QString &id)
{
    for (const Part &part : all()) {
        if (part.id == id) {
            return true;
        }
    }
    return false;
}

const Part &byProgram(const QString &program)
{
    const QString name = QFileInfo(program).fileName();
    for (const Part &part : all()) {
        if (part.programs.contains(name)) {
            return part;
        }
    }
    return all().last();
}

QVariantList choices()
{
    QVariantList list;
    for (const Part &part : all()) {
        list.append(QVariantMap{{QStringLiteral("value"), part.id}, {QStringLiteral("text"), part.name}});
    }
    return list;
}
}
