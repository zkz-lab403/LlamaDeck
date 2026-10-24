#pragma once

#include <QWidget>

class AppSettings;
class ServerController;
class QLabel;
class QPlainTextEdit;
class QVBoxLayout;

class ApiDocsPage : public QWidget {
    Q_OBJECT

public:
    ApiDocsPage(AppSettings *settings, ServerController *controller, QWidget *parent = nullptr);

signals:
    void toastRequested(const QString &message);

private:
    void buildUi();
    void addEndpointRow(QVBoxLayout *layout, const QString &method, const QString &path,
                        const QString &description, bool controlApi);
    void updateEndpoints();
    void copyText(const QString &text, const QString &what);
    QString serverBaseUrl() const;
    QString controlBaseUrl() const;

    AppSettings *m_settings = nullptr;
    ServerController *m_controller = nullptr;

    QLabel *m_serverBaseLabel = nullptr;
    QLabel *m_controlBaseLabel = nullptr;
    QPlainTextEdit *m_curlView = nullptr;
};
