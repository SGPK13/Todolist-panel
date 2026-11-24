#include "Panel.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TodolistPanel"));
    QApplication::setApplicationDisplayName(QStringLiteral("待办"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("TodolistPanel"), QStringLiteral("TodolistPanel"));
    const QString dataDirectory = QFileInfo(settings.fileName()).absolutePath();
    QLockFile instanceLock(settings.fileName() + QStringLiteral(".instance"));
    if (!QDir().mkpath(dataDirectory) || !instanceLock.tryLock(0)) {
        const QString message = instanceLock.error() == QLockFile::LockFailedError
            ? QStringLiteral("待办面板已经在运行。")
            : QStringLiteral("无法访问待办的本地数据目录。\n%1").arg(QDir::toNativeSeparators(dataDirectory));
        QMessageBox::information(nullptr, QApplication::applicationDisplayName(), message);
        return 1;
    }

    Panel panel(settings);
    panel.show();
    return app.exec();
}
