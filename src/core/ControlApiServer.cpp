#include "ControlApiServer.h"

#include "AppSettings.h"
#include "HistoryStore.h"
#include "ModelStore.h"
#include "ServerController.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTcpSocket>
#include <QUrl>

namespace {

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

const char *reasonPhrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
    default:  return "OK";
    }
}

QString humanStatus(const QString &status)
{
    return status.isEmpty() ? QStringLiteral("未知") : status;
}

} // namespace

ControlApiServer::ControlApiServer(ServerController *controller, AppSettings *settings,
                                   HistoryStore *history, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_settings(settings)
    , m_history(history)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ControlApiServer::onNewConnection);
}

QString ControlApiServer::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_port);
}

bool ControlApiServer::start(quint16 port)
{
    if (m_server.isListening()) {
        if (m_port == port)
            return true;
        stop();
    }

    m_error.clear();
    if (!m_server.listen(QHostAddress::LocalHost, port)) {
        m_error = m_server.errorString();
        emit errorOccurred(QStringLiteral("控制 API 启动失败：%1").arg(m_error));
        return false;
    }

    m_port = port;
    emit listeningChanged(true);
    return true;
}

void ControlApiServer::stop()
{
    const bool wasListening = m_server.isListening();
    m_server.close();
    for (QTcpSocket *socket : m_buffers.keys())
        socket->disconnectFromHost();
    m_buffers.clear();
    if (wasListening)
        emit listeningChanged(false);
}

void ControlApiServer::applyConfig(bool enabled, quint16 port)
{
    if (!enabled) {
        stop();
        return;
    }
    if (m_server.isListening() && m_port == port)
        return;
    start(port);
}

void ControlApiServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { processSocket(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ControlApiServer::processSocket(QTcpSocket *socket)
{
    auto it = m_buffers.find(socket);
    if (it == m_buffers.end()) {
        socket->readAll();
        return;
    }

    it.value().append(socket->readAll());
    QByteArray &buffer = it.value();

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (buffer.size() > 64 * 1024)
            socket->abort();
        return;
    }

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QList<QByteArray> headerLines = headerBlock.split('\n');
    if (headerLines.isEmpty()) {
        socket->abort();
        return;
    }

    const QList<QByteArray> requestLine = headerLines.first().trimmed().split(' ');
    if (requestLine.size() < 2) {
        socket->abort();
        return;
    }

    const QByteArray method = requestLine.at(0).toUpper();
    const QString target = QString::fromUtf8(requestLine.at(1));
    const QString path = QUrl(target).path();

    int contentLength = 0;
    for (int i = 1; i < headerLines.size(); ++i) {
        const QByteArray line = headerLines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        if (line.left(colon).trimmed().toLower() == "content-length") {
            contentLength = line.mid(colon + 1).trimmed().toInt();
            break;
        }
    }
    contentLength = qBound(0, contentLength, 1024 * 1024);

    if (buffer.size() < headerEnd + 4 + contentLength)
        return;

    const QByteArray body = buffer.mid(headerEnd + 4, contentLength);
    m_buffers.remove(socket);

    handleRequest(socket, method, path, body);
}

void ControlApiServer::handleRequest(QTcpSocket *socket, const QByteArray &method,
                                     const QString &path, const QByteArray &body)
{
    if (method == "OPTIONS") {
        respondText(socket, 204, "text/plain", QByteArray());
        return;
    }

    if (method == "GET" && path == QLatin1String("/api/health")) {
        QJsonObject obj;
        obj[QStringLiteral("ok")] = true;
        obj[QStringLiteral("app")] = QStringLiteral("LlamaDeck");
        obj[QStringLiteral("time")] = QDateTime::currentDateTime().toString(Qt::ISODate);
        respondJson(socket, 200, obj);
        return;
    }

    if (method == "GET" && path == QLatin1String("/api/status")) {
        const ServerStats &stats = m_controller->stats();
        const LaunchOptions &options = m_controller->options();
        QJsonObject obj;
        obj[QStringLiteral("state")] = stateSlug(m_controller->state());
        obj[QStringLiteral("stateText")] = serverStateText(m_controller->state());
        obj[QStringLiteral("running")] = m_controller->state() == ServerState::Running;
        obj[QStringLiteral("model")] = QFileInfo(options.modelPath).completeBaseName();
        obj[QStringLiteral("modelPath")] = options.modelPath;
        obj[QStringLiteral("port")] = options.port;
        obj[QStringLiteral("command")] = m_controller->currentCommand();
        obj[QStringLiteral("genTps")] = stats.genTps;
        obj[QStringLiteral("promptTps")] = stats.promptTps;
        obj[QStringLiteral("peakTps")] = stats.peakGenTps;
        obj[QStringLiteral("avgTps")] = m_controller->averageGenTps();
        obj[QStringLiteral("tokensGenerated")] = static_cast<double>(stats.genTokens);
        obj[QStringLiteral("tokensPrompt")] = static_cast<double>(stats.promptTokens);
        obj[QStringLiteral("pid")] = static_cast<double>(stats.pid);
        obj[QStringLiteral("activeRequests")] = stats.activeRequests;
        obj[QStringLiteral("uptimeSeconds")] = stats.startedAt.isValid()
            ? double(stats.startedAt.secsTo(QDateTime::currentDateTime()))
            : 0.0;
        obj[QStringLiteral("controlApiPort")] = m_server.isListening() ? m_port : 0;
        obj[QStringLiteral("lastError")] = m_controller->lastError();
        respondJson(socket, 200, obj);
        return;
    }

    if (method == "GET" && path == QLatin1String("/api/models")) {
        const QVector<ModelInfo> models = ModelStore::scan(m_settings->modelsDir);
        QJsonArray array;
        for (const ModelInfo &model : models) {
            QJsonObject obj;
            obj[QStringLiteral("name")] = model.name;
            obj[QStringLiteral("file")] = model.fileName;
            obj[QStringLiteral("path")] = model.path;
            obj[QStringLiteral("architecture")] = model.architecture;
            obj[QStringLiteral("quantization")] = model.quantization;
            obj[QStringLiteral("parameterCount")] = static_cast<double>(model.parameterCount);
            obj[QStringLiteral("parameters")] = ModelStore::formatParams(model.parameterCount);
            obj[QStringLiteral("contextLength")] = static_cast<double>(model.contextLength);
            obj[QStringLiteral("sizeBytes")] = static_cast<double>(model.sizeBytes);
            obj[QStringLiteral("size")] = ModelStore::formatSize(model.sizeBytes);
            array.append(obj);
        }
        QJsonObject obj;
        obj[QStringLiteral("directory")] = m_settings->modelsDir;
        obj[QStringLiteral("count")] = array.size();
        obj[QStringLiteral("models")] = array;
        respondJson(socket, 200, obj);
        return;
    }

    if (method == "GET" && path == QLatin1String("/api/history")) {
        QJsonArray array;
        for (const HistoryEntry &entry : m_history->entries()) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = entry.id;
            obj[QStringLiteral("startedAt")] = entry.startedAt.toString(Qt::ISODateWithMs);
            obj[QStringLiteral("endedAt")] = entry.endedAt.isValid()
                ? entry.endedAt.toString(Qt::ISODateWithMs) : QString();
            obj[QStringLiteral("model")] = entry.modelName;
            obj[QStringLiteral("modelPath")] = entry.modelPath;
            obj[QStringLiteral("port")] = entry.options.port;
            obj[QStringLiteral("status")] = humanStatus(entry.status);
            obj[QStringLiteral("durationSeconds")] = static_cast<double>(entry.durationSec);
            obj[QStringLiteral("avgTps")] = entry.avgTps;
            obj[QStringLiteral("peakTps")] = entry.peakTps;
            obj[QStringLiteral("tokensGenerated")] = static_cast<double>(entry.tokens);
            obj[QStringLiteral("command")] = entry.command;
            array.append(obj);
        }
        QJsonObject obj;
        obj[QStringLiteral("count")] = array.size();
        obj[QStringLiteral("entries")] = array;
        respondJson(socket, 200, obj);
        return;
    }

    if (method == "POST" && path == QLatin1String("/api/launch")) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            respondJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("请求体必须是 JSON 对象。")}});
            return;
        }

        const QJsonObject obj = doc.object();
        const QString wanted = obj.value(QStringLiteral("model")).toString().trimmed();
        if (wanted.isEmpty()) {
            respondJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("缺少 model 字段。")}});
            return;
        }

        if (m_controller->isRunning()) {
            respondJson(socket, 409, QJsonObject{{QStringLiteral("error"), QStringLiteral("推理服务已在运行。")}});
            return;
        }

        QString modelPath;
        QString modelName;
        const QVector<ModelInfo> models = ModelStore::scan(m_settings->modelsDir);
        for (const ModelInfo &model : models) {
            const bool matches = model.path.compare(wanted, Qt::CaseInsensitive) == 0
                || model.fileName.compare(wanted, Qt::CaseInsensitive) == 0
                || model.name.compare(wanted, Qt::CaseInsensitive) == 0
                || model.path.endsWith(QLatin1Char('/') + wanted, Qt::CaseInsensitive)
                || model.fileName.startsWith(wanted, Qt::CaseInsensitive);
            if (matches) {
                modelPath = model.path;
                modelName = model.name;
                break;
            }
        }

        if (modelPath.isEmpty()) {
            const QFileInfo direct(wanted);
            if (direct.exists() && direct.suffix().compare(QLatin1String("gguf"), Qt::CaseInsensitive) == 0) {
                modelPath = QDir::toNativeSeparators(direct.absoluteFilePath());
                modelName = direct.completeBaseName();
            }
        }

        if (modelPath.isEmpty()) {
            respondJson(socket, 404, QJsonObject{{QStringLiteral("error"),
                                                  QStringLiteral("未找到匹配的模型：%1").arg(wanted)}});
            return;
        }

        LaunchOptions options;
        options.modelPath = modelPath;
        options.port = obj.value(QStringLiteral("port")).toInt(m_settings->port);
        options.ctx = obj.value(QStringLiteral("ctx")).toInt(m_settings->ctx);
        options.gpuLayers = obj.value(QStringLiteral("ngl")).toInt(m_settings->gpuLayers);
        options.threads = obj.value(QStringLiteral("threads")).toInt(m_settings->threads);
        options.batch = obj.value(QStringLiteral("batch")).toInt(m_settings->batch);
        options.ubatch = obj.value(QStringLiteral("ubatch")).toInt(m_settings->ubatch);
        options.parallel = obj.value(QStringLiteral("parallel")).toInt(m_settings->parallel);
        options.kvCacheType = obj.value(QStringLiteral("kvCacheType"))
                                  .toString(m_settings->kvCacheType);
        options.mlock = obj.value(QStringLiteral("mlock")).toBool(m_settings->mlock);
        options.noMmap = obj.value(QStringLiteral("noMmap")).toBool(m_settings->noMmap);
        options.flashAttn = obj.value(QStringLiteral("flashAttn")).toBool(m_settings->flashAttn);
        options.jinja = obj.value(QStringLiteral("jinja")).toBool(m_settings->jinja);
        options.apiKey = obj.value(QStringLiteral("apiKey")).toString(m_settings->apiKey);
        options.extraArgs = obj.value(QStringLiteral("extraArgs")).toString(m_settings->extraArgs);

        if (options.port < 1 || options.port > 65535) {
            respondJson(socket, 400, QJsonObject{{QStringLiteral("error"), QStringLiteral("port 超出范围。")}});
            return;
        }

        if (!m_controller->start(options)) {
            respondJson(socket, 500, QJsonObject{{QStringLiteral("error"), m_controller->lastError()}});
            return;
        }

        QJsonObject result;
        result[QStringLiteral("ok")] = true;
        result[QStringLiteral("model")] = modelName;
        result[QStringLiteral("modelPath")] = modelPath;
        result[QStringLiteral("port")] = options.port;
        result[QStringLiteral("command")] = m_controller->currentCommand();
        respondJson(socket, 200, result);
        return;
    }

    if (method == "POST" && path == QLatin1String("/api/stop")) {
        m_controller->stop();
        QJsonObject result;
        result[QStringLiteral("ok")] = true;
        result[QStringLiteral("state")] = QStringLiteral("stopping");
        respondJson(socket, 200, result);
        return;
    }

    respondJson(socket, 404, QJsonObject{{QStringLiteral("error"), QStringLiteral("未知接口：%1").arg(path)}});
}

void ControlApiServer::respondJson(QTcpSocket *socket, int status, const QJsonObject &object) const
{
    respondText(socket, status, "application/json; charset=utf-8",
                QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void ControlApiServer::respondText(QTcpSocket *socket, int status, const QByteArray &contentType,
                                   const QByteArray &payload) const
{
    QByteArray headers;
    headers += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status) + "\r\n";
    headers += "Content-Type: " + contentType + "\r\n";
    headers += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
    headers += "Access-Control-Allow-Origin: *\r\n";
    headers += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    headers += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    headers += "Connection: close\r\n\r\n";

    socket->write(headers + payload);
    socket->flush();
    socket->disconnectFromHost();
}
