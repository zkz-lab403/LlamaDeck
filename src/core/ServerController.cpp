#include "ServerController.h"

#include "AppSettings.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString stripAnsi(const QString &text)
{
    static const QRegularExpression ansi(QStringLiteral("\x1B\\[[0-9;]*[A-Za-z]"));
    QString result = text;
    result.remove(ansi);
    result.remove(QLatin1Char('\r'));
    return result;
}

QString quoteArgument(const QString &argument)
{
    if (argument.isEmpty())
        return QStringLiteral("\"\"");
    if (!argument.contains(QLatin1Char(' ')) && !argument.contains(QLatin1Char('\t'))
        && !argument.contains(QLatin1Char('"')))
        return argument;
    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString stateSlug(ServerState state)
{
    switch (state) {
    case ServerState::Stopped:  return QStringLiteral("stopped");
    case ServerState::Starting: return QStringLiteral("starting");
    case ServerState::Running:  return QStringLiteral("running");
    case ServerState::Stopping: return QStringLiteral("stopping");
    case ServerState::Failed:   return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

} // namespace

ServerController::ServerController(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_network(new QNetworkAccessManager(this))
{
    m_healthTimer.setInterval(1500);
    m_metricsTimer.setInterval(2000);
    connect(&m_healthTimer, &QTimer::timeout, this, &ServerController::pollHealth);
    connect(&m_metricsTimer, &QTimer::timeout, this, &ServerController::pollMetrics);
}

ServerController::~ServerController()
{
    if (m_process) {
        m_process->blockSignals(true);
        m_process->kill();
        m_process->waitForFinished(1500);
    }
}

QStringList ServerController::buildArguments(const LaunchOptions &options)
{
    QStringList args;
    args << QStringLiteral("--model") << QDir::toNativeSeparators(options.modelPath);
    args << QStringLiteral("--host") << QStringLiteral("127.0.0.1");
    args << QStringLiteral("--port") << QString::number(options.port);
    args << QStringLiteral("-c") << QString::number(options.ctx);

    if (options.gpuLayers >= 0)
        args << QStringLiteral("-ngl") << QString::number(options.gpuLayers);
    if (options.threads > 0)
        args << QStringLiteral("-t") << QString::number(options.threads);
    if (options.flashAttn)
        args << QStringLiteral("-fa") << QStringLiteral("on");
    if (options.jinja)
        args << QStringLiteral("--jinja");
    if (!options.apiKey.isEmpty())
        args << QStringLiteral("--api-key") << options.apiKey;

    if (!options.extraArgs.trimmed().isEmpty())
        args << QProcess::splitCommand(options.extraArgs.trimmed());

    return args;
}

QString ServerController::formatCommand(const QString &executable, const QStringList &arguments)
{
    QStringList parts;
    parts << quoteArgument(QDir::toNativeSeparators(executable));
    for (const QString &argument : arguments)
        parts << quoteArgument(argument);
    return parts.join(QLatin1Char(' '));
}

bool ServerController::start(const LaunchOptions &options)
{
    if (m_process) {
        m_lastError = QStringLiteral("推理服务已在运行，请先停止当前实例。");
        emit errorOccurred(m_lastError);
        return false;
    }

    const QString executable = m_settings->llamaServerPath.trimmed();
    if (executable.isEmpty() || !QFileInfo::exists(executable)) {
        m_lastError = QStringLiteral("未找到 llama-server，请在“设置”中选择可执行文件路径。");
        setState(ServerState::Failed);
        emit errorOccurred(m_lastError);
        return false;
    }

    if (options.modelPath.isEmpty() || !QFileInfo::exists(options.modelPath)) {
        m_lastError = QStringLiteral("模型文件不存在，请重新选择或扫描模型目录。");
        setState(ServerState::Failed);
        emit errorOccurred(m_lastError);
        return false;
    }

    {
        QTcpSocket probe;
        probe.connectToHost(QStringLiteral("127.0.0.1"), quint16(options.port));
        if (probe.waitForConnected(150)) {
            probe.abort();
            m_lastError = QStringLiteral("端口 %1 已被占用，请更换端口后重试。").arg(options.port);
            setState(ServerState::Failed);
            emit errorOccurred(m_lastError);
            return false;
        }
    }

    m_options = options;
    m_stats = ServerStats();
    m_logs.clear();
    m_buffer.clear();
    m_lastError.clear();
    m_hasMetricsBaseline = false;
    m_lastGenTokens = m_lastGenSeconds = 0.0;
    m_lastPromptTokens = m_lastPromptSeconds = 0.0;
    m_timedGenTokens = m_timedGenSeconds = 0.0;
    m_metricsAvailable = true;
    m_runningEmitted = false;

    const QStringList args = buildArguments(options);
    m_command = formatCommand(executable, args);

    m_process = new QProcess(this);
    m_process->setProgram(executable);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(QFileInfo(options.modelPath).absolutePath());
    m_process->setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif

    connect(m_process, &QProcess::readyRead, this, &ServerController::onReadyRead);
    connect(m_process, &QProcess::finished, this, &ServerController::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &ServerController::onProcessError);

    setState(ServerState::Starting);
    addLog(QStringLiteral("$ ") + m_command);
    emit launchStarted(options, m_command);

    m_startClock.start();
    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        failStart(QStringLiteral("无法启动 llama-server：%1").arg(m_process->errorString()));
        return false;
    }

    m_stats.pid = qint64(m_process->processId());
    emit statsChanged(m_stats);
    m_healthTimer.start();
    return true;
}

void ServerController::stop()
{
    if (!m_process) {
        setState(ServerState::Stopped);
        return;
    }

    if (m_state == ServerState::Stopping)
        return;

    m_healthTimer.stop();
    m_metricsTimer.stop();
    setState(ServerState::Stopping);
    addLog(QStringLiteral("正在停止推理服务…"));

    m_process->terminate();
    QTimer::singleShot(900, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
}

void ServerController::detachRunningProcess()
{
    if (!m_process)
        return;
    m_healthTimer.stop();
    m_metricsTimer.stop();
    m_process->blockSignals(true);
    m_process->setParent(nullptr); // intentionally leaked so the child keeps running
    m_process = nullptr;
    m_state = ServerState::Stopped;
    m_runningEmitted = false;
    m_command.clear();
}

void ServerController::clearLog()
{
    m_logs.clear();
}

bool ServerController::isRunning() const
{
    return m_state == ServerState::Starting || m_state == ServerState::Running
        || m_state == ServerState::Stopping;
}

double ServerController::averageGenTps() const
{
    if (m_timedGenSeconds > 0.0)
        return m_timedGenTokens / m_timedGenSeconds;
    if (m_stats.genSeconds > 0.0)
        return double(m_stats.genTokens) / m_stats.genSeconds;
    if (m_stats.startedAt.isValid()) {
        const qint64 seconds = m_stats.startedAt.secsTo(QDateTime::currentDateTime());
        if (seconds > 0)
            return double(m_stats.genTokens) / double(seconds);
    }
    return 0.0;
}

void ServerController::setState(ServerState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void ServerController::addLog(const QString &line)
{
    if (line.trimmed().isEmpty())
        return;
    m_logs.append(line);
    while (m_logs.size() > 6000)
        m_logs.removeFirst();
    emit logLine(line);
}

void ServerController::onReadyRead()
{
    if (!m_process)
        return;

    m_buffer += m_process->readAll();
    int index = -1;
    while ((index = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray raw = m_buffer.left(index);
        m_buffer.remove(0, index + 1);
        const QString line = stripAnsi(QString::fromUtf8(raw));
        if (line.trimmed().isEmpty())
            continue;
        addLog(line);
        handleLine(line);
    }

    if (m_buffer.size() > 1024 * 1024)
        m_buffer.clear();
}

void ServerController::handleLine(const QString &line)
{
    static const QRegularExpression promptRx(QString::fromLatin1(
        R"(prompt eval time\s*=\s*([\d.]+)\s*ms\s*/\s*(\d+)\s*(?:tokens|runs).*?([\d.]+)\s*tokens per second)"));
    static const QRegularExpression genRx(QString::fromLatin1(
        R"((?:^|\|)\s*(?:llama_print_timings:|llama_perf_context_print:)?\s*eval time\s*=\s*([\d.]+)\s*ms\s*/\s*(\d+)\s*(?:tokens|runs).*?([\d.]+)\s*tokens per second)"));

    if (line.contains(QLatin1String("server is listening"))
        || line.contains(QLatin1String("HTTP server listening"))) {
        setRunning();
    }

    if (m_state == ServerState::Starting) {
        const QString lower = line.toLower();
        if (lower.contains(QLatin1String("couldn't bind"))
            || lower.contains(QLatin1String("failed to bind"))
            || lower.contains(QLatin1String("address already in use"))
            || lower.contains(QLatin1String("error loading model"))
            || lower.contains(QLatin1String("failed to load model"))
            || lower.contains(QLatin1String("unable to load model"))) {
            m_lastError = QStringLiteral("推理服务启动失败，请查看日志了解详情。");
            emit errorOccurred(m_lastError);
        }
    }

    const auto promptMatch = promptRx.match(line);
    if (promptMatch.hasMatch()) {
        const double milliseconds = promptMatch.captured(1).toDouble();
        const int tokens = promptMatch.captured(2).toInt();
        const double tps = promptMatch.captured(3).toDouble();
        m_stats.promptTokens += quint64(tokens);
        m_stats.promptSeconds += milliseconds / 1000.0;
        if (tps > 0.0)
            m_stats.promptTps = tps;
        emit statsChanged(m_stats);
    }

    const auto genMatch = genRx.match(line);
    if (genMatch.hasMatch()) {
        const double milliseconds = genMatch.captured(1).toDouble();
        const int tokens = genMatch.captured(2).toInt();
        const double tps = genMatch.captured(3).toDouble();
        m_stats.genTokens += quint64(tokens);
        m_stats.genSeconds += milliseconds / 1000.0;
        m_timedGenTokens += double(tokens);
        m_timedGenSeconds += milliseconds / 1000.0;
        if (tps > 0.0) {
            m_stats.genTps = tps;
            m_stats.peakGenTps = qMax(m_stats.peakGenTps, tps);
        }
        emit statsChanged(m_stats);
    }
}

void ServerController::setRunning()
{
    if (m_state != ServerState::Starting)
        return;

    m_healthTimer.stop();
    m_stats.startedAt = QDateTime::currentDateTime();
    if (m_process)
        m_stats.pid = qint64(m_process->processId());
    setState(ServerState::Running);
    m_metricsTimer.start();
    emit statsChanged(m_stats);

    if (!m_runningEmitted) {
        m_runningEmitted = true;
        emit running();
    }
    pollMetrics();
}

void ServerController::failStart(const QString &message)
{
    m_lastError = message;
    m_healthTimer.stop();
    m_metricsTimer.stop();

    if (m_process) {
        m_process->blockSignals(true);
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_stats.pid = 0;
    setState(ServerState::Failed);
    addLog(QStringLiteral("[错误] ") + message);
    emit errorOccurred(message);

    if (m_runningEmitted || !m_command.isEmpty()) {
        m_runningEmitted = false;
        emit stopped(-1, QStringLiteral("启动失败"));
    }
    m_command.clear();
}

void ServerController::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)

    const ServerState previous = m_state;
    m_healthTimer.stop();
    m_metricsTimer.stop();

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    QString result;
    ServerState finalState = ServerState::Stopped;

    if (previous == ServerState::Stopping) {
        result = QStringLiteral("已停止");
    } else if (previous == ServerState::Starting) {
        result = QStringLiteral("启动失败");
        finalState = ServerState::Failed;
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("推理服务在启动过程中退出（代码 %1）。").arg(exitCode);
    } else if (previous == ServerState::Running) {
        if (exitCode == 0) {
            result = QStringLiteral("已停止");
        } else {
            result = QStringLiteral("异常退出");
            finalState = ServerState::Failed;
            m_lastError = QStringLiteral("推理服务异常退出（代码 %1）。").arg(exitCode);
        }
    } else {
        result = QStringLiteral("已停止");
    }

    m_stats.pid = 0;
    m_stats.genTps = 0.0;
    m_stats.promptTps = 0.0;

    if (m_runningEmitted || !m_command.isEmpty()) {
        m_runningEmitted = false;
        emit stopped(exitCode, result);
    }
    m_command.clear();

    setState(finalState);

    if (finalState == ServerState::Failed && !m_lastError.isEmpty())
        emit errorOccurred(m_lastError);
}

void ServerController::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart && m_state == ServerState::Starting) {
        failStart(QStringLiteral("无法启动 llama-server：%1")
                      .arg(m_process ? m_process->errorString() : QStringLiteral("未知错误")));
    }
}

QNetworkRequest ServerController::apiRequest(const QString &path) const
{
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                                     .arg(m_options.port)
                                     .arg(path)));
    request.setTransferTimeout(3000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LlamaDeck/1.0"));
    if (!m_options.apiKey.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_options.apiKey.toUtf8());
    return request;
}

void ServerController::pollHealth()
{
    if (m_state != ServerState::Starting || !m_process) {
        m_healthTimer.stop();
        return;
    }

    if (m_startClock.hasExpired(180000)) {
        failStart(QStringLiteral("启动超时（180 秒），请检查模型文件或日志输出。"));
        return;
    }

    QNetworkReply *reply = m_network->get(apiRequest(QStringLiteral("/health")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError
            && statusCode == 200
            && body.contains("ok");
        reply->deleteLater();
        if (ok)
            setRunning();
    });
}

void ServerController::pollMetrics()
{
    if (m_state != ServerState::Running || !m_process || !m_metricsAvailable)
        return;

    QNetworkReply *reply = m_network->get(apiRequest(QStringLiteral("/metrics")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode != 200) {
            m_metricsAvailable = false;
            m_metricsTimer.stop();
            reply->deleteLater();
            return;
        }
        reply->deleteLater();

        QHash<QString, double> values;
        const QList<QByteArray> lines = body.split('\n');
        for (const QByteArray &raw : lines) {
            const QByteArray line = raw.trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            const int space = line.indexOf(' ');
            if (space <= 0)
                continue;
            QString name = QString::fromUtf8(line.left(space));
            const int brace = name.indexOf(QLatin1Char('{'));
            if (brace > 0)
                name.truncate(brace);
            bool ok = false;
            const double value = line.mid(space + 1).trimmed().toDouble(&ok);
            if (ok)
                values.insert(name, value);
        }

        if (values.isEmpty())
            return;

        auto findValue = [&values](std::initializer_list<const char *> names) {
            for (const char *name : names) {
                const auto it = values.constFind(QString::fromLatin1(name));
                if (it != values.cend())
                    return it.value();
            }
            return -1.0;
        };

        const double genTotal = findValue({"llamacpp:tokens_predicted_total"});
        const double genSeconds = findValue({"llamacpp:tokens_predicted_seconds_total",
                                             "llamacpp:tokens_predicted_seconds"});
        const double promptTotal = findValue({"llamacpp:prompt_tokens_total"});
        const double promptSeconds = findValue({"llamacpp:prompt_seconds_total",
                                                "llamacpp:prompt_tokens_seconds_total"});
        const double active = findValue({"llamacpp:requests_processing"});

        if (genTotal >= 0.0) {
            if (m_hasMetricsBaseline && genSeconds >= 0.0) {
                const double deltaTokens = genTotal - m_lastGenTokens;
                const double deltaSeconds = genSeconds - m_lastGenSeconds;
                if (deltaSeconds > 1e-9) {
                    const double rate = qMax(0.0, deltaTokens / deltaSeconds);
                    m_stats.genTps = rate;
                    m_stats.peakGenTps = qMax(m_stats.peakGenTps, rate);
                } else if (deltaTokens <= 0.0) {
                    m_stats.genTps = 0.0;
                }
            }
            m_stats.genTokens = qMax(m_stats.genTokens, quint64(genTotal));
            m_lastGenTokens = genTotal;
            if (genSeconds >= 0.0) {
                m_lastGenSeconds = genSeconds;
                m_stats.genSeconds = qMax(m_stats.genSeconds, genSeconds);
            }
            m_hasMetricsBaseline = true;
        }

        if (promptTotal >= 0.0) {
            if (m_hasMetricsBaseline && promptSeconds >= 0.0) {
                const double deltaTokens = promptTotal - m_lastPromptTokens;
                const double deltaSeconds = promptSeconds - m_lastPromptSeconds;
                if (deltaSeconds > 1e-9 && deltaTokens > 0.0)
                    m_stats.promptTps = qMax(0.0, deltaTokens / deltaSeconds);
            }
            if (m_stats.promptTokens < quint64(promptTotal))
                m_stats.promptTokens = quint64(promptTotal);
            m_lastPromptTokens = promptTotal;
            if (promptSeconds >= 0.0) {
                m_lastPromptSeconds = promptSeconds;
                m_stats.promptSeconds = qMax(m_stats.promptSeconds, promptSeconds);
            }
        }

        if (active >= 0.0)
            m_stats.activeRequests = int(active);

        emit statsChanged(m_stats);
    });
}
