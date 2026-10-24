#include "ApiPage.h"

#include "AppSettings.h"
#include "Icons.h"
#include "ServerController.h"
#include "Theme.h"
#include "Types.h"
#include "Widgets.h"

#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

class ChatBubble : public QFrame {
public:
    explicit ChatBubble(const QString &role, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("Bubble"));
        setProperty("role", role);
        setMaximumWidth(480);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(13, 9, 13, 10);
        layout->setSpacing(3);

        m_meta = new QLabel(role == QLatin1String("user") ? QStringLiteral("你")
                                                          : QStringLiteral("助手"),
                            this);
        m_meta->setObjectName(QStringLiteral("BubbleMeta"));
        m_meta->setAlignment(role == QLatin1String("user") ? Qt::AlignRight : Qt::AlignLeft);

        m_text = new QLabel(this);
        m_text->setObjectName(QStringLiteral("BubbleText"));
        m_text->setWordWrap(true);
        m_text->setTextFormat(Qt::PlainText);
        m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_text->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        layout->addWidget(m_meta);
        layout->addWidget(m_text);

        repolish(this);
        repolish(m_text);
        repolish(m_meta);
    }

    void setText(const QString &text)
    {
        m_text->setText(text);

        const QFontMetrics metrics(m_text->font());
        int naturalWidth = 0;
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines)
            naturalWidth = qMax(naturalWidth, metrics.horizontalAdvance(line));

        const int width = qBound(88, naturalWidth + 2, 420);
        const QRect bounds = metrics.boundingRect(QRect(0, 0, width, 100000),
                                                  Qt::TextWordWrap, text);
        m_text->setFixedSize(width, qMax(metrics.height(), bounds.height()) + 2);
        updateGeometry();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QFrame::showEvent(event);
        if (!m_text->text().isEmpty())
            setText(m_text->text());
    }

private:
    QLabel *m_meta = nullptr;
    QLabel *m_text = nullptr;
};

namespace {

QString extractErrorMessage(const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return QString();
    const QJsonValue error = doc.object().value(QStringLiteral("error"));
    if (error.isObject()) {
        const QJsonObject obj = error.toObject();
        const QString message = obj.value(QStringLiteral("message")).toString();
        if (!message.isEmpty())
            return message;
        return obj.value(QStringLiteral("type")).toString();
    }
    if (error.isString())
        return error.toString();
    return QString();
}

} // namespace

ApiPage::ApiPage(AppSettings *settings, ServerController *controller, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_controller(controller)
    , m_network(new QNetworkAccessManager(this))
{
    buildUi();

    connect(m_controller, &ServerController::stateChanged, this, [this](ServerState state) {
        if (state != ServerState::Running && m_sending)
            finishStreaming(true, QStringLiteral("推理服务已停止。"));
        updateStatus();
    });
    connect(m_settings, &AppSettings::changed, this, &ApiPage::updateStatus);

    resetChat();
    updateStatus();
}

void ApiPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 8, 28, 22);
    layout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);

    auto *scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(430);

    auto *leftBody = new QWidget(scroll);
    auto *leftLayout = new QVBoxLayout(leftBody);
    leftLayout->setContentsMargins(0, 0, 8, 0);
    leftLayout->setSpacing(14);

    auto *serverCard = makeCard(leftBody);
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

    leftLayout->addWidget(serverCard);

    auto *controlCard = makeCard(leftBody);
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

    controlLayout->addSpacing(8);
    auto *controlHint = makeLabel(QStringLiteral("控制 API 端口可在设置中修改，修改后即时生效。"),
                                   QStringLiteral("Hint"), controlCard);
    controlHint->setWordWrap(true);
    controlLayout->addWidget(controlHint);

    leftLayout->addWidget(controlCard);
    leftLayout->addStretch();
    scroll->setWidget(leftBody);

    splitter->addWidget(scroll);
    splitter->addWidget(buildChatCard());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({520, 760});

    layout->addWidget(splitter, 1);
}

QWidget *ApiPage::buildChatCard()
{
    auto *card = makeCard(this);
    card->setMinimumWidth(460);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout();
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    titleBox->addWidget(makeLabel(QStringLiteral("接口调试"), QStringLiteral("SectionTitle"), card));
    titleBox->addWidget(makeLabel(QStringLiteral("直接与本地 OpenAI 兼容接口对话"),
                                  QStringLiteral("Hint"), card));
    header->addLayout(titleBox);
    header->addStretch();

    m_connectionBadge = makeLabel(QStringLiteral("未连接"), QStringLiteral("Badge"), card);
    m_connectionBadge->setProperty("variant", QStringLiteral("muted"));
    m_connectionBadge->setAlignment(Qt::AlignCenter);
    header->addWidget(m_connectionBadge);

    auto *clearChat = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Muted, 14),
                                       QString(), card);
    clearChat->setObjectName(QStringLiteral("IconButton"));
    clearChat->setFixedSize(30, 26);
    clearChat->setToolTip(QStringLiteral("清空对话"));
    clearChat->setCursor(Qt::PointingHandCursor);
    connect(clearChat, &QPushButton::clicked, this, &ApiPage::resetChat);
    header->addWidget(clearChat);
    layout->addLayout(header);

    m_statusLabel = makeLabel(QStringLiteral("服务未运行 · 启动后即可对话"),
                              QStringLiteral("Hint"), card);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_statusLabel);
    layout->addWidget(makeDivider(card));

    m_chatScroll = new QScrollArea(card);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatBody = new QWidget(m_chatScroll);
    m_chatLayout = new QVBoxLayout(m_chatBody);
    m_chatLayout->setContentsMargins(4, 8, 4, 8);
    m_chatLayout->setSpacing(10);
    m_chatLayout->addStretch();
    m_chatScroll->setWidget(m_chatBody);
    layout->addWidget(m_chatScroll, 1);

    layout->addWidget(makeDivider(card));

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(10);
    m_input = new QPlainTextEdit(card);
    m_input->setObjectName(QStringLiteral("ChatInput"));
    m_input->setPlaceholderText(QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行…"));
    m_input->setFixedHeight(72);
    m_input->installEventFilter(this);
    inputRow->addWidget(m_input, 1);

    m_sendButton = new QPushButton(Icons::icon(Icons::Glyph::Send, Qt::white, 15),
                                    QStringLiteral("发送"), card);
    m_sendButton->setObjectName(QStringLiteral("Primary"));
    m_sendButton->setMinimumSize(96, 42);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    connect(m_sendButton, &QPushButton::clicked, this, &ApiPage::sendMessage);
    inputRow->addWidget(m_sendButton, 0, Qt::AlignBottom);
    layout->addLayout(inputRow);

    auto *footer = makeLabel(QStringLiteral("Enter 发送 · Shift+Enter 换行 · 通过 /v1/chat/completions 流式返回"),
                             QStringLiteral("Hint"), card);
    footer->setWordWrap(true);
    layout->addWidget(footer);

    return card;
}

void ApiPage::addEndpointRow(QVBoxLayout *layout, const QString &method, const QString &path,
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

bool ApiPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            sendMessage();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QString ApiPage::serverBaseUrl() const
{
    int port = m_settings->port;
    const ServerState state = m_controller->state();
    if (state == ServerState::Running || state == ServerState::Starting
        || state == ServerState::Stopping)
        port = m_controller->options().port;
    return QStringLiteral("http://127.0.0.1:%1").arg(port);
}

QString ApiPage::controlBaseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_settings->controlApiPort);
}

void ApiPage::updateStatus()
{
    const ServerState state = m_controller->state();
    const bool running = state == ServerState::Running;

    QString badgeText;
    QString badgeVariant;
    switch (state) {
    case ServerState::Running:
        badgeText = QStringLiteral("已连接");
        badgeVariant = QStringLiteral("success");
        break;
    case ServerState::Starting:
        badgeText = QStringLiteral("启动中");
        badgeVariant = QStringLiteral("warning");
        break;
    case ServerState::Stopping:
        badgeText = QStringLiteral("停止中");
        badgeVariant = QStringLiteral("warning");
        break;
    case ServerState::Failed:
        badgeText = QStringLiteral("连接失败");
        badgeVariant = QStringLiteral("danger");
        break;
    case ServerState::Stopped:
        badgeText = QStringLiteral("未连接");
        badgeVariant = QStringLiteral("muted");
        break;
    }

    m_connectionBadge->setText(badgeText);
    m_connectionBadge->setProperty("variant", badgeVariant);
    repolish(m_connectionBadge);

    if (running) {
        const QString model = QFileInfo(m_controller->options().modelPath).completeBaseName();
        m_statusLabel->setText(QStringLiteral("%1 · 模型 %2").arg(serverBaseUrl(),
                                                                   model.isEmpty()
                                                                       ? QStringLiteral("default")
                                                                       : model));
    } else if (state == ServerState::Starting) {
        m_statusLabel->setText(QStringLiteral("正在加载模型，完成后可发送请求…"));
    } else {
        m_statusLabel->setText(QStringLiteral("服务未运行 · 启动后即可对话"));
    }

    m_serverBaseLabel->setText(serverBaseUrl());
    m_controlBaseLabel->setText(controlBaseUrl());

    m_sendButton->setEnabled(running && !m_sending);
    m_input->setEnabled(running && !m_sending);

    m_curlView->setPlainText(
        QStringLiteral("curl %1/v1/chat/completions \\\n"
                       "  -H \"Content-Type: application/json\" \\\n"
                       "  -d '{\"model\":\"default\",\"messages\":[{\"role\":\"user\",\"content\":\"你好\"}],\"stream\":false}'")
            .arg(serverBaseUrl()));
}

void ApiPage::sendMessage()
{
    if (m_sending)
        return;
    if (m_controller->state() != ServerState::Running) {
        emit toastRequested(QStringLiteral("请先启动推理服务。"));
        return;
    }

    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    m_input->clear();
    appendBubble(text, true);

    m_messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                  {QStringLiteral("content"), text}});
    while (m_messages.size() > 20)
        m_messages.removeFirst();

    m_assistantText.clear();
    m_sseBuffer.clear();
    m_errorBuffer.clear();
    m_streamingBubble = appendBubble(QStringLiteral("正在生成…"), false);

    m_sending = true;
    m_sendButton->setEnabled(false);
    m_input->setEnabled(false);

    QNetworkRequest request(QUrl(serverBaseUrl() + QStringLiteral("/v1/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LlamaDeck/1.0"));
    if (!m_controller->options().apiKey.isEmpty())
        request.setRawHeader("Authorization",
                             "Bearer " + m_controller->options().apiKey.toUtf8());

    QJsonObject payload;
    payload[QStringLiteral("model")] = QStringLiteral("default");
    payload[QStringLiteral("messages")] = m_messages;
    payload[QStringLiteral("stream")] = true;
    payload[QStringLiteral("temperature")] = 0.7;

    QNetworkReply *reply = m_network->post(request,
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_reply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (reply != m_reply)
            return;
        processStream(reply);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_reply) {
            reply->deleteLater();
            return;
        }
        m_reply = nullptr;

        if (!m_sending) {
            reply->deleteLater();
            return;
        }

        QString errorText;
        if (reply->error() != QNetworkReply::NoError)
            errorText = extractErrorMessage(m_errorBuffer);
        if (errorText.isEmpty() && m_assistantText.isEmpty())
            errorText = extractErrorMessage(m_errorBuffer);
        if (errorText.isEmpty() && reply->error() != QNetworkReply::NoError)
            errorText = reply->errorString();

        const bool failed = !errorText.isEmpty() || m_assistantText.trimmed().isEmpty();
        reply->deleteLater();
        finishStreaming(failed, errorText.isEmpty() ? QStringLiteral("服务未返回内容。") : errorText);
    });
}

void ApiPage::processStream(QNetworkReply *reply)
{
    const QByteArray chunk = reply->readAll();
    m_errorBuffer.append(chunk);
    if (m_errorBuffer.size() > 16384)
        m_errorBuffer = m_errorBuffer.right(8192);

    m_sseBuffer.append(chunk);
    int newline = -1;
    while ((newline = m_sseBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_sseBuffer.left(newline).trimmed();
        m_sseBuffer.remove(0, newline + 1);
        if (!line.startsWith("data:"))
            continue;

        const QByteArray data = line.mid(5).trimmed();
        if (data == "[DONE]") {
            if (m_reply) {
                m_reply->disconnect(this);
                m_reply->abort();
                m_reply->deleteLater();
                m_reply = nullptr;
            }
            finishStreaming(false, QString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject())
            continue;
        const QJsonObject object = doc.object();

        if (object.contains(QStringLiteral("error"))) {
            const QString message = extractErrorMessage(data);
            if (m_reply) {
                m_reply->disconnect(this);
                m_reply->abort();
                m_reply->deleteLater();
                m_reply = nullptr;
            }
            finishStreaming(true, message.isEmpty() ? QStringLiteral("接口返回错误。") : message);
            return;
        }

        const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty())
            continue;
        const QJsonObject delta = choices.first().toObject()
                                      .value(QStringLiteral("delta")).toObject();
        const QString piece = delta.value(QStringLiteral("content")).toString();
        if (piece.isEmpty())
            continue;

        m_assistantText += piece;
        if (m_streamingBubble)
            m_streamingBubble->setText(m_assistantText);
        scrollChatToBottom();
    }
}

void ApiPage::finishStreaming(bool failed, const QString &errorText)
{
    if (!m_sending && !m_streamingBubble)
        return;

    m_sending = false;

    if (failed) {
        const QString message = errorText.isEmpty() ? QStringLiteral("请求失败。") : errorText;
        if (m_streamingBubble) {
            if (m_assistantText.trimmed().isEmpty())
                m_streamingBubble->setText(QStringLiteral("请求失败：%1").arg(message));
            else
                m_streamingBubble->setText(m_assistantText + QStringLiteral("\n\n[中断] ") + message);
        }
        emit toastRequested(QStringLiteral("API 请求失败：%1").arg(message));
    } else {
        if (m_streamingBubble) {
            if (m_assistantText.trimmed().isEmpty()) {
                m_streamingBubble->setText(QStringLiteral("（空响应）"));
            } else {
                m_messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                              {QStringLiteral("content"), m_assistantText}});
            }
        }
    }

    m_streamingBubble = nullptr;
    m_sseBuffer.clear();
    m_errorBuffer.clear();
    scrollChatToBottom();
    updateStatus();
}

ChatBubble *ApiPage::appendBubble(const QString &text, bool user)
{
    auto *bubble = new ChatBubble(user ? QStringLiteral("user") : QStringLiteral("assistant"),
                                   m_chatBody);
    bubble->setText(text);
    const int index = qMax(0, m_chatLayout->count() - 1);
    m_chatLayout->insertWidget(index, bubble, 0, user ? Qt::AlignRight : Qt::AlignLeft);
    scrollChatToBottom();
    return bubble;
}

void ApiPage::scrollChatToBottom()
{
    QTimer::singleShot(0, this, [this]() {
        if (m_chatScroll)
            m_chatScroll->verticalScrollBar()->setValue(
                m_chatScroll->verticalScrollBar()->maximum());
    });
}

void ApiPage::resetChat()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_sending = false;
    m_streamingBubble = nullptr;
    m_assistantText.clear();
    m_sseBuffer.clear();
    m_errorBuffer.clear();
    m_messages = QJsonArray();

    while (m_chatLayout->count() > 1) {
        QLayoutItem *item = m_chatLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    appendBubble(QStringLiteral("启动推理服务后，即可在这里测试 OpenAI 兼容的对话接口。"),
                 false);
    updateStatus();
}

void ApiPage::copyText(const QString &text, const QString &what)
{
    QGuiApplication::clipboard()->setText(text);
    emit toastRequested(QStringLiteral("%1已复制到剪贴板。").arg(what));
}
