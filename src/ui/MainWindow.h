#pragma once

#include "Types.h"

#include <QMainWindow>
#include <QString>
#include <QVector>

class AppSettings;
class ServerController;
class HistoryStore;
class KnowledgeStore;
class ControlApiServer;
class ModelPage;
class MonitorPage;
class HistoryPage;
class KnowledgePage;
class ApiPage;
class ApiDocsPage;
class SettingsPage;
class QButtonGroup;
class QGraphicsOpacityEffect;
class QLabel;
class QPropertyAnimation;
class QPushButton;
class QStackedWidget;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildHeader();
    void connectSignals();
    void navigate(int index);
    void startSelectedModel(bool quiet = false);
    void replayEntry(const HistoryEntry &entry);
    void updateGlobalState();
    void updateFooter();
    void applyControlApi();
    void showToast(const QString &message, bool error = false);
    void positionToast();
    void finalizeHistory(const QString &status);
    void restoreWindowState();
    void saveWindowState();

    AppSettings *m_settings = nullptr;
    ServerController *m_controller = nullptr;
    HistoryStore *m_history = nullptr;
    KnowledgeStore *m_knowledge = nullptr;
    ControlApiServer *m_api = nullptr;

    QStackedWidget *m_pages = nullptr;
    QVector<QPushButton *> m_navButtons;
    QButtonGroup *m_navGroup = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSub = nullptr;
    QLabel *m_statusPill = nullptr;
    QPushButton *m_quickButton = nullptr;
    QPushButton *m_webButton = nullptr;
    QLabel *m_sidebarDot = nullptr;
    QLabel *m_sidebarState = nullptr;
    QLabel *m_sidebarHint = nullptr;
    QLabel *m_footer = nullptr;

    ModelPage *m_modelsPage = nullptr;
    MonitorPage *m_monitorPage = nullptr;
    HistoryPage *m_historyPage = nullptr;
    KnowledgePage *m_knowledgePage = nullptr;
    ApiPage *m_apiPage = nullptr;
    ApiDocsPage *m_apiDocsPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;

    QLabel *m_toast = nullptr;
    QGraphicsOpacityEffect *m_toastEffect = nullptr;
    QPropertyAnimation *m_toastFade = nullptr;
    QTimer *m_toastTimer = nullptr;

    QString m_activeHistoryId;
    QString m_initialModelsDir;
    bool m_darkTitleApplied = false;
};
