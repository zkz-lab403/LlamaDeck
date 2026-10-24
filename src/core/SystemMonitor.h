#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

struct SystemSample {
    double cpuPercent = 0.0;
    double memUsedMb = 0.0;
    double memTotalMb = 0.0;
    bool gpuAvailable = false;
    QString gpuName;
    double gpuPercent = 0.0;
    double vramUsedMb = 0.0;
    double vramTotalMb = 0.0;
    int gpuTempC = -1;
};

struct HardwareInfo {
    QString gpuName;
    bool nvidia = false;
    double vramTotalMb = 0.0;
    double ramTotalMb = 0.0;
};

class SystemMonitor : public QObject {
    Q_OBJECT

public:
    explicit SystemMonitor(QObject *parent = nullptr);
    ~SystemMonitor() override;

    static HardwareInfo queryHardware();

    void start(int intervalMs = 2000);
    void stop();
    const SystemSample &latest() const { return m_latest; }

signals:
    void sampled(const SystemSample &sample);

private:
    void sample();

    QTimer m_timer;
    SystemSample m_latest;
    quint64 m_lastIdle = 0;
    quint64 m_lastKernel = 0;
    quint64 m_lastUser = 0;
    bool m_hasCpuBaseline = false;
};
