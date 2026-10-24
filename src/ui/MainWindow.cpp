#include "MainWindow.h"

#include "ApiDocsPage.h"
#include "ApiPage.h"
#include "AppSettings.h"
#include "ControlApiServer.h"
#include "HistoryPage.h"
#include "HistoryStore.h"
#include "Icons.h"
#include "ModelPage.h"
#include "MonitorPage.h"
#include "ServerController.h"
#include "SettingsPage.h"
#include "Theme.h"
#include "Widgets.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPair>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
const char *const kPageTitles[] = {
    "模型管理",
    "运行监控",
    "历史记录",
    "API 接口",
    "API 文档",
    "设置"
};

const char *const kPageSubtitles[] = {
    "选择 GGUF 模型，一键启动本地推理服务",
    "实时速度、吞吐曲线与服务日志",
    "每次启动的命令、时长与性能统计",
    "兼容 OpenAI 的推理接口调试与实时速度",
    "推理服务与控制 API 的端点说明",
    "路径、默认参数与启动行为"
};

QString stateSlug(ServerState state)
{
    switch (state) {
    case ServerState::Stopped:  return QStringLiteral("stopped");
    case ServerState::Starting: return QStringLiteral("starting");
    case ServerState::Running:  return QStringLiteral("running");
    case ServerState::Stopping: return QStringLiteral("stopping");
    case ServerState::Failed:   return QStringLiteral("failed");
    }
    return QStringLiteral("stopped");
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_settings = new AppSettings(this);
    m_controller = new ServerController(m_settings, this);
    m_history = new HistoryStore(this);
    m_api = new ControlApiServer(m_controller, m_settings, m_history, this);
    m_initialModelsDir = m_settings->modelsDir;

    buildUi();
    connectSignals();
    restoreWindowState();

    applyControlApi();
    updateGlobalState();
    updateFooter();
    navigate(0);

    QTimer::singleShot(700, this, [this]() {
        if (!m_settings->autoStartLast)
            return;
        if (m_settings->lastModelPath.isEmpty()
            || !QFileInfo::exists(m_settings->lastModelPath))
            return;
        if (!m_modelsPage->selectModelByPath(m_settings->lastModelPath))
            m_modelsPage->refreshModels();
        startSelectedModel(true);
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("LlamaDeck · llama.cpp 控制台"));
    setWindowIcon(Icons::appIcon());

    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    setMinimumSize(qMin(1020, available.width()), qMin(640, available.height()));
    resize(qMin(1400, available.width() - 40), qMin(880, available.height() - 40));

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("Root"));
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(buildSidebar());

    auto *body = new QWidget(root);
    body->setObjectName(QStringLiteral("PageBody"));
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(buildHeader());

    m_pages = new QStackedWidget(body);
    m_modelsPage = new ModelPage(m_settings, m_controller, m_pages);
    m_monitorPage = new MonitorPage(m_controller, m_pages);
    m_historyPage = new HistoryPage(m_history, m_pages);
    m_apiPage = new ApiPage(m_settings, m_controller, m_pages);
    m_apiDocsPage = new ApiDocsPage(m_settings, m_controller, m_pages);
    m_settingsPage = new SettingsPage(m_settings, m_pages);
    m_pages->addWidget(m_modelsPage);
    m_pages->addWidget(m_monitorPage);
    m_pages->addWidget(m_historyPage);
    m_pages->addWidget(m_apiPage);
    m_pages->addWidget(m_apiDocsPage);
    m_pages->addWidget(m_settingsPage);
    bodyLayout->addWidget(m_pages, 1);

    rootLayout->addWidget(body, 1);
    setCentralWidget(root);

    m_toast = new QLabel(root);
    m_toast->setObjectName(QStringLiteral("Toast"));
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toast->hide();

    m_toastEffect = new QGraphicsOpacityEffect(m_toast);
    m_toastEffect->setOpacity(1.0);
    m_toast->setGraphicsEffect(m_toastEffect);

    m_toastFade = new QPropertyAnimation(m_toastEffect, "opacity", this);
    m_toastFade->setDuration(260);
    connect(m_toastFade, &QPropertyAnimation::finished, this, [this]() {
        m_toast->hide();
        m_toastEffect->setOpacity(1.0);
    });

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(2600);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastFade->stop();
        m_toastFade->setStartValue(m_toastEffect->opacity());
        m_toastFade->setEndValue(0.0);
        m_toastFade->start();
    });
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(236);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 22, 16, 18);
    layout->setSpacing(6);

    auto *brandRow = new QHBoxLayout();
    brandRow->setSpacing(10);
    auto *logo = new QLabel(sidebar);
    logo->setPixmap(Icons::logo(36));
    logo->setFixedSize(36, 36);
    auto *brandText = new QVBoxLayout();
    brandText->setSpacing(0);
    brandText->addWidget(makeLabel(QStringLiteral("LlamaDeck"), QStringLiteral("BrandTitle"),
                                   sidebar));
    brandText->addWidget(makeLabel(QStringLiteral("LLAMA.CPP STUDIO"), QStringLiteral("BrandSub"),
                                   sidebar));
    brandRow->addWidget(logo);
    brandRow->addLayout(brandText);
    brandRow->addStretch();
    layout->addLayout(brandRow);
    layout->addSpacing(20);

    layout->addWidget(makeLabel(QStringLiteral("导航"), QStringLiteral("NavSection"), sidebar));
    layout->addSpacing(2);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    const QVector<QPair<Icons::Glyph, QString>> navItems{
        {Icons::Glyph::Models, QStringLiteral("模型管理")},
        {Icons::Glyph::Monitor, QStringLiteral("运行监控")},
        {Icons::Glyph::History, QStringLiteral("历史记录")},
        {Icons::Glyph::Api, QStringLiteral("API 接口")},
        {Icons::Glyph::Globe, QStringLiteral("API 文档")},
        {Icons::Glyph::Settings, QStringLiteral("设置")}
    };

    for (int i = 0; i < navItems.size(); ++i) {
        auto *button = new QPushButton(sidebar);
        button->setObjectName(QStringLiteral("Nav"));
        button->setText(navItems.at(i).second);
        button->setIcon(Icons::navIcon(navItems.at(i).first, Theme::Muted,
                                       QColor(160, 181, 255), 18));
        button->setIconSize(QSize(18, 18));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(42);
        m_navGroup->addButton(button, i);
        m_navButtons.append(button);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i]() { navigate(i); });
    }

    layout->addStretch();

    auto *statusCard = new QFrame(sidebar);
    statusCard->setObjectName(QStringLiteral("SidebarStatus"));
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(14, 12, 14, 12);
    statusLayout->setSpacing(5);

    auto *statusHeader = new QHBoxLayout();
    statusHeader->setSpacing(7);
    m_sidebarDot = new QLabel(statusCard);
    m_sidebarDot->setFixedSize(7, 7);
    m_sidebarDot->setStyleSheet(QStringLiteral("background-color:#6B778C;border-radius:3px;"));
    statusHeader->addWidget(m_sidebarDot);
    statusHeader->addWidget(makeLabel(QStringLiteral("推理服务"), QStringLiteral("SidebarStatusTitle"),
                                      statusCard));
    statusHeader->addStretch();
    statusLayout->addLayout(statusHeader);

    m_sidebarState = makeLabel(QStringLiteral("未运行"), QStringLiteral("SidebarStatusValue"),
                               statusCard);
    m_sidebarHint = makeLabel(QStringLiteral("127.0.0.1:8080"), QStringLiteral("SidebarStatusHint"),
                              statusCard);
    m_sidebarHint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addWidget(m_sidebarState);
    statusLayout->addWidget(m_sidebarHint);
    layout->addWidget(statusCard);

    layout->addSpacing(8);
    m_footer = makeLabel(QStringLiteral("LlamaDeck v1.0"), QStringLiteral("SidebarFooter"), sidebar);
    m_footer->setWordWrap(true);
    layout->addWidget(m_footer);

    return sidebar;
}

QWidget *MainWindow::buildHeader()
{
    auto *header = new QWidget(this);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(28, 20, 28, 10);
    layout->setSpacing(10);

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    m_pageTitle = makeLabel(QStringLiteral("模型管理"), QStringLiteral("PageTitle"), header);
    m_pageSub = makeLabel(QStringLiteral("选择 GGUF 模型，一键启动本地推理服务"),
                           QStringLiteral("PageSub"), header);
    titleBox->addWidget(m_pageTitle);
    titleBox->addWidget(m_pageSub);
    layout->addLayout(titleBox);
    layout->addStretch();

    m_statusPill = makeLabel(QStringLiteral("未运行"), QStringLiteral("ServerPill"), header);
    m_statusPill->setProperty("state", QStringLiteral("stopped"));
    m_statusPill->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusPill);

    m_webButton = new QPushButton(Icons::icon(Icons::Glyph::Globe, Theme::Muted, 15),
                                   QStringLiteral("Web UI"), header);
    m_webButton->setObjectName(QStringLiteral("Ghost"));
    m_webButton->setCursor(Qt::PointingHandCursor);
    m_webButton->setMinimumHeight(36);
    m_webButton->setToolTip(QStringLiteral("在浏览器中打开 llama-server Web UI"));
    m_webButton->setEnabled(false);
    layout->addWidget(m_webButton);

    m_quickButton = new QPushButton(Icons::icon(Icons::Glyph::Play, Qt::white, 15),
                                     QStringLiteral("启动服务"), header);
    m_quickButton->setObjectName(QStringLiteral("Primary"));
    m_quickButton->setCursor(Qt::PointingHandCursor);
    m_quickButton->setMinimumHeight(36);
    m_quickButton->setMinimumWidth(128);
    layout->addWidget(m_quickButton);

    return header;
}

void MainWindow::connectSignals()
{
    connect(m_modelsPage, &ModelPage::startRequested, this, [this]() { startSelectedModel(); });
    connect(m_modelsPage, &ModelPage::stopRequested, m_controller, &ServerController::stop);
    connect(m_modelsPage, &ModelPage::openSettingsRequested, this, [this]() { navigate(5); });
    connect(m_modelsPage, &ModelPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });

    connect(m_monitorPage, &MonitorPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });

    connect(m_historyPage, &HistoryPage::replayRequested, this, &MainWindow::replayEntry);
    connect(m_historyPage, &HistoryPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });

    connect(m_apiPage, &ApiPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });

    connect(m_apiDocsPage, &ApiDocsPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });

    connect(m_settingsPage, &SettingsPage::toastRequested, this, [this](const QString &message) {
        showToast(message);
    });
    connect(m_settingsPage, &SettingsPage::settingsSaved, this, [this]() {
        if (QDir::cleanPath(m_settings->modelsDir) != QDir::cleanPath(m_initialModelsDir)) {
            m_initialModelsDir = m_settings->modelsDir;
            m_modelsPage->refreshModels();
        }
        m_settingsPage->refresh();
    });

    connect(m_settings, &AppSettings::changed, this, [this]() {
        applyControlApi();
        updateGlobalState();
    });

    connect(m_controller, &ServerController::stateChanged, this, [this](ServerState) {
        updateGlobalState();
    });

    connect(m_controller, &ServerController::launchStarted, this,
            [this](const LaunchOptions &options, const QString &command) {
                QString modelName;
                if (m_modelsPage->currentOptions().modelPath.compare(
                        options.modelPath, Qt::CaseInsensitive) == 0)
                    modelName = m_modelsPage->selectedModelName();
                if (modelName.isEmpty())
                    modelName = QFileInfo(options.modelPath).completeBaseName();
                m_activeHistoryId = m_history->begin(options, modelName, command);
                updateGlobalState();
            });

    connect(m_controller, &ServerController::running, this, [this]() {
        if (!m_activeHistoryId.isEmpty())
            m_history->markRunning(m_activeHistoryId);
        updateGlobalState();

        const int port = m_controller->options().port;
        if (m_settings->autoOpenApiTab)
            navigate(3);
        if (m_settings->autoOpenBrowser)
            QDesktopServices::openUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port)));
    });

    connect(m_controller, &ServerController::stopped, this,
            [this](int, const QString &result) {
                finalizeHistory(result);
                updateGlobalState();
                if (result == QStringLiteral("已停止"))
                    showToast(QStringLiteral("推理服务已停止。"));
            });

    connect(m_controller, &ServerController::errorOccurred, this, [this](const QString &message) {
        showToast(message, true);
    });

    connect(m_quickButton, &QPushButton::clicked, this, [this]() {
        const ServerState state = m_controller->state();
        if (state == ServerState::Running || state == ServerState::Starting)
            m_controller->stop();
        else if (state != ServerState::Stopping)
            startSelectedModel();
    });

    connect(m_webButton, &QPushButton::clicked, this, [this]() {
        if (m_controller->state() != ServerState::Running)
            return;
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(m_controller->options().port)));
    });

    connect(m_api, &ControlApiServer::listeningChanged, this, [this](bool) { updateFooter(); });
    connect(m_api, &ControlApiServer::errorOccurred, this, [this](const QString &message) {
        showToast(message, true);
    });
}

void MainWindow::navigate(int index)
{
    if (index < 0 || index >= m_pages->count())
        return;
    m_pages->setCurrentIndex(index);
    if (index < m_navButtons.size())
        m_navButtons.at(index)->setChecked(true);
    m_pageTitle->setText(QString::fromUtf8(kPageTitles[index]));
    m_pageSub->setText(QString::fromUtf8(kPageSubtitles[index]));
    if (index == 2)
        m_historyPage->reload();
}

void MainWindow::startSelectedModel(bool quiet)
{
    if (m_controller->isRunning()) {
        showToast(QStringLiteral("推理服务正在运行，请先停止当前实例。"));
        return;
    }

    const LaunchOptions options = m_modelsPage->currentOptions();
    if (options.modelPath.isEmpty()) {
        if (!quiet)
            showToast(QStringLiteral("请先选择一个模型。"));
        navigate(0);
        return;
    }

    if (m_settings->llamaServerPath.isEmpty()
        || !QFileInfo::exists(m_settings->llamaServerPath)) {
        showToast(QStringLiteral("未找到 llama-server，请先在设置中指定路径。"), true);
        if (!quiet)
            navigate(5);
        return;
    }

    m_settings->port = options.port;
    m_settings->ctx = options.ctx;
    m_settings->gpuLayers = options.gpuLayers;
    m_settings->threads = options.threads;
    m_settings->flashAttn = options.flashAttn;
    m_settings->jinja = options.jinja;
    m_settings->apiKey = options.apiKey;
    m_settings->extraArgs = options.extraArgs;
    m_settings->lastModelPath = options.modelPath;
    m_settings->save();

    if (!m_controller->start(options)) {
        showToast(m_controller->lastError(), true);
        return;
    }
    navigate(1);
}

void MainWindow::replayEntry(const HistoryEntry &entry)
{
    if (m_controller->isRunning()) {
        showToast(QStringLiteral("推理服务正在运行，请先停止当前实例。"));
        return;
    }

    navigate(0);
    bool selected = m_modelsPage->selectModelByPath(entry.options.modelPath);
    if (!selected) {
        m_modelsPage->refreshModels();
        selected = m_modelsPage->selectModelByPath(entry.options.modelPath);
    }
    if (!selected) {
        showToast(QStringLiteral("历史记录中的模型文件已不存在。"), true);
        return;
    }
    m_modelsPage->applyOptions(entry.options);
    startSelectedModel();
}

void MainWindow::updateGlobalState()
{
    const ServerState state = m_controller->state();
    const int port = (state == ServerState::Running || state == ServerState::Starting
                      || state == ServerState::Stopping)
        ? m_controller->options().port
        : m_settings->port;

    m_statusPill->setText(serverStateText(state));
    m_statusPill->setProperty("state", stateSlug(state));
    repolish(m_statusPill);

    if (state == ServerState::Running || state == ServerState::Starting) {
        m_quickButton->setObjectName(QStringLiteral("Danger"));
        m_quickButton->setText(QStringLiteral("停止服务"));
        m_quickButton->setIcon(Icons::icon(Icons::Glyph::Stop, Theme::Danger, 15));
        m_quickButton->setEnabled(true);
    } else if (state == ServerState::Stopping) {
        m_quickButton->setObjectName(QStringLiteral("Danger"));
        m_quickButton->setText(QStringLiteral("正在停止…"));
        m_quickButton->setIcon(Icons::icon(Icons::Glyph::Stop, Theme::Danger, 15));
        m_quickButton->setEnabled(false);
    } else {
        m_quickButton->setObjectName(QStringLiteral("Primary"));
        m_quickButton->setText(QStringLiteral("启动服务"));
        m_quickButton->setIcon(Icons::icon(Icons::Glyph::Play, Qt::white, 15));
        m_quickButton->setEnabled(true);
    }
    repolish(m_quickButton);

    m_webButton->setEnabled(state == ServerState::Running);

    QString color = QStringLiteral("#6B778C");
    switch (state) {
    case ServerState::Running:
        color = QStringLiteral("#3DDC97");
        break;
    case ServerState::Starting:
    case ServerState::Stopping:
        color = QStringLiteral("#F5B84A");
        break;
    case ServerState::Failed:
        color = QStringLiteral("#FF6B85");
        break;
    case ServerState::Stopped:
        color = QStringLiteral("#5A657A");
        break;
    }
    m_sidebarDot->setStyleSheet(QStringLiteral("background-color:%1;border-radius:3px;").arg(color));
    m_sidebarState->setText(serverStateText(state));
    m_sidebarHint->setText(QStringLiteral("127.0.0.1:%1").arg(port));
}

void MainWindow::updateFooter()
{
    const QString apiText = m_api->isListening()
        ? QStringLiteral("控制 API :%1").arg(m_api->port())
        : QStringLiteral("控制 API 已关闭");
    m_footer->setText(QStringLiteral("LlamaDeck v1.0  ·  %1").arg(apiText));
}

void MainWindow::applyControlApi()
{
    m_api->applyConfig(m_settings->controlApiEnabled,
                       quint16(qBound(1024, m_settings->controlApiPort, 65535)));
}

void MainWindow::showToast(const QString &message, bool error)
{
    if (!m_toast || message.isEmpty())
        return;

    QString text = message;
    if (text.size() > 120)
        text = text.left(118) + QStringLiteral("…");
    m_toast->setToolTip(message);

    m_toastTimer->stop();
    m_toastFade->stop();
    m_toastEffect->setOpacity(1.0);
    m_toast->setProperty("variant", error ? QStringLiteral("error") : QStringLiteral("default"));
    m_toast->setText(text);
    repolish(m_toast);
    m_toast->adjustSize();
    positionToast();
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start();
}

void MainWindow::positionToast()
{
    if (!m_toast || !centralWidget() || !m_toast->isVisible())
        return;
    const int x = (centralWidget()->width() - m_toast->width()) / 2;
    const int y = centralWidget()->height() - m_toast->height() - 30;
    m_toast->move(qMax(12, x), qMax(12, y));
}

void MainWindow::finalizeHistory(const QString &status)
{
    if (m_activeHistoryId.isEmpty())
        return;

    const ServerStats stats = m_controller->stats();
    const double average = m_controller->averageGenTps();
    const double peak = qMax(stats.peakGenTps, average);
    m_history->finish(m_activeHistoryId, status, average, peak, stats.genTokens);
    m_activeHistoryId.clear();
}

void MainWindow::restoreWindowState()
{
    QSettings s(m_settings->configFilePath(), QSettings::IniFormat);
    const QByteArray geometry = s.value(QStringLiteral("ui/geometry")).toByteArray();
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();

    bool restored = false;
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
        const QRect margin = available.adjusted(8, 8, -8, -8);
        restored = margin.contains(frameGeometry());
    }

    if (!restored) {
        resize(qMin(1400, available.width() - 80), qMin(880, available.height() - 80));
        const QRect frame = frameGeometry();
        move(available.center().x() - frame.width() / 2,
             available.center().y() - frame.height() / 2);
    }
}

void MainWindow::saveWindowState()
{
    QSettings s(m_settings->configFilePath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("ui/geometry"), saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_controller->isRunning()) {
        if (m_settings->stopOnExit) {
            finalizeHistory(QStringLiteral("已停止"));
            m_controller->stop();
        } else {
            finalizeHistory(QStringLiteral("已分离"));
            m_controller->detachRunningProcess();
        }
    } else {
        finalizeHistory(QStringLiteral("已停止"));
    }

    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    positionToast();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
    if (!m_darkTitleApplied) {
        const HWND handle = reinterpret_cast<HWND>(winId());
        BOOL dark = TRUE;
        DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(handle, 19, &dark, sizeof(dark));
        m_darkTitleApplied = true;
    }
#endif
}
