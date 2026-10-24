#pragma once

#include "Types.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

class AppSettings;

class ServerController : public QObject {
    Q_OBJECT

public:
    explicit ServerController(AppSettings *settings, QObject *parent = nullptr);
    ~ServerController() override;

    static QStringList buildArguments(const LaunchOptions &options);
    static QString formatCommand(const QString &executable, const QStringList &arguments);

    bool start(const LaunchOptions &options);
    void stop();
    void detachRunningProcess();
    void clearLog();

    bool isRunning() const;
    ServerState state() const { return m_state; }
    const ServerStats &stats() const { return m_stats; }
    double averageGenTps() const;
    const LaunchOptions &options() const { return m_options; }
    QString currentCommand() const { return m_command; }
    QStringList logs() const { return m_logs; }
    QString lastError() const { return m_lastError; }
    quint16 port() const { return quint16(m_options.port); }

signals:
    void stateChanged(ServerState state);
    void logLine(const QString &line);
    void statsChanged(const ServerStats &stats);
    void launchStarted(const LaunchOptions &options, const QString &command);
    void running();
    void stopped(int exitCode, const QString &result);
    void errorOccurred(const QString &message);

private:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void setState(ServerState state);
    void handleLine(const QString &line);
    void setRunning();
    void failStart(const QString &message);
    void pollHealth();
    void pollMetrics();
    void addLog(const QString &line);
    QNetworkRequest apiRequest(const QString &path) const;

    AppSettings *m_settings = nullptr;
    QProcess *m_process = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QTimer m_healthTimer;
    QTimer m_metricsTimer;

    LaunchOptions m_options;
    ServerStats m_stats;
    ServerState m_state = ServerState::Stopped;
    QString m_command;
    QString m_lastError;
    QStringList m_logs;
    QByteArray m_buffer;
    QElapsedTimer m_startClock;

    bool m_hasMetricsBaseline = false;
    double m_lastGenTokens = 0.0;
    double m_lastGenSeconds = 0.0;
    double m_lastPromptTokens = 0.0;
    double m_lastPromptSeconds = 0.0;
    double m_timedGenTokens = 0.0;
    double m_timedGenSeconds = 0.0;
    bool m_metricsAvailable = true;
    bool m_runningEmitted = false;
};
