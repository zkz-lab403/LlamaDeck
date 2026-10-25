#include "ApiDocsPage.h"

#include "AppSettings.h"
#include "Icons.h"
#include "ServerController.h"
#include "Theme.h"
#include "Types.h"
#include "Widgets.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextOption>
#include <QVBoxLayout>

ApiDocsPage::ApiDocsPage(AppSettings *settings, ServerController *controller, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_controller(controller)
{
    buildUi();

    connect(m_controller, &ServerController::stateChanged, this, [this]() { updateEndpoints(); });
    connect(m_settings, &AppSettings::changed, this, &ApiDocsPage::updateEndpoints);

    updateEndpoints();
}

void ApiDocsPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 10, 32, 24);
    layout->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *body = new QWidget(scroll);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 4, 0);
    bodyLayout->setSpacing(14);

    auto *serverCard = makeCard(body);
    auto *serverLayout = new QVBoxLayout(serverCard);
    serverLayout->setContentsMargins(18, 16, 18, 16);
    serverLayout->setSpacing(4);

    auto *serverHeader = new QHBoxLayout();
    auto *serverTitleBox = new QVBoxLayout();
    serverTitleBox->setSpacing(1);
    serverTitleBox->addWidget(makeLabel(QStringLiteral("推理服务 API"),
                                        QStringLiteral("SectionTitle"), serverCard));
    serverTitleBox->addWidget(makeLabel(QStringLiteral("llama-server 原生接口 · OpenAI 兼容"),
                                        QStringLiteral("Hint"), serverCard));
    serverHeader->addLayout(serverTitleBox);
    serverHeader->addStretch();
    m_serverBaseLabel = makeLabel(QStringLiteral("http://127.0.0.1:8080"),
                                   QStringLiteral("Endpoint"), serverCard);
    serverHeader->addWidget(m_serverBaseLabel);
    serverLayout->addLayout(serverHeader);
    serverLayout->addSpacing(6);

    addEndpointRow(serverLayout, QStringLiteral("POST"), QStringLiteral("/v1/chat/completions"),
                   QStringLiteral("对话补全，支持 SSE 流式输出"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("POST"), QStringLiteral("/v1/completions"),
                   QStringLiteral("原始文本补全"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("POST"), QStringLiteral("/v1/embeddings"),
                   QStringLiteral("文本向量嵌入"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("GET"), QStringLiteral("/health"),
                   QStringLiteral("健康检查，加载完成时返回 ok"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("GET"), QStringLiteral("/props"),
                   QStringLiteral("当前模型与默认生成参数"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("GET"), QStringLiteral("/metrics"),
                   QStringLiteral("Prometheus 格式的吞吐指标"), false);
    serverLayout->addWidget(makeDivider(serverCard));
    addEndpointRow(serverLayout, QStringLiteral("GET"), QStringLiteral("/slots"),
                   QStringLiteral("槽位与当前请求状态"), false);

    serverLayout->addSpacing(12);
    auto *curlHeader = new QHBoxLayout();
    curlHeader->addWidget(makeLabel(QStringLiteral("调用示例（bash / curl）"),
                                    QStringLiteral("FieldLabel"), serverCard));
    curlHeader->addStretch();
    auto *curlCopy = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                      QString(), serverCard);
    curlCopy->setObjectName(QStringLiteral("IconButton"));
    curlCopy->setFixedSize(28, 24);
    curlCopy->setCursor(Qt::PointingHandCursor);
    curlCopy->setToolTip(QStringLiteral("复制示例命令"));
    connect(curlCopy, &QPushButton::clicked, this, [this]() {
        copyText(m_curlView->toPlainText(), QStringLiteral("调用示例"));
    });
    curlHeader->addWidget(curlCopy);
    serverLayout->addLayout(curlHeader);

    m_curlView = new QPlainTextEdit(serverCard);
    m_curlView->setObjectName(QStringLiteral("CommandView"));
    m_curlView->setReadOnly(true);
    m_curlView->setFixedHeight(120);
    m_curlView->setWordWrapMode(QTextOption::WrapAnywhere);
    serverLayout->addWidget(m_curlView);
    serverLayout->addStretch();

    auto *controlCard = makeCard(body);
    auto *controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(18, 16, 18, 16);
    controlLayout->setSpacing(4);

    auto *controlHeader = new QHBoxLayout();
    auto *controlTitleBox = new QVBoxLayout();
    controlTitleBox->setSpacing(1);
    controlTitleBox->addWidget(makeLabel(QStringLiteral("GUI 控制 API"),
                                         QStringLiteral("SectionTitle"), controlCard));
    controlTitleBox->addWidget(makeLabel(QStringLiteral("由 LlamaDeck 提供 · 仅监听本机"),
                                         QStringLiteral("Hint"), controlCard));
    controlHeader->addLayout(controlTitleBox);
    controlHeader->addStretch();
    m_controlBaseLabel = makeLabel(QStringLiteral("http://127.0.0.1:8745"),
                                    QStringLiteral("Endpoint"), controlCard);
    controlHeader->addWidget(m_controlBaseLabel);
    controlLayout->addLayout(controlHeader);
    controlLayout->addSpacing(6);

    addEndpointRow(controlLayout, QStringLiteral("GET"), QStringLiteral("/api/status"),
                   QStringLiteral("查询状态、模型与实时速度"), true);
    controlLayout->addWidget(makeDivider(controlCard));
    addEndpointRow(controlLayout, QStringLiteral("GET"), QStringLiteral("/api/models"),
                   QStringLiteral("列出模型目录中的 GGUF 文件"), true);
    controlLayout->addWidget(makeDivider(controlCard));
    addEndpointRow(controlLayout, QStringLiteral("POST"), QStringLiteral("/api/launch"),
                   QStringLiteral("启动模型：{\"model\":\"文件名或完整路径\"}"), true);
    controlLayout->addWidget(makeDivider(controlCard));
    addEndpointRow(controlLayout, QStringLiteral("POST"), QStringLiteral("/api/stop"),
                   QStringLiteral("停止当前推理服务"), true);
    controlLayout->addWidget(makeDivider(controlCard));
    addEndpointRow(controlLayout, QStringLiteral("GET"), QStringLiteral("/api/history"),
                   QStringLiteral("返回启动历史记录"), true);
    controlLayout->addWidget(makeDivider(controlCard));
    addEndpointRow(controlLayout, QStringLiteral("GET"), QStringLiteral("/api/health"),
                   QStringLiteral("GUI 控制 API 存活检查"), true);

    controlLayout->addSpacing(8);
    auto *controlHint = makeLabel(QStringLiteral("控制 API 端口可在设置中修改，修改后即时生效。"
                                                 "所有接口仅绑定 127.0.0.1。"),
                                   QStringLiteral("Hint"), controlCard);
    controlHint->setWordWrap(true);
    controlLayout->addWidget(controlHint);
    controlLayout->addStretch();

    bodyLayout->addWidget(serverCard, 1);
    bodyLayout->addWidget(controlCard, 1);
    scroll->setWidget(body);

    layout->addWidget(scroll, 1);
}

void ApiDocsPage::addEndpointRow(QVBoxLayout *layout, const QString &method, const QString &path,
                                 const QString &description, bool controlApi)
{
    QWidget *row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(2, 7, 2, 7);
    rowLayout->setSpacing(10);

    auto *methodLabel = makeLabel(method, QString(), row);
    methodLabel->setObjectName(QStringLiteral("MethodPill"));
    methodLabel->setProperty("method", method);
    methodLabel->setAlignment(Qt::AlignCenter);
    methodLabel->setFixedWidth(48);
    rowLayout->addWidget(methodLabel, 0, Qt::AlignTop);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(1);
    auto *pathLabel = makeLabel(path, QStringLiteral("EndpointPath"), row);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLayout->addWidget(pathLabel);
    auto *descLabel = makeLabel(description, QStringLiteral("Hint"), row);
    descLabel->setWordWrap(true);
    textLayout->addWidget(descLabel);
    rowLayout->addLayout(textLayout, 1);

    auto *copyButton = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                        QString(), row);
    copyButton->setObjectName(QStringLiteral("IconButton"));
    copyButton->setFixedSize(28, 26);
    copyButton->setCursor(Qt::PointingHandCursor);
    copyButton->setToolTip(QStringLiteral("复制完整 URL"));
    connect(copyButton, &QPushButton::clicked, this, [this, path, controlApi]() {
        const QString base = controlApi ? controlBaseUrl() : serverBaseUrl();
        copyText(base + path, QStringLiteral("接口地址"));
    });
    rowLayout->addWidget(copyButton, 0, Qt::AlignTop);

    layout->addWidget(row);
}

QString ApiDocsPage::serverBaseUrl() const
{
    int port = m_settings->port;
    const ServerState state = m_controller->state();
    if (state == ServerState::Running || state == ServerState::Starting
        || state == ServerState::Stopping)
        port = m_controller->options().port;
    return QStringLiteral("http://127.0.0.1:%1").arg(port);
}

QString ApiDocsPage::controlBaseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_settings->controlApiPort);
}

void ApiDocsPage::updateEndpoints()
{
    m_serverBaseLabel->setText(serverBaseUrl());
    m_controlBaseLabel->setText(controlBaseUrl());

    m_curlView->setPlainText(
        QStringLiteral("curl %1/v1/chat/completions \\\n"
                       "  -H \"Content-Type: application/json\" \\\n"
                       "  -d '{\"model\":\"default\",\"messages\":[{\"role\":\"user\",\"content\":\"你好\"}],\"stream\":false}'")
            .arg(serverBaseUrl()));
}

void ApiDocsPage::copyText(const QString &text, const QString &what)
{
    QGuiApplication::clipboard()->setText(text);
    emit toastRequested(QStringLiteral("%1已复制到剪贴板。").arg(what));
}
