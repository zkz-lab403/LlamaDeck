#include "ApiPage.h"

#include "AppSettings.h"
#include "Icons.h"
#include "KnowledgeStore.h"
#include "ServerController.h"
#include "SpeedChart.h"
#include "Theme.h"
#include "Types.h"
#include "Widgets.h"

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

ApiPage::ApiPage(AppSettings *settings, ServerController *controller, KnowledgeStore *knowledge,
                 QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_controller(controller)
    , m_knowledge(knowledge)
    , m_network(new QNetworkAccessManager(this))
{
    m_speedTimer = new QTimer(this);
    m_speedTimer->setInterval(400);
    connect(m_speedTimer, &QTimer::timeout, this, &ApiPage::updateStreamStats);

    buildUi();

    connect(m_controller, &ServerController::stateChanged, this, [this](ServerState state) {
        if (state != ServerState::Running && m_sending)
            finishStreaming(true, QStringLiteral("推理服务已停止。"));
        updateStatus();
    });
    connect(m_settings, &AppSettings::changed, this, &ApiPage::updateStatus);

    resetChat();
    resetStreamStats();
    updateStatus();
}

void ApiPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 10, 32, 24);
    layout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);

    splitter->addWidget(buildChatCard());
    splitter->addWidget(buildSpeedPanel());
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({820, 268});

    layout->addWidget(splitter, 1);
}

QWidget *ApiPage::buildChatCard()
{
    auto *card = makeCard(this);
    card->setMinimumWidth(500);
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

    m_knowledgeBadge = makeLabel(QString(), QStringLiteral("Badge"), card);
    m_knowledgeBadge->setProperty("variant", QStringLiteral("accent"));
    m_knowledgeBadge->setAlignment(Qt::AlignCenter);
    m_knowledgeBadge->hide();
    header->addWidget(m_knowledgeBadge);

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

    m_stopButton = new QPushButton(Icons::icon(Icons::Glyph::Stop, Qt::white, 14),
                                    QStringLiteral("停止"), card);
    m_stopButton->setObjectName(QStringLiteral("Danger"));
    m_stopButton->setMinimumSize(88, 42);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setToolTip(QStringLiteral("停止当前回答"));
    m_stopButton->hide();
    connect(m_stopButton, &QPushButton::clicked, this, &ApiPage::stopStreaming);
    inputRow->addWidget(m_stopButton, 0, Qt::AlignBottom);

    m_sendButton = new QPushButton(Icons::icon(Icons::Glyph::Send, Qt::white, 15),
                                    QStringLiteral("发送"), card);
    m_sendButton->setObjectName(QStringLiteral("Primary"));
    m_sendButton->setMinimumSize(96, 42);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    connect(m_sendButton, &QPushButton::clicked, this, &ApiPage::sendMessage);
    inputRow->addWidget(m_sendButton, 0, Qt::AlignBottom);
    layout->addLayout(inputRow);

    auto *footerRow = new QHBoxLayout();
    footerRow->setSpacing(8);
    auto *footer = makeLabel(QStringLiteral("Enter 发送 · Shift+Enter 换行 · 通过 /v1/chat/completions 流式返回"),
                             QStringLiteral("Hint"), card);
    footer->setWordWrap(true);
    footerRow->addWidget(footer, 1);

    auto *knowledgeLabel = makeLabel(QStringLiteral("知识库"), QStringLiteral("Hint"), card);
    footerRow->addWidget(knowledgeLabel);
    m_useKnowledge = new ToggleSwitch(card);
    m_useKnowledge->setToolTip(QStringLiteral("发送时检索知识库并注入相关片段"));
    footerRow->addWidget(m_useKnowledge);
    layout->addLayout(footerRow);

    return card;
}

QWidget *ApiPage::buildSpeedPanel()
{
    auto *card = makeCard(this);
    card->setMinimumWidth(240);
    card->setMaximumWidth(340);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    titleBox->addWidget(makeLabel(QStringLiteral("实时速度"), QStringLiteral("SectionTitle"), card));
    titleBox->addWidget(makeLabel(QStringLiteral("本轮流式输出"), QStringLiteral("Hint"), card));
    header->addLayout(titleBox);
    header->addStretch();

    m_speedBadge = makeLabel(QStringLiteral("空闲"), QStringLiteral("Badge"), card);
    m_speedBadge->setProperty("variant", QStringLiteral("muted"));
    m_speedBadge->setAlignment(Qt::AlignCenter);
    header->addWidget(m_speedBadge);
    layout->addLayout(header);

    layout->addWidget(makeDivider(card));

    m_speedCard = new StatCard(QStringLiteral("生成速度"), card);
    m_speedCard->setMinimumHeight(96);
    m_speedCard->setAccent(Theme::Accent);
    m_speedCard->setValue(QStringLiteral("—"));
    m_speedCard->setCaption(QStringLiteral("发送请求后实时统计"));
    layout->addWidget(m_speedCard);

    m_latencyCard = new StatCard(QStringLiteral("首字延迟"), card);
    m_latencyCard->setMinimumHeight(88);
    m_latencyCard->setAccent(Theme::Info);
    m_latencyCard->setValue(QStringLiteral("—"), QStringLiteral("ms"));
    m_latencyCard->setCaption(QStringLiteral("从发送到首个分片"));
    layout->addWidget(m_latencyCard);

    m_tokenCard = new StatCard(QStringLiteral("已生成"), card);
    m_tokenCard->setMinimumHeight(88);
    m_tokenCard->setAccent(Theme::Success);
    m_tokenCard->setValue(QStringLiteral("0"), QStringLiteral("tokens"));
    m_tokenCard->setCaption(QStringLiteral("本轮已接收分片数"));
    layout->addWidget(m_tokenCard);

    auto *chartHeader = new QHBoxLayout();
    chartHeader->addWidget(makeLabel(QStringLiteral("速度曲线"), QStringLiteral("FieldLabel"), card));
    chartHeader->addStretch();
    auto *chartHint = makeLabel(QStringLiteral("每 0.4 秒"), QStringLiteral("Hint"), card);
    chartHeader->addWidget(chartHint);
    layout->addLayout(chartHeader);

    m_streamChart = new SpeedChart(card);
    m_streamChart->setMinimumHeight(150);
    m_streamChart->setCapacity(60);
    layout->addWidget(m_streamChart, 1);

    auto *hint = makeLabel(QStringLiteral("按 SSE 流式分片统计，仅供参考"), QStringLiteral("Hint"), card);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    return card;
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

    m_sendButton->setEnabled(running && !m_sending);
    m_input->setEnabled(running && !m_sending);
    m_stopButton->setVisible(m_sending);
    m_stopButton->setEnabled(m_sending);
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
    m_stopButton->setVisible(true);
    m_stopButton->setEnabled(true);

    m_speedBadge->setText(QStringLiteral("生成中"));
    m_speedBadge->setProperty("variant", QStringLiteral("accent"));
    repolish(m_speedBadge);
    resetStreamStats();
    m_streamClock.start();
    m_speedTimer->start();

    QNetworkRequest request(QUrl(serverBaseUrl() + QStringLiteral("/v1/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LlamaDeck/1.0"));
    if (!m_controller->options().apiKey.isEmpty())
        request.setRawHeader("Authorization",
                             "Bearer " + m_controller->options().apiKey.toUtf8());

    QJsonArray messages = m_messages;
    m_knowledgeBadge->hide();
    if (m_useKnowledge->isChecked() && m_knowledge && m_knowledge->documentCount() > 0) {
        const QVector<KnowledgeHit> hits = m_knowledge->search(text, 3);
        if (hits.isEmpty()) {
            emit toastRequested(QStringLiteral("知识库未命中相关内容，本次直接回答。"));
        } else {
            QString context = QStringLiteral(
                "以下是从本地知识库检索到的片段，请优先依据这些内容回答；"
                "如果与问题无关，请忽略并使用你自己的知识。\n\n");
            for (int i = 0; i < hits.size(); ++i) {
                context += QStringLiteral("[%1] 来源：%2\n%3\n\n")
                               .arg(i + 1)
                               .arg(hits.at(i).title, hits.at(i).text);
            }

            QJsonArray withContext;
            withContext.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                           {QStringLiteral("content"), context}});
            for (const QJsonValue &message : messages)
                withContext.append(message);
            messages = withContext;

            m_knowledgeBadge->setText(QStringLiteral("知识库 ×%1").arg(hits.size()));
            m_knowledgeBadge->show();
        }
    }

    QJsonObject payload;
    payload[QStringLiteral("model")] = QStringLiteral("default");
    payload[QStringLiteral("messages")] = messages;
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

void ApiPage::stopStreaming()
{
    if (!m_sending || !m_reply)
        return;

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();

    finishStreaming(false, QString(), true);
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
        ++m_streamTokens;
        if (m_firstTokenMs < 0) {
            m_firstTokenMs = m_streamClock.elapsed();
            m_latencyCard->setValue(QString::number(m_firstTokenMs), QStringLiteral("ms"));
        }
        m_tokenCard->setValue(QString::number(m_streamTokens), QStringLiteral("tokens"));

        if (m_streamingBubble)
            m_streamingBubble->setText(m_assistantText);
        scrollChatToBottom();
    }
}

void ApiPage::updateStreamStats()
{
    if (!m_sending || !m_streamClock.isValid())
        return;

    const qint64 now = m_streamClock.elapsed();
    const qint64 dt = now - m_lastSampleMs;
    if (dt <= 0)
        return;

    const int delta = m_streamTokens - m_sampledTokens;
    m_streamChart->addSample(delta * 1000.0 / double(dt));
    m_sampledTokens = m_streamTokens;
    m_lastSampleMs = now;

    if (m_firstTokenMs >= 0) {
        const double seconds = qMax<qint64>(1, now - m_firstTokenMs) / 1000.0;
        m_speedCard->setValue(QString::number(m_streamTokens / seconds, 'f', 1),
                              QStringLiteral("tokens/s"));
        m_speedCard->setCaption(QStringLiteral("平均速度 · 已用时 %1 s")
                                    .arg(seconds, 0, 'f', 1));
    }
}

void ApiPage::resetStreamStats()
{
    if (m_speedTimer)
        m_speedTimer->stop();
    m_streamClock.invalidate();
    m_streamTokens = 0;
    m_sampledTokens = 0;
    m_firstTokenMs = -1;
    m_lastSampleMs = 0;

    if (m_speedCard) {
        m_speedCard->setValue(QStringLiteral("—"));
        m_speedCard->setCaption(QStringLiteral("发送请求后实时统计"));
    }
    if (m_latencyCard)
        m_latencyCard->setValue(QStringLiteral("—"), QStringLiteral("ms"));
    if (m_tokenCard)
        m_tokenCard->setValue(QStringLiteral("0"), QStringLiteral("tokens"));
    if (m_streamChart)
        m_streamChart->clear();
}

void ApiPage::finishStreaming(bool failed, const QString &errorText, bool stopped)
{
    if (!m_sending && !m_streamingBubble)
        return;

    m_sending = false;
    m_speedTimer->stop();

    if (m_firstTokenMs >= 0 && m_streamClock.isValid()) {
        const double seconds = qMax<qint64>(1, m_streamClock.elapsed() - m_firstTokenMs) / 1000.0;
        m_speedCard->setValue(QString::number(m_streamTokens / seconds, 'f', 1),
                              QStringLiteral("tokens/s"));
        m_speedCard->setCaption(QStringLiteral("本轮平均 · 共 %1 tokens").arg(m_streamTokens));
    }

    if (stopped) {
        m_speedBadge->setText(QStringLiteral("已停止"));
        m_speedBadge->setProperty("variant", QStringLiteral("warning"));
    } else {
        m_speedBadge->setText(QStringLiteral("空闲"));
        m_speedBadge->setProperty("variant", QStringLiteral("muted"));
    }
    repolish(m_speedBadge);

    if (stopped) {
        if (m_streamingBubble) {
            if (m_assistantText.trimmed().isEmpty()) {
                m_streamingBubble->setText(QStringLiteral("已停止生成。"));
            } else {
                m_streamingBubble->setText(m_assistantText + QStringLiteral("\n\n[已停止生成]"));
                m_messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                              {QStringLiteral("content"), m_assistantText}});
            }
        }
    } else if (failed) {
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

    resetStreamStats();
    m_speedBadge->setText(QStringLiteral("空闲"));
    m_speedBadge->setProperty("variant", QStringLiteral("muted"));
    repolish(m_speedBadge);
    m_knowledgeBadge->hide();

    appendBubble(QStringLiteral("启动推理服务后，即可在这里测试 OpenAI 兼容的对话接口。"),
                 false);
    updateStatus();
}
