#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QString>

class ServerController;
class AppSettings;
class HistoryStore;

class ControlApiServer : public QObject {
    Q_OBJECT

public:
    ControlApiServer(ServerController *controller, AppSettings *settings, HistoryStore *history,
                     QObject *parent = nullptr);

    bool start(quint16 port);
    void stop();
    void applyConfig(bool enabled, quint16 port);

    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_port; }
    QString errorString() const { return m_error; }
    QString baseUrl() const;

signals:
    void listeningChanged(bool listening);
    void errorOccurred(const QString &message);

private:
    void onNewConnection();
    void processSocket(QTcpSocket *socket);
    void handleRequest(QTcpSocket *socket, const QByteArray &method, const QString &path,
                       const QByteArray &body);
    void respondJson(QTcpSocket *socket, int status, const QJsonObject &object) const;
    void respondText(QTcpSocket *socket, int status, const QByteArray &contentType,
                     const QByteArray &payload) const;

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    ServerController *m_controller = nullptr;
    AppSettings *m_settings = nullptr;
    HistoryStore *m_history = nullptr;
    quint16 m_port = 0;
    QString m_error;
};
