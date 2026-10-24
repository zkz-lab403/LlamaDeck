#include "ModelStore.h"

#include "GgufParser.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace {
QString stripExtension(const QString &fileName)
{
    QString base = fileName;
    if (base.endsWith(QLatin1String(".gguf"), Qt::CaseInsensitive))
        base.chop(5);
    return base;
}
} // namespace

QVector<ModelInfo> ModelStore::scan(const QString &directory)
{
    QVector<ModelInfo> models;
    if (directory.isEmpty())
        return models;

    QDir dir(directory);
    if (!dir.exists())
        return models;

    QDirIterator it(directory, QStringList{QStringLiteral("*.gguf"), QStringLiteral("*.GGUF")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QFileInfo fi(filePath);

        ModelInfo info;
        info.path = QDir::toNativeSeparators(fi.absoluteFilePath());
        info.fileName = fi.fileName();
        info.sizeBytes = quint64(fi.size());
        info.quantization = detectQuantization(fi.fileName());

        const GgufInfo gguf = GgufParser::parse(info.path);
        info.metadataParsed = gguf.valid;
        info.name = gguf.name.trimmed();
        info.architecture = gguf.architecture;
        info.parameterCount = gguf.parameterCount;
        info.contextLength = gguf.contextLength;
        info.blockCount = gguf.blockCount;

        if (info.name.isEmpty())
            info.name = stripExtension(fi.fileName());

        models.append(info);
    }

    std::sort(models.begin(), models.end(), [](const ModelInfo &a, const ModelInfo &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    return models;
}

QString ModelStore::detectQuantization(const QString &fileName)
{
    static const QRegularExpression rx(
        QStringLiteral("(?i)(IQ[0-9]+_[A-Z0-9]+|Q[0-9]+_K(?:_[A-Z0-9]+)?|Q[0-9]+_[0-9]+|BF16|F16|F32)"));

    auto it = rx.globalMatch(fileName);
    QString last;
    while (it.hasNext()) {
        const auto match = it.next();
        last = match.captured(1).toUpper();
    }
    return last;
}

QString ModelStore::formatSize(quint64 bytes)
{
    if (bytes == 0)
        return QStringLiteral("—");
    const double gb = double(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', gb >= 10.0 ? 1 : 2);
    const double mb = double(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', mb >= 10.0 ? 0 : 1);
}

QString ModelStore::formatParams(quint64 parameters)
{
    if (parameters == 0)
        return QStringLiteral("—");
    const double b = double(parameters) / 1.0e9;
    if (b >= 1.0)
        return QStringLiteral("%1B").arg(b, 0, 'f', b >= 100.0 ? 0 : 1);
    const double m = double(parameters) / 1.0e6;
    return QStringLiteral("%1M").arg(m, 0, 'f', m >= 100.0 ? 0 : 1);
}

QString ModelStore::formatContext(quint64 context)
{
    if (context == 0)
        return QStringLiteral("—");
    if (context >= 1024 && context % 1024 == 0)
        return QStringLiteral("%1K").arg(context / 1024);
    return QString::number(context);
}
