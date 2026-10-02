#include <QApplication>
#include <QCoreApplication>

#include "app/WelcomeWindow/welcomeform.h"
#include "ToolTabs/Canvas/trace_explorer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("cremniy");
    QCoreApplication::setApplicationName("Cremniy");
    a.setWindowIcon(QIcon(":/icons/icon.png"));

    QFile file(":/styles/style.qss");
    file.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(file.readAll());
    a.setStyleSheet(styleSheet);

    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--trace-prototype") {
        TraceExplorer explorer;
        QString error;
        if (!explorer.openManifest(QString::fromLocal8Bit(argv[2]), &error)) {
            qCritical() << error;
            return 2;
        }
        explorer.show();
        return QCoreApplication::exec();
    }

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}
