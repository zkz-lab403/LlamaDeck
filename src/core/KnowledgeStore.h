#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

class AppSettings;

struct KnowledgeDocument {
    QString id;
    QString title;
    QString sourcePath;
    qint64 sizeBytes = 0;
    int chunkCount = 0;
    QDateTime addedAt;
};

struct KnowledgeHit {
    QString documentId;
    QString title;
    QString text;
    double score = 0.0;
};

class KnowledgeStore : public QObject {
    Q_OBJECT

public:
    explicit KnowledgeStore(AppSettings *settings, QObject *parent = nullptr);

    QVector<KnowledgeDocument> documents() const;
    int documentCount() const;
    int chunkCount() const;
    QString lastError() const { return m_lastError; }

    bool importFile(const QString &path);
    int importFiles(const QStringList &paths, int *skipped = nullptr);
    void removeDocument(const QString &id);
    void clearAll();
    QVector<KnowledgeHit> search(const QString &query, int limit = 4) const;

signals:
    void changed();

private:
    struct Chunk {
        QString text;
        QHash<QString, int> tokens;
    };

    struct StoredDocument {
        KnowledgeDocument info;
        QVector<Chunk> chunks;
    };

    void load();
    void save();
    static QVector<Chunk> splitIntoChunks(const QString &text);
    static QHash<QString, int> tokenize(const QString &text);
    qint64 totalCharacters() const;

    AppSettings *m_settings = nullptr;
    QVector<StoredDocument> m_documents;
    QString m_lastError;
};
