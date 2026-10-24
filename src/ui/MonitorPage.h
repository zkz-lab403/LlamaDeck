#pragma once

#include "SystemMonitor.h"

#include <QWidget>

class ServerController;
class StatCard;
class SpeedChart;
class ResourceMeter;
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
    void updateResources(const SystemSample &sample);
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
    SystemMonitor *m_system = nullptr;
    QLabel *m_resourceHint = nullptr;
    ResourceMeter *m_cpuMeter = nullptr;
    ResourceMeter *m_memMeter = nullptr;
    ResourceMeter *m_gpuMeter = nullptr;
    ResourceMeter *m_vramMeter = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_logMeta = nullptr;
    ToggleSwitch *m_autoScroll = nullptr;
    QTimer *m_sampleTimer = nullptr;
    QTimer *m_uptimeTimer = nullptr;
};
