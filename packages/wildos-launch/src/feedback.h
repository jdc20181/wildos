#pragma once

#include <QObject>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

#include "qwayland-plasma-window-management.h"

// One app being started, as KWin announces it: its app id (the desktop
// file name) arrives first, then "finished" once a window of it was
// activated or another launch took over.
class Activation : public QObject, public QtWayland::org_kde_plasma_activation
{
    Q_OBJECT

public:
    explicit Activation(::org_kde_plasma_activation *object);
    ~Activation() override;

Q_SIGNALS:
    void appId(const QString &appId);
    void finished();

protected:
    void org_kde_plasma_activation_app_id(const QString &appId) override;
    void org_kde_plasma_activation_finished() override;
};

// KWin's launch feedback, the same source the taskbar's starting line uses.
// It is a privileged interface: KWin grants it only to programs whose
// desktop file lists it in X-KDE-Wayland-Interfaces.
class ActivationFeedback : public QWaylandClientExtensionTemplate<ActivationFeedback>, public QtWayland::org_kde_plasma_activation_feedback
{
    Q_OBJECT

public:
    ActivationFeedback();
    ~ActivationFeedback() override;

Q_SIGNALS:
    void activation(Activation *activation);

protected:
    void org_kde_plasma_activation_feedback_activation(::org_kde_plasma_activation *id) override;
};
