#include "KnowledgeStore.h"

#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr qint64 kMaxFileBytes = 1024 * 1024;
constexpr qint64 kMaxTotalCharacters = 4 * 1024 * 1024;
constexpr int kMaxDocuments = 200;

bool isCjk(QChar ch)
{
    return ch.unicode() >= 0x2E80;
}

QString expandedLongPath(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    wchar_t buffer[32768];
    const DWORD length = GetLongPathNameW(
        reinterpret_cast<LPCWSTR>(native.utf16()), buffer, DWORD(32768));
    if (length > 0 && length < 32768)
        return QString::fromWCharArray(buffer, int(length));
#endif
    return path;
}

QString normalizedPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QString resolved = canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    return QDir::cleanPath(expandedLongPath(resolved)).toLower();
}

} // namespace

KnowledgeStore::KnowledgeStore(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    load();
}

QVector<KnowledgeDocument> KnowledgeStore::documents() const
{
    QVector<KnowledgeDocument> result;
    result.reserve(m_documents.size());
    for (const StoredDocument &document : m_documents)
        result.append(document.info);
    return result;
}

int KnowledgeStore::documentCount() const
{
    return m_documents.size();
}

int KnowledgeStore::chunkCount() const
{
    int total = 0;
    for (const StoredDocument &document : m_documents)
        total += document.chunks.size();
    return total;
}

qint64 KnowledgeStore::totalCharacters() const
{
    qint64 total = 0;
    for (const StoredDocument &document : m_documents) {
        for (const Chunk &chunk : document.chunks)
            total += chunk.text.size();
    }
    return total;
}

bool KnowledgeStore::importFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        m_lastError = QStringLiteral("文件不存在：%1").arg(path);
        return false;
    }
    if (info.size() > kMaxFileBytes) {
        m_lastError = QStringLiteral("%1 超过 1 MB，请拆分后再导入。").arg(info.fileName());
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法读取 %1：%2").arg(info.fileName(), file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    if (data.contains('\0')) {
        m_lastError = QStringLiteral("%1 看起来是二进制文件，仅支持文本类文档。").arg(info.fileName());
        return false;
    }

    QString text = QString::fromUtf8(data);
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (text.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("%1 没有可用文本内容。").arg(info.fileName());
        return false;
    }

    QVector<Chunk> chunks = splitIntoChunks(text);
    if (chunks.isEmpty()) {
        m_lastError = QStringLiteral("%1 内容太短，未能切分。").arg(info.fileName());
        return false;
    }

    const QString canonicalPath = normalizedPath(path);
    const QString canonicalFile = info.canonicalFilePath();
    const QString displayPath = expandedLongPath(
        canonicalFile.isEmpty() ? info.absoluteFilePath() : canonicalFile);
    int replaceIndex = -1;
    for (int i = 0; i < m_documents.size(); ++i) {
        if (normalizedPath(m_documents.at(i).info.sourcePath) == canonicalPath) {
            replaceIndex = i;
            break;
        }
    }
    if (replaceIndex < 0 && m_documents.size() >= kMaxDocuments) {
        m_lastError = QStringLiteral("知识库最多保存 %1 个文档。").arg(kMaxDocuments);
        return false;
    }

    qint64 replacedCharacters = 0;
    QString replacedId;
    if (replaceIndex >= 0) {
        const StoredDocument &previous = m_documents.at(replaceIndex);
        replacedId = previous.info.id;
        for (const Chunk &chunk : previous.chunks)
            replacedCharacters += chunk.text.size();
    }

    qint64 newCharacters = 0;
    for (const Chunk &chunk : chunks)
        newCharacters += chunk.text.size();

    if (totalCharacters() - replacedCharacters + newCharacters > kMaxTotalCharacters) {
        m_lastError = QStringLiteral("知识库总内容超过 4 M 字符，请删除部分文档后再导入。");
        return false;
    }

    StoredDocument stored;
    stored.info.id = replacedId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces) : replacedId;
    stored.info.title = info.completeBaseName();
    stored.info.sourcePath = QDir::toNativeSeparators(displayPath);
    stored.info.sizeBytes = info.size();
    stored.info.chunkCount = chunks.size();
    stored.info.addedAt = QDateTime::currentDateTime();
    stored.chunks = std::move(chunks);

    if (replaceIndex >= 0)
        m_documents.removeAt(replaceIndex);
    m_documents.append(stored);

    m_lastError.clear();
    save();
    emit changed();
    return true;
}

int KnowledgeStore::importFiles(const QStringList &paths, int *skipped)
{
    int imported = 0;
    int failed = 0;
    for (const QString &path : paths) {
        if (importFile(path))
            ++imported;
        else
            ++failed;
    }
    if (skipped)
        *skipped = failed;
    return imported;
}

void KnowledgeStore::removeDocument(const QString &id)
{
    const int before = m_documents.size();
    m_documents.erase(std::remove_if(m_documents.begin(), m_documents.end(),
                                     [&id](const StoredDocument &document) {
                                         return document.info.id == id;
                                     }),
                      m_documents.end());
    if (m_documents.size() == before)
        return;
    save();
    emit changed();
}

void KnowledgeStore::clearAll()
{
    if (m_documents.isEmpty())
        return;
    m_documents.clear();
    save();
    emit changed();
}

QVector<KnowledgeHit> KnowledgeStore::search(const QString &query, int limit) const
{
    QVector<KnowledgeHit> hits;
    const QHash<QString, int> queryTokens = tokenize(query);
    if (queryTokens.isEmpty() || m_documents.isEmpty())
        return hits;

    const QString needle = query.trimmed();

    for (const StoredDocument &document : m_documents) {
        for (const Chunk &chunk : document.chunks) {
            double score = 0.0;
            for (auto it = queryTokens.constBegin(); it != queryTokens.constEnd(); ++it) {
                const int count = chunk.tokens.value(it.key());
                if (count > 0)
                    score += 1.0 + std::log(double(count));
            }
            if (score <= 0.0)
                continue;

            score /= std::sqrt(double(chunk.text.size()) / 140.0 + 1.0);
            if (!needle.isEmpty() && chunk.text.contains(needle, Qt::CaseInsensitive))
                score += 3.0;

            KnowledgeHit hit;
            hit.documentId = document.info.id;
            hit.title = document.info.title;
            hit.text = chunk.text;
            hit.score = score;
            hits.append(hit);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const KnowledgeHit &left, const KnowledgeHit &right) {
        return left.score > right.score;
    });

    if (limit > 0 && hits.size() > limit)
        hits.resize(limit);
    return hits;
}

QVector<KnowledgeStore::Chunk> KnowledgeStore::splitIntoChunks(const QString &text)
{
    QVector<Chunk> chunks;

    const int target = 420;
    const int overlap = 60;
    const int length = text.size();
    int position = 0;

    while (position < length) {
        int end = qMin(position + target, length);
        if (end < length) {
            for (int i = end; i > position + target / 2; --i) {
                const QChar ch = text.at(i - 1);
                if (ch == QLatin1Char('\n') || ch == QChar(0x3002) || ch == QChar(0xFF01)
                    || ch == QChar(0xFF1F) || ch == QChar(0xFF1B) || ch == QLatin1Char('.')
                    || ch == QLatin1Char(';') || ch == QLatin1Char('?') || ch == QLatin1Char('!')) {
                    end = i;
                    break;
                }
            }
        }

        const QString piece = text.mid(position, end - position).trimmed();
        if (piece.size() >= 12) {
            Chunk chunk;
            chunk.text = piece;
            chunk.tokens = tokenize(piece);
            chunks.append(chunk);
        }

        if (end >= length)
            break;
        position = qMax(position + 1, end - overlap);
    }

    return chunks;
}

QHash<QString, int> KnowledgeStore::tokenize(const QString &text)
{
    QHash<QString, int> tokens;

    const QString lower = text.toLower();
    QString latin;
    QString cjk;

    auto flushLatin = [&tokens, &latin]() {
        if (latin.size() >= 2)
            tokens[latin] += 1;
        latin.clear();
    };
    auto flushCjk = [&tokens, &cjk]() {
        if (cjk.isEmpty())
            return;
        if (cjk.size() == 1) {
            tokens[cjk] += 1;
        } else {
            for (int i = 0; i + 1 < cjk.size(); ++i)
                tokens[cjk.mid(i, 2)] += 1;
        }
        cjk.clear();
    };

    for (const QChar ch : lower) {
        if (isCjk(ch)) {
            flushLatin();
            cjk.append(ch);
        } else if (ch.isLetterOrNumber()) {
            flushCjk();
            latin.append(ch);
        } else {
            flushLatin();
            flushCjk();
        }
    }
    flushLatin();
    flushCjk();

    return tokens;
}

void KnowledgeStore::load()
{
    m_documents.clear();
    m_lastError.clear();

    QFile file(m_settings->dataDir() + QStringLiteral("/knowledge.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject())
        return;

    const QJsonArray items = document.object().value(QStringLiteral("documents")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        StoredDocument stored;
        stored.info.id = object.value(QStringLiteral("id")).toString();
        stored.info.title = object.value(QStringLiteral("title")).toString();
        stored.info.sourcePath = object.value(QStringLiteral("path")).toString();
        stored.info.sizeBytes = qint64(object.value(QStringLiteral("size")).toDouble());
        stored.info.addedAt = QDateTime::fromString(
            object.value(QStringLiteral("addedAt")).toString(), Qt::ISODateWithMs);

        const QJsonArray chunkArray = object.value(QStringLiteral("chunks")).toArray();
        for (const QJsonValue &chunkValue : chunkArray) {
            const QString piece = chunkValue.toString();
            if (piece.isEmpty())
                continue;
            Chunk chunk;
            chunk.text = piece;
            chunk.tokens = tokenize(piece);
            stored.chunks.append(chunk);
        }

        if (stored.info.id.isEmpty() || stored.chunks.isEmpty())
            continue;
        stored.info.chunkCount = stored.chunks.size();
        m_documents.append(stored);
    }
}

void KnowledgeStore::save()
{
    QJsonArray items;
    for (const StoredDocument &document : m_documents) {
        QJsonObject object;
        object[QStringLiteral("id")] = document.info.id;
        object[QStringLiteral("title")] = document.info.title;
        object[QStringLiteral("path")] = document.info.sourcePath;
        object[QStringLiteral("size")] = double(document.info.sizeBytes);
        object[QStringLiteral("addedAt")] = document.info.addedAt.toString(Qt::ISODateWithMs);

        QJsonArray chunks;
        for (const Chunk &chunk : document.chunks)
            chunks.append(chunk.text);
        object[QStringLiteral("chunks")] = chunks;

        items.append(object);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("documents")] = items;

    QSaveFile file(m_settings->dataDir() + QStringLiteral("/knowledge.json"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}
