#pragma once

#include <QWidget>

class AppSettings;
class QComboBox;
class QLineEdit;
class QSpinBox;
class ToggleSwitch;

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(AppSettings *settings, QWidget *parent = nullptr);

    void refresh();

signals:
    void settingsSaved();
    void toastRequested(const QString &message);

private:
    void buildUi();
    void save();
    void browseServerPath();
    void browseModelsDir();
    void detectServer();

    AppSettings *m_settings = nullptr;
    QLineEdit *m_serverPath = nullptr;
    QLineEdit *m_modelsDir = nullptr;
    QSpinBox *m_port = nullptr;
    QSpinBox *m_ctx = nullptr;
    QSpinBox *m_gpuLayers = nullptr;
    QSpinBox *m_threads = nullptr;
    QSpinBox *m_batch = nullptr;
    QSpinBox *m_ubatch = nullptr;
    QSpinBox *m_parallel = nullptr;
    QComboBox *m_kvCache = nullptr;
    ToggleSwitch *m_mlock = nullptr;
    ToggleSwitch *m_noMmap = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_extraArgs = nullptr;
    ToggleSwitch *m_flashAttn = nullptr;
    ToggleSwitch *m_jinja = nullptr;
    ToggleSwitch *m_autoStartLast = nullptr;
    ToggleSwitch *m_autoOpenApi = nullptr;
    ToggleSwitch *m_autoOpenBrowser = nullptr;
    ToggleSwitch *m_stopOnExit = nullptr;
    ToggleSwitch *m_controlApiEnabled = nullptr;
    QSpinBox *m_controlApiPort = nullptr;
};
