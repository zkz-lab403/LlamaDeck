#include "MonitorPage.h"

#include "Icons.h"
#include "ServerController.h"
#include "SpeedChart.h"
#include "Theme.h"
#include "Types.h"
#include "Widgets.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

MonitorPage::MonitorPage(ServerController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
{
    buildUi();

    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setInterval(2000);
    connect(m_sampleTimer, &QTimer::timeout, this, [this]() {
        if (m_controller->state() == ServerState::Running)
            m_chart->addSample(m_controller->stats().genTps);
    });

    m_uptimeTimer = new QTimer(this);
    m_uptimeTimer->setInterval(1000);
    connect(m_uptimeTimer, &QTimer::timeout, this, &MonitorPage::updateUptime);
    m_uptimeTimer->start();

    connect(m_controller, &ServerController::statsChanged, this, [this]() {
        updateStats();
    });
    connect(m_controller, &ServerController::stateChanged, this, [this](ServerState) {
        updateState();
        updateStats();
        updateUptime();
    });
    connect(m_controller, &ServerController::logLine, this, &MonitorPage::appendLog);

    for (const QString &line : m_controller->logs())
        appendLog(line);

    updateState();
    updateStats();
    updateUptime();
}

void MonitorPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 8, 28, 22);
    layout->setSpacing(14);

    auto *statsRow = new QHBoxLayout();
    statsRow->setSpacing(12);

    m_stateCard = new StatCard(QStringLiteral("运行状态"), this);
    m_stateCard->setMinimumHeight(112);
    m_stateCard->setAccent(Theme::Muted);
    m_stateCard->setValue(QStringLiteral("未运行"));
    m_stateCard->setCaption(QStringLiteral("启动服务后显示实时指标"));

    m_genCard = new StatCard(QStringLiteral("生成速度"), this);
    m_genCard->setMinimumHeight(112);
    m_genCard->setAccent(Theme::Accent);
    m_genCard->setValue(QStringLiteral("0.0"), QStringLiteral("tokens/s"));
    m_genCard->setCaption(QStringLiteral("最近一次生成解码速度"));

    m_promptCard = new StatCard(QStringLiteral("提示处理"), this);
    m_promptCard->setMinimumHeight(112);
    m_promptCard->setAccent(Theme::Info);
    m_promptCard->setValue(QStringLiteral("0.0"), QStringLiteral("tokens/s"));
    m_promptCard->setCaption(QStringLiteral("提示词预填充速度"));

    m_uptimeCard = new StatCard(QStringLiteral("运行时长"), this);
    m_uptimeCard->setMinimumHeight(112);
    m_uptimeCard->setAccent(Theme::Success);
    m_uptimeCard->setValue(QStringLiteral("00:00"));
    m_uptimeCard->setCaption(QStringLiteral("累计运行时长"));

    statsRow->addWidget(m_stateCard, 1);
    statsRow->addWidget(m_genCard, 1);
    statsRow->addWidget(m_promptCard, 1);
    statsRow->addWidget(m_uptimeCard, 1);
    layout->addLayout(statsRow);

    auto *chartCard = makeCard(this);
    chartCard->setMinimumHeight(286);
    auto *chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(18, 16, 18, 14);
    chartLayout->setSpacing(6);

    auto *chartHeader = new QHBoxLayout();
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    titleBox->addWidget(makeLabel(QStringLiteral("实时吞吐"), QStringLiteral("SectionTitle"),
                                  chartCard));
    titleBox->addWidget(makeLabel(QStringLiteral("tokens/s · 每 2 秒采样 · 最近 4 分钟"),
                                  QStringLiteral("Hint"), chartCard));
    chartHeader->addLayout(titleBox);
    chartHeader->addStretch();
    m_chartBadge = makeLabel(QStringLiteral("等待数据"), QStringLiteral("Badge"), chartCard);
    m_chartBadge->setProperty("variant", QStringLiteral("muted"));
    m_chartBadge->setAlignment(Qt::AlignCenter);
    chartHeader->addWidget(m_chartBadge);
    chartLayout->addLayout(chartHeader);

    m_chart = new SpeedChart(chartCard);
    chartLayout->addWidget(m_chart, 1);

    auto *chartFooter = new QHBoxLayout();
    m_peakLabel = makeLabel(QStringLiteral("峰值 0.0 t/s"), QStringLiteral("Hint"), chartCard);
    m_tokensLabel = makeLabel(QStringLiteral("生成 0 tokens · 提示 0 tokens"),
                               QStringLiteral("Hint"), chartCard);
    chartFooter->addWidget(m_peakLabel);
    chartFooter->addStretch();
    chartFooter->addWidget(m_tokensLabel);
    chartLayout->addLayout(chartFooter);

    layout->addWidget(chartCard);

    auto *logCard = makeCard(this);
    auto *logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(18, 14, 18, 14);
    logLayout->setSpacing(10);

    auto *logHeader = new QHBoxLayout();
    logHeader->setSpacing(10);
    logHeader->addWidget(makeLabel(QStringLiteral("服务日志"), QStringLiteral("SectionTitle"),
                                   logCard));
    m_logMeta = makeLabel(QStringLiteral("实时输出"), QStringLiteral("Hint"), logCard);
    logHeader->addWidget(m_logMeta);
    logHeader->addStretch();

    auto *autoScrollLabel = makeLabel(QStringLiteral("自动滚动"), QStringLiteral("Hint"), logCard);
    m_autoScroll = new ToggleSwitch(logCard);
    m_autoScroll->setChecked(true);
    logHeader->addWidget(autoScrollLabel);
    logHeader->addWidget(m_autoScroll);

    auto *copyButton = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                        QString(), logCard);
    copyButton->setObjectName(QStringLiteral("IconButton"));
    copyButton->setToolTip(QStringLiteral("复制全部日志"));
    copyButton->setCursor(Qt::PointingHandCursor);
    copyButton->setFixedSize(30, 26);
    connect(copyButton, &QPushButton::clicked, this, &MonitorPage::copyLog);

    auto *clearButton = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Muted, 14),
                                         QString(), logCard);
    clearButton->setObjectName(QStringLiteral("IconButton"));
    clearButton->setToolTip(QStringLiteral("清空日志"));
    clearButton->setCursor(Qt::PointingHandCursor);
    clearButton->setFixedSize(30, 26);
    connect(clearButton, &QPushButton::clicked, this, &MonitorPage::clearLog);

    logHeader->addWidget(copyButton);
    logHeader->addWidget(clearButton);
    logLayout->addLayout(logHeader);

    m_log = new QPlainTextEdit(logCard);
    m_log->setObjectName(QStringLiteral("Log"));
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(QStringLiteral("服务输出会显示在这里…"));
    m_log->setMaximumBlockCount(5000);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    logLayout->addWidget(m_log, 1);

    layout->addWidget(logCard, 1);
}

void MonitorPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateState();
    updateStats();
    updateUptime();
}

void MonitorPage::updateState()
{
    const ServerState state = m_controller->state();
    QColor accent = Theme::Muted;
    switch (state) {
    case ServerState::Running:
        accent = Theme::Success;
        break;
    case ServerState::Starting:
    case ServerState::Stopping:
        accent = Theme::Warning;
        break;
    case ServerState::Failed:
        accent = Theme::Danger;
        break;
    case ServerState::Stopped:
        accent = Theme::Muted;
        break;
    }

    m_stateCard->setValue(serverStateText(state));
    m_stateCard->setAccent(accent);

    if (state == ServerState::Running) {
        m_chartBadge->setText(QStringLiteral("实时"));
        m_chartBadge->setProperty("variant", QStringLiteral("success"));
        if (!m_sampleTimer->isActive())
            m_sampleTimer->start();
    } else if (state == ServerState::Starting) {
        m_chart->clear();
        m_chartBadge->setText(QStringLiteral("启动中"));
        m_chartBadge->setProperty("variant", QStringLiteral("warning"));
        m_sampleTimer->stop();
    } else if (state == ServerState::Stopping) {
        m_chartBadge->setText(QStringLiteral("停止中"));
        m_chartBadge->setProperty("variant", QStringLiteral("warning"));
        m_sampleTimer->stop();
    } else if (state == ServerState::Failed) {
        m_chartBadge->setText(QStringLiteral("启动失败"));
        m_chartBadge->setProperty("variant", QStringLiteral("danger"));
        m_sampleTimer->stop();
    } else {
        m_chartBadge->setText(QStringLiteral("未运行"));
        m_chartBadge->setProperty("variant", QStringLiteral("muted"));
        m_sampleTimer->stop();
    }
    repolish(m_chartBadge);

    if (state == ServerState::Running)
        m_logMeta->setText(QStringLiteral("实时输出"));
    else if (state == ServerState::Starting)
        m_logMeta->setText(QStringLiteral("正在加载模型…"));
    else
        m_logMeta->setText(QStringLiteral("%1 行")
                               .arg(m_log->toPlainText().isEmpty()
                                        ? 0
                                        : m_log->document()->blockCount()));
}

void MonitorPage::updateStats()
{
    const ServerStats &stats = m_controller->stats();
    m_genCard->setValue(QString::number(stats.genTps, 'f', 1), QStringLiteral("tokens/s"));
    m_promptCard->setValue(QString::number(stats.promptTps, 'f', 1), QStringLiteral("tokens/s"));
    m_peakLabel->setText(QStringLiteral("峰值 %1 t/s").arg(stats.peakGenTps, 0, 'f', 1));
    m_tokensLabel->setText(QStringLiteral("生成 %1 tokens · 提示 %2 tokens")
                               .arg(stats.genTokens)
                               .arg(stats.promptTokens));

    const ServerState state = m_controller->state();
    if (state == ServerState::Running) {
        m_stateCard->setCaption(QStringLiteral("PID %1 · 端口 %2%3")
                                    .arg(stats.pid)
                                    .arg(m_controller->options().port)
                                    .arg(stats.activeRequests > 0
                                             ? QStringLiteral(" · 活跃请求 %1").arg(stats.activeRequests)
                                             : QString()));
    } else if (state == ServerState::Starting) {
        m_stateCard->setCaption(QStringLiteral("正在等待健康检查通过…"));
    } else if (state == ServerState::Stopping) {
        m_stateCard->setCaption(QStringLiteral("正在关闭进程…"));
    } else if (state == ServerState::Failed) {
        m_stateCard->setCaption(QStringLiteral("启动失败，请查看日志"));
    } else {
        m_stateCard->setCaption(QStringLiteral("启动服务后显示实时指标"));
    }

    if (state == ServerState::Running)
        m_genCard->setCaption(QStringLiteral("最近一次生成解码速度"));
    else if (state == ServerState::Stopped)
        m_genCard->setCaption(QStringLiteral("服务未运行"));

    m_peakLabel->setText(QStringLiteral("峰值 %1 t/s").arg(stats.peakGenTps, 0, 'f', 1));
}

void MonitorPage::updateUptime()
{
    const ServerStats &stats = m_controller->stats();
    if (m_controller->state() == ServerState::Running && stats.startedAt.isValid()) {
        const qint64 seconds = stats.startedAt.secsTo(QDateTime::currentDateTime());
        m_uptimeCard->setValue(formatDuration(seconds));
        m_uptimeCard->setCaption(QStringLiteral("开始于 %1")
                                     .arg(stats.startedAt.toString(QStringLiteral("HH:mm:ss"))));
    } else {
        m_uptimeCard->setValue(QStringLiteral("00:00"));
        m_uptimeCard->setCaption(QStringLiteral("累计运行时长"));
    }
    m_uptimeCard->setAccent(m_controller->state() == ServerState::Running ? Theme::Success
                                                                         : Theme::Muted);
}

void MonitorPage::appendLog(const QString &line)
{
    if (line.isEmpty())
        return;
    const bool autoScroll = m_autoScroll->isChecked();
    QScrollBar *bar = m_log->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_log->appendPlainText(line);
    if (autoScroll && atBottom)
        bar->setValue(bar->maximum());
    if (m_controller->state() != ServerState::Running
        && m_controller->state() != ServerState::Starting)
        m_logMeta->setText(QStringLiteral("%1 行").arg(m_log->document()->blockCount()));
}

void MonitorPage::clearLog()
{
    m_log->clear();
    m_controller->clearLog();
    m_logMeta->setText(QStringLiteral("0 行"));
}

void MonitorPage::copyLog()
{
    QGuiApplication::clipboard()->setText(m_log->toPlainText());
    emit toastRequested(QStringLiteral("服务日志已复制到剪贴板。"));
}
