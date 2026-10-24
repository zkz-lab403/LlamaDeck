#pragma once

#include <QObject>
#include <QString>

class AppSettings : public QObject {
    Q_OBJECT

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString llamaServerPath;
    QString modelsDir;
    QString apiKey;
    QString extraArgs;

    int port = 8080;
    int ctx = 4096;
    int gpuLayers = 99;
    int threads = 0;
    int batch = 2048;
    int ubatch = 512;
    int parallel = 1;
    QString kvCacheType = QStringLiteral("f16");
    bool mlock = false;
    bool noMmap = false;

    bool flashAttn = true;
    bool jinja = true;
    bool autoStartLast = false;
    bool autoOpenBrowser = false;
    bool autoOpenApiTab = false;
    bool stopOnExit = true;
    bool controlApiEnabled = true;
    int controlApiPort = 8745;

    QString lastModelPath;

    QString dataDir() const;
    QString configFilePath() const;
    QString historyFilePath() const;
    QString detectServerPath() const;

    void load();
    void save();

signals:
    void changed();
};
