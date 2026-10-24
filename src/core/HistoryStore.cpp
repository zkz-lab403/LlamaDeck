#include "HistoryStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

namespace {

QJsonObject optionsToJson(const LaunchOptions &o)
{
    QJsonObject obj;
    obj[QStringLiteral("modelPath")] = o.modelPath;
    obj[QStringLiteral("port")] = o.port;
    obj[QStringLiteral("ctx")] = o.ctx;
    obj[QStringLiteral("gpuLayers")] = o.gpuLayers;
    obj[QStringLiteral("threads")] = o.threads;
    obj[QStringLiteral("flashAttn")] = o.flashAttn;
    obj[QStringLiteral("jinja")] = o.jinja;
    obj[QStringLiteral("apiKey")] = o.apiKey;
    obj[QStringLiteral("extraArgs")] = o.extraArgs;
    return obj;
}

LaunchOptions optionsFromJson(const QJsonObject &obj)
{
    LaunchOptions o;
    o.modelPath = obj.value(QStringLiteral("modelPath")).toString();
    o.port = obj.value(QStringLiteral("port")).toInt(8080);
    o.ctx = obj.value(QStringLiteral("ctx")).toInt(4096);
    o.gpuLayers = obj.value(QStringLiteral("gpuLayers")).toInt(99);
    o.threads = obj.value(QStringLiteral("threads")).toInt(0);
    o.flashAttn = obj.value(QStringLiteral("flashAttn")).toBool(true);
    o.jinja = obj.value(QStringLiteral("jinja")).toBool(true);
    o.apiKey = obj.value(QStringLiteral("apiKey")).toString();
    o.extraArgs = obj.value(QStringLiteral("extraArgs")).toString();
    return o;
}

QJsonObject entryToJson(const HistoryEntry &e)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = e.id;
    obj[QStringLiteral("startedAt")] = e.startedAt.toString(Qt::ISODateWithMs);
    obj[QStringLiteral("endedAt")] = e.endedAt.isValid() ? e.endedAt.toString(Qt::ISODateWithMs) : QString();
    obj[QStringLiteral("modelName")] = e.modelName;
    obj[QStringLiteral("modelPath")] = e.modelPath;
    obj[QStringLiteral("command")] = e.command;
    obj[QStringLiteral("status")] = e.status;
    obj[QStringLiteral("avgTps")] = e.avgTps;
    obj[QStringLiteral("peakTps")] = e.peakTps;
    obj[QStringLiteral("tokens")] = static_cast<double>(e.tokens);
    obj[QStringLiteral("durationSec")] = static_cast<double>(e.durationSec);
    obj[QStringLiteral("options")] = optionsToJson(e.options);
    return obj;
}

HistoryEntry entryFromJson(const QJsonObject &obj)
{
    HistoryEntry e;
    e.id = obj.value(QStringLiteral("id")).toString();
    e.startedAt = QDateTime::fromString(obj.value(QStringLiteral("startedAt")).toString(), Qt::ISODateWithMs);
    const QString ended = obj.value(QStringLiteral("endedAt")).toString();
    if (!ended.isEmpty())
        e.endedAt = QDateTime::fromString(ended, Qt::ISODateWithMs);
    e.modelName = obj.value(QStringLiteral("modelName")).toString();
    e.modelPath = obj.value(QStringLiteral("modelPath")).toString();
    e.command = obj.value(QStringLiteral("command")).toString();
    e.status = obj.value(QStringLiteral("status")).toString();
    e.avgTps = obj.value(QStringLiteral("avgTps")).toDouble();
    e.peakTps = obj.value(QStringLiteral("peakTps")).toDouble();
    e.tokens = quint64(obj.value(QStringLiteral("tokens")).toDouble());
    e.durationSec = qint64(obj.value(QStringLiteral("durationSec")).toDouble());
    e.options = optionsFromJson(obj.value(QStringLiteral("options")).toObject());
    return e;
}

QString escapeCsv(const QString &value)
{
    QString v = value;
    v.replace(QLatin1Char('"'), QLatin1String("\"\""));
    if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
        || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r')))
        v = QLatin1Char('"') + v + QLatin1Char('"');
    return v;
}

} // namespace

HistoryStore::HistoryStore(QObject *parent)
    : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString dir = base.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data")
        : base + QStringLiteral("/LlamaDeck");
    m_filePath = dir + QStringLiteral("/history.json");
    load();
}

QString HistoryStore::begin(const LaunchOptions &options, const QString &modelName, const QString &command)
{
    HistoryEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.startedAt = QDateTime::currentDateTime();
    entry.modelName = modelName;
    entry.modelPath = options.modelPath;
    entry.command = command;
    entry.status = QStringLiteral("启动中");
    entry.options = options;

    m_entries.prepend(entry);
    while (m_entries.size() > 300)
        m_entries.removeLast();

    save();
    emit changed();
    return entry.id;
}

void HistoryStore::markRunning(const QString &id)
{
    for (auto &entry : m_entries) {
        if (entry.id == id) {
            entry.status = QStringLiteral("运行中");
            save();
            emit changed();
            return;
        }
    }
}

void HistoryStore::finish(const QString &id, const QString &status, double avgTps, double peakTps, quint64 tokens)
{
    for (auto &entry : m_entries) {
        if (entry.id == id) {
            entry.endedAt = QDateTime::currentDateTime();
            entry.durationSec = entry.startedAt.secsTo(entry.endedAt);
            entry.status = status;
            entry.avgTps = avgTps;
            entry.peakTps = peakTps;
            entry.tokens = tokens;
            save();
            emit changed();
            return;
        }
    }
}

void HistoryStore::clear()
{
    m_entries.clear();
    save();
    emit changed();
}

bool HistoryStore::exportCsv(const QString &filePath, QString *error) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QChar(0xFEFF);
    stream << QStringLiteral("时间,模型,端口,上下文,GPU层数,运行时长(秒),平均速度(t/s),峰值速度(t/s),生成tokens,状态,命令\n");

    for (const HistoryEntry &e : m_entries) {
        const QStringList fields{
            e.startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            e.modelName,
            QString::number(e.options.port),
            QString::number(e.options.ctx),
            QString::number(e.options.gpuLayers),
            QString::number(e.durationSec),
            QString::number(e.avgTps, 'f', 2),
            QString::number(e.peakTps, 'f', 2),
            QString::number(e.tokens),
            e.status,
            e.command
        };
        QStringList escaped;
        escaped.reserve(fields.size());
        for (const QString &f : fields)
            escaped << escapeCsv(f);
        stream << escaped.join(QLatin1Char(',')) << QLatin1Char('\n');
    }

    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

void HistoryStore::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonArray array = doc.object().value(QStringLiteral("entries")).toArray();
    m_entries.clear();
    for (const QJsonValue &value : array)
        m_entries.append(entryFromJson(value.toObject()));

    bool repaired = false;
    for (HistoryEntry &entry : m_entries) {
        if (!entry.endedAt.isValid()
            && (entry.status == QStringLiteral("运行中") || entry.status == QStringLiteral("启动中"))) {
            entry.endedAt = QDateTime::currentDateTime();
            entry.durationSec = entry.startedAt.secsTo(entry.endedAt);
            entry.status = QStringLiteral("异常结束");
            repaired = true;
        }
    }
    if (repaired)
        save();
}

void HistoryStore::save()
{
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonArray array;
    for (const HistoryEntry &e : m_entries)
        array.append(entryToJson(e));

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("entries")] = array;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}
