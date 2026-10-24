#pragma once

#include <QWidget>

class ServerController;
class StatCard;
class SpeedChart;
class QPlainTextEdit;
class QLabel;
class ToggleSwitch;
class QTimer;

class MonitorPage : public QWidget {
    Q_OBJECT

public:
    MonitorPage(ServerController *controller, QWidget *parent = nullptr);

signals:
    void toastRequested(const QString &message);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    void updateStats();
    void updateUptime();
    void updateState();
    void appendLog(const QString &line);
    void clearLog();
    void copyLog();

    ServerController *m_controller = nullptr;
    StatCard *m_stateCard = nullptr;
    StatCard *m_genCard = nullptr;
    StatCard *m_promptCard = nullptr;
    StatCard *m_uptimeCard = nullptr;
    SpeedChart *m_chart = nullptr;
    QLabel *m_chartBadge = nullptr;
    QLabel *m_peakLabel = nullptr;
    QLabel *m_tokensLabel = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_logMeta = nullptr;
    ToggleSwitch *m_autoScroll = nullptr;
    QTimer *m_sampleTimer = nullptr;
    QTimer *m_uptimeTimer = nullptr;
};
