#include "feedback.h"

Activation::Activation(::org_kde_plasma_activation *object)
    : QtWayland::org_kde_plasma_activation(object)
{
}

Activation::~Activation()
{
    destroy();
}

void Activation::org_kde_plasma_activation_app_id(const QString &appId)
{
    Q_EMIT this->appId(appId);
}

void Activation::org_kde_plasma_activation_finished()
{
    Q_EMIT finished();
}

ActivationFeedback::ActivationFeedback()
    : QWaylandClientExtensionTemplate(1)
{
    connect(this, &QWaylandClientExtension::activeChanged, this, [this] {
        if (!isActive()) {
            destroy();
        }
    });
    initialize();
}

ActivationFeedback::~ActivationFeedback()
{
    if (isActive()) {
        destroy();
    }
}

void ActivationFeedback::org_kde_plasma_activation_feedback_activation(::org_kde_plasma_activation *id)
{
    Q_EMIT activation(new Activation(id));
}
