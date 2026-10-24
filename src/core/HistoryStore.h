#pragma once

#include "Types.h"

#include <QObject>
#include <QVector>

class HistoryStore : public QObject {
    Q_OBJECT

public:
    explicit HistoryStore(QObject *parent = nullptr);

    QVector<HistoryEntry> entries() const { return m_entries; }

    QString begin(const LaunchOptions &options, const QString &modelName, const QString &command);
    void markRunning(const QString &id);
    void finish(const QString &id, const QString &status, double avgTps, double peakTps, quint64 tokens);
    void clear();
    bool exportCsv(const QString &filePath, QString *error = nullptr) const;

signals:
    void changed();

private:
    void load();
    void save();

    QString m_filePath;
    QVector<HistoryEntry> m_entries;
};
