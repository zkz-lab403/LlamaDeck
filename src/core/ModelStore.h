#pragma once

#include <QString>
#include <QVector>

struct ModelInfo {
    QString path;
    QString fileName;
    QString name;
    QString architecture;
    QString quantization;
    quint64 sizeBytes = 0;
    quint64 parameterCount = 0;
    quint64 contextLength = 0;
    int blockCount = 0;
    bool metadataParsed = false;
};

class ModelStore {
public:
    static QVector<ModelInfo> scan(const QString &directory);
    static QString detectQuantization(const QString &fileName);
    static QString formatSize(quint64 bytes);
    static QString formatParams(quint64 parameters);
    static QString formatContext(quint64 context);
};
