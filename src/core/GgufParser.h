#pragma once

#include <QString>

struct GgufInfo {
    QString name;
    QString architecture;
    quint64 parameterCount = 0;
    quint64 contextLength = 0;
    int blockCount = 0;
    bool valid = false;
};

class GgufParser {
public:
    static GgufInfo parse(const QString &filePath);
};
