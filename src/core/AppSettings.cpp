#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    load();
}

QString AppSettings::dataDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString dir = base.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data")
        : base + QStringLiteral("/LlamaDeck");
    QDir().mkpath(dir);
    return dir;
}

QString AppSettings::configFilePath() const
{
    return dataDir() + QStringLiteral("/settings.ini");
}

QString AppSettings::historyFilePath() const
{
    return dataDir() + QStringLiteral("/history.json");
}

QString AppSettings::detectServerPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    candidates << appDir + QStringLiteral("/llama-server.exe")
               << appDir + QStringLiteral("/bin/llama-server.exe")
               << appDir + QStringLiteral("/llama.cpp/llama-server.exe")
               << QDir::currentPath() + QStringLiteral("/llama-server.exe");

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("llama-server"));
    if (!fromPath.isEmpty())
        candidates << fromPath;

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QDir::toNativeSeparators(candidate);
    }
    return QString();
}

void AppSettings::load()
{
    QSettings s(configFilePath(), QSettings::IniFormat);

    llamaServerPath = s.value(QStringLiteral("runtime/serverPath")).toString();
    modelsDir = s.value(QStringLiteral("runtime/modelsDir")).toString();
    apiKey = s.value(QStringLiteral("runtime/apiKey")).toString();
    extraArgs = s.value(QStringLiteral("runtime/extraArgs")).toString();
    port = s.value(QStringLiteral("runtime/port"), 8080).toInt();
    ctx = s.value(QStringLiteral("runtime/ctx"), 4096).toInt();
    gpuLayers = s.value(QStringLiteral("runtime/gpuLayers"), 99).toInt();
    threads = s.value(QStringLiteral("runtime/threads"), 0).toInt();
    batch = s.value(QStringLiteral("runtime/batch"), 2048).toInt();
    ubatch = s.value(QStringLiteral("runtime/ubatch"), 512).toInt();
    parallel = s.value(QStringLiteral("runtime/parallel"), 1).toInt();
    kvCacheType = s.value(QStringLiteral("runtime/kvCacheType"), QStringLiteral("f16")).toString();
    mlock = s.value(QStringLiteral("runtime/mlock"), false).toBool();
    noMmap = s.value(QStringLiteral("runtime/noMmap"), false).toBool();
    flashAttn = s.value(QStringLiteral("runtime/flashAttn"), true).toBool();
    jinja = s.value(QStringLiteral("runtime/jinja"), true).toBool();
    autoStartLast = s.value(QStringLiteral("startup/autoStartLast"), false).toBool();
    autoOpenBrowser = s.value(QStringLiteral("startup/autoOpenBrowser"), false).toBool();
    autoOpenApiTab = s.value(QStringLiteral("startup/autoOpenApiTab"), false).toBool();
    stopOnExit = s.value(QStringLiteral("startup/stopOnExit"), true).toBool();
    controlApiEnabled = s.value(QStringLiteral("controlApi/enabled"), true).toBool();
    controlApiPort = s.value(QStringLiteral("controlApi/port"), 8745).toInt();
    lastModelPath = s.value(QStringLiteral("runtime/lastModelPath")).toString();

    if (llamaServerPath.isEmpty())
        llamaServerPath = detectServerPath();

    if (modelsDir.isEmpty()) {
        const QString localModels = QCoreApplication::applicationDirPath() + QStringLiteral("/models");
        if (QDir(localModels).exists())
            modelsDir = QDir::toNativeSeparators(localModels);
        else
            modelsDir = QDir::toNativeSeparators(
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/models"));
    }
}

void AppSettings::save()
{
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("runtime/serverPath"), llamaServerPath);
    s.setValue(QStringLiteral("runtime/modelsDir"), modelsDir);
    s.setValue(QStringLiteral("runtime/apiKey"), apiKey);
    s.setValue(QStringLiteral("runtime/extraArgs"), extraArgs);
    s.setValue(QStringLiteral("runtime/port"), port);
    s.setValue(QStringLiteral("runtime/ctx"), ctx);
    s.setValue(QStringLiteral("runtime/gpuLayers"), gpuLayers);
    s.setValue(QStringLiteral("runtime/threads"), threads);
    s.setValue(QStringLiteral("runtime/batch"), batch);
    s.setValue(QStringLiteral("runtime/ubatch"), ubatch);
    s.setValue(QStringLiteral("runtime/parallel"), parallel);
    s.setValue(QStringLiteral("runtime/kvCacheType"), kvCacheType);
    s.setValue(QStringLiteral("runtime/mlock"), mlock);
    s.setValue(QStringLiteral("runtime/noMmap"), noMmap);
    s.setValue(QStringLiteral("runtime/flashAttn"), flashAttn);
    s.setValue(QStringLiteral("runtime/jinja"), jinja);
    s.setValue(QStringLiteral("startup/autoStartLast"), autoStartLast);
    s.setValue(QStringLiteral("startup/autoOpenBrowser"), autoOpenBrowser);
    s.setValue(QStringLiteral("startup/autoOpenApiTab"), autoOpenApiTab);
    s.setValue(QStringLiteral("startup/stopOnExit"), stopOnExit);
    s.setValue(QStringLiteral("controlApi/enabled"), controlApiEnabled);
    s.setValue(QStringLiteral("controlApi/port"), controlApiPort);
    s.setValue(QStringLiteral("runtime/lastModelPath"), lastModelPath);
    s.sync();
    emit changed();
}
