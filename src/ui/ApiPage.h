#pragma once

#include <QByteArray>
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
class Badge;
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
    void addEndpointRow(QVBoxLayout *layout, const QString &method, const QString &path,
                        const QString &description, bool controlApi);
    void updateStatus();
    void sendMessage();
    void processStream(QNetworkReply *reply);
    void finishStreaming(bool failed, const QString &errorText);
    ChatBubble *appendBubble(const QString &text, bool user);
    void scrollChatToBottom();
    void resetChat();
    void copyText(const QString &text, const QString &what);
    QString serverBaseUrl() const;
    QString controlBaseUrl() const;

    AppSettings *m_settings = nullptr;
    ServerController *m_controller = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;

    QLabel *m_connectionBadge = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_serverBaseLabel = nullptr;
    QLabel *m_controlBaseLabel = nullptr;
    QPlainTextEdit *m_curlView = nullptr;
    QScrollArea *m_chatScroll = nullptr;
    QWidget *m_chatBody = nullptr;
    QVBoxLayout *m_chatLayout = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    ChatBubble *m_streamingBubble = nullptr;

    QJsonArray m_messages;
    QByteArray m_sseBuffer;
    QByteArray m_errorBuffer;
    QString m_assistantText;
    bool m_sending = false;
};
