#pragma once

#include "Types.h"

#include <QWidget>

class HistoryStore;
class QLabel;
class QStackedWidget;
class QTableWidget;

class HistoryPage : public QWidget {
    Q_OBJECT

public:
    explicit HistoryPage(HistoryStore *store, QWidget *parent = nullptr);

    void reload();

signals:
    void replayRequested(const HistoryEntry &entry);
    void toastRequested(const QString &message);

private:
    void buildUi();
    QWidget *buildEmptyState();
    void exportCsv();
    void clearHistory();
    QString statusVariant(const QString &status) const;

    HistoryStore *m_store = nullptr;
    QTableWidget *m_table = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_countLabel = nullptr;
};
