#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LlamaDeck"));
    QCoreApplication::setApplicationName(QStringLiteral("LlamaDeck"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    app.setStyle(QStringLiteral("Fusion"));
    app.setPalette(Theme::palette());
    app.setFont(Theme::applicationFont());
    app.setStyleSheet(Theme::styleSheet());
    app.setWindowIcon(Icons::appIcon());

    MainWindow window;
    window.show();
    return app.exec();
}
