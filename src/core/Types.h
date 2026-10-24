#pragma once

#include <QDateTime>
#include <QString>

struct LaunchOptions {
    QString modelPath;
    int port = 8080;
    int ctx = 4096;
    int gpuLayers = 99;
    int threads = 0;
    bool flashAttn = true;
    bool jinja = true;
    QString apiKey;
    QString extraArgs;
};

enum class ServerState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Failed
};

struct ServerStats {
    double genTps = 0.0;
    double promptTps = 0.0;
    double peakGenTps = 0.0;
    double genSeconds = 0.0;
    double promptSeconds = 0.0;
    quint64 genTokens = 0;
    quint64 promptTokens = 0;
    int activeRequests = 0;
    QDateTime startedAt;
    qint64 pid = 0;
};

struct HistoryEntry {
    QString id;
    QDateTime startedAt;
    QDateTime endedAt;
    QString modelName;
    QString modelPath;
    QString command;
    QString status;
    LaunchOptions options;
    double avgTps = 0.0;
    double peakTps = 0.0;
    quint64 tokens = 0;
    qint64 durationSec = 0;
};

inline QString serverStateText(ServerState state)
{
    switch (state) {
    case ServerState::Stopped:  return QStringLiteral("未运行");
    case ServerState::Starting: return QStringLiteral("启动中");
    case ServerState::Running:  return QStringLiteral("运行中");
    case ServerState::Stopping: return QStringLiteral("停止中");
    case ServerState::Failed:   return QStringLiteral("启动失败");
    }
    return QStringLiteral("未知");
}

inline QString formatDuration(qint64 seconds)
{
    if (seconds < 0)
        seconds = 0;
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
