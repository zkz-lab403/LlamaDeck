#include "SystemMonitor.h"

#include <QLibrary>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN

using NvmlDevice = void *;

struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct NvmlApi {
    QLibrary library;
    NvmlDevice device = nullptr;
    QString name;
    int (*init)() = nullptr;
    int (*getHandle)(unsigned int, NvmlDevice *) = nullptr;
    int (*getUtilization)(NvmlDevice, NvmlUtilization *) = nullptr;
    int (*getMemory)(NvmlDevice, NvmlMemory *) = nullptr;
    int (*getTemperature)(NvmlDevice, int, unsigned int *) = nullptr;
    int (*getName)(NvmlDevice, char *, unsigned int) = nullptr;
    bool ready = false;
};

NvmlApi &nvmlApi()
{
    static NvmlApi api;
    static bool tried = false;
    if (tried)
        return api;
    tried = true;

    api.library.setFileName(QStringLiteral("nvml"));
    if (!api.library.load())
        return api;

    api.init = reinterpret_cast<int (*)()>(api.library.resolve("nvmlInit_v2"));
    api.getHandle = reinterpret_cast<int (*)(unsigned int, NvmlDevice *)>(
        api.library.resolve("nvmlDeviceGetHandleByIndex_v2"));
    api.getUtilization = reinterpret_cast<int (*)(NvmlDevice, NvmlUtilization *)>(
        api.library.resolve("nvmlDeviceGetUtilizationRates"));
    api.getMemory = reinterpret_cast<int (*)(NvmlDevice, NvmlMemory *)>(
        api.library.resolve("nvmlDeviceGetMemoryInfo"));
    api.getTemperature = reinterpret_cast<int (*)(NvmlDevice, int, unsigned int *)>(
        api.library.resolve("nvmlDeviceGetTemperature"));
    api.getName = reinterpret_cast<int (*)(NvmlDevice, char *, unsigned int)>(
        api.library.resolve("nvmlDeviceGetName"));

    if (!api.init || !api.getHandle || !api.getUtilization || !api.getMemory)
        return api;
    if (api.init() != 0)
        return api;
    if (api.getHandle(0, &api.device) != 0) {
        api.device = nullptr;
        return api;
    }

    char nameBuffer[96] = {0};
    if (api.getName && api.getName(api.device, nameBuffer, sizeof(nameBuffer)) == 0)
        api.name = QString::fromUtf8(nameBuffer);

    api.ready = true;
    return api;
}

quint64 fileTimeValue(const FILETIME &time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return quint64(value.QuadPart);
}

#endif // Q_OS_WIN

} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::sample);
}

SystemMonitor::~SystemMonitor() = default;

void SystemMonitor::start(int intervalMs)
{
    m_timer.setInterval(qMax(500, intervalMs));
    m_timer.start();
    sample();
}

void SystemMonitor::stop()
{
    m_timer.stop();
}

void SystemMonitor::sample()
{
    SystemSample result;

#ifdef Q_OS_WIN
    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        const quint64 idle = fileTimeValue(idleTime);
        const quint64 kernel = fileTimeValue(kernelTime);
        const quint64 user = fileTimeValue(userTime);

        if (m_hasCpuBaseline) {
            const quint64 idleDelta = idle - m_lastIdle;
            const quint64 totalDelta = (kernel - m_lastKernel) + (user - m_lastUser);
            if (totalDelta > 0) {
                const double busy = double(totalDelta - qMin(idleDelta, totalDelta));
                result.cpuPercent = qBound(0.0, busy * 100.0 / double(totalDelta), 100.0);
            }
        }

        m_lastIdle = idle;
        m_lastKernel = kernel;
        m_lastUser = user;
        m_hasCpuBaseline = true;
    }

    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        result.memTotalMb = double(memoryStatus.ullTotalPhys) / 1048576.0;
        result.memUsedMb = double(memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) / 1048576.0;
    }

    const NvmlApi &api = nvmlApi();
    if (api.ready) {
        result.gpuAvailable = true;
        result.gpuName = api.name;

        NvmlUtilization utilization{};
        if (api.getUtilization(api.device, &utilization) == 0)
            result.gpuPercent = qBound(0.0, double(utilization.gpu), 100.0);

        NvmlMemory gpuMemory{};
        if (api.getMemory(api.device, &gpuMemory) == 0) {
            result.vramTotalMb = double(gpuMemory.total) / 1048576.0;
            result.vramUsedMb = double(gpuMemory.used) / 1048576.0;
        }

        unsigned int temperature = 0;
        if (api.getTemperature && api.getTemperature(api.device, 0, &temperature) == 0)
            result.gpuTempC = int(temperature);
    }
#endif

    m_latest = result;
    emit sampled(result);
}

HardwareInfo SystemMonitor::queryHardware()
{
    HardwareInfo info;

#ifdef Q_OS_WIN
    MEMORYSTATUSEX memoryStatus{};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus))
        info.ramTotalMb = double(memoryStatus.ullTotalPhys) / 1048576.0;

    const NvmlApi &api = nvmlApi();
    if (api.ready) {
        info.nvidia = true;
        info.gpuName = api.name;
        NvmlMemory gpuMemory{};
        if (api.getMemory(api.device, &gpuMemory) == 0)
            info.vramTotalMb = double(gpuMemory.total) / 1048576.0;
    }
#endif

    return info;
}
