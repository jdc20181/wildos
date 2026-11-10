#include "lighting.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-lighting"));
    Lighting lighting;
    return app.exec();
}
