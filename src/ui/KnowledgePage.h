#pragma once

#include <QWidget>

class KnowledgeStore;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

class KnowledgePage : public QWidget {
    Q_OBJECT

public:
    explicit KnowledgePage(KnowledgeStore *store, QWidget *parent = nullptr);

signals:
    void toastRequested(const QString &message);

private:
    void buildUi();
    QWidget *buildDocumentCard();
    QWidget *buildSearchCard();
    QWidget *buildEmptyState();
    void reload();
    void addFiles();
    void removeSelected();
    void clearAll();
    void runSearch();
    void copyResults();

    KnowledgeStore *m_store = nullptr;
    QTableWidget *m_table = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_chunkLabel = nullptr;
    QLabel *m_searchMeta = nullptr;
    QLineEdit *m_search = nullptr;
    QPlainTextEdit *m_results = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_clearButton = nullptr;
};
