#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QWidget>

class AppSettings;
class ServerController;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class SpeedChart;
class StatCard;
class ChatBubble;

class ApiPage : public QWidget {
    Q_OBJECT

public:
    ApiPage(AppSettings *settings, ServerController *controller, QWidget *parent = nullptr);

signals:
    void toastRequested(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    QWidget *buildChatCard();
    QWidget *buildSpeedPanel();
    void updateStatus();
    void sendMessage();
    void stopStreaming();
    void processStream(QNetworkReply *reply);
    void updateStreamStats();
    void resetStreamStats();
    void finishStreaming(bool failed, const QString &errorText, bool stopped = false);
    ChatBubble *appendBubble(const QString &text, bool user);
    void scrollChatToBottom();
    void resetChat();
    QString serverBaseUrl() const;

    AppSettings *m_settings = nullptr;
    ServerController *m_controller = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_speedTimer = nullptr;

    QLabel *m_connectionBadge = nullptr;
    QLabel *m_statusLabel = nullptr;
    QScrollArea *m_chatScroll = nullptr;
    QWidget *m_chatBody = nullptr;
    QVBoxLayout *m_chatLayout = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    ChatBubble *m_streamingBubble = nullptr;

    QLabel *m_speedBadge = nullptr;
    StatCard *m_speedCard = nullptr;
    StatCard *m_latencyCard = nullptr;
    StatCard *m_tokenCard = nullptr;
    SpeedChart *m_streamChart = nullptr;

    QJsonArray m_messages;
    QByteArray m_sseBuffer;
    QByteArray m_errorBuffer;
    QString m_assistantText;
    QElapsedTimer m_streamClock;
    bool m_sending = false;
    int m_streamTokens = 0;
    int m_sampledTokens = 0;
    qint64 m_firstTokenMs = -1;
    qint64 m_lastSampleMs = 0;
};
