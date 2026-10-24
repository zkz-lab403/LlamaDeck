#pragma once

#include "Types.h"

#include <QWidget>

class AppSettings;
class ServerController;
class QLabel;
class QLineEdit;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QFrame;
class ToggleSwitch;
class Badge;

class ModelPage : public QWidget {
    Q_OBJECT

public:
    ModelPage(AppSettings *settings, ServerController *controller, QWidget *parent = nullptr);

    void refreshModels();
    bool selectModelByPath(const QString &path);
    void applyOptions(const LaunchOptions &options);
    void syncDefaults();
    LaunchOptions currentOptions() const;
    QString selectedModelName() const;

signals:
    void startRequested();
    void stopRequested();
    void openSettingsRequested();
    void toastRequested(const QString &message);

private:
    void buildUi();
    QWidget *buildEmptyState();
    void filterModels(const QString &query);
    void updateSelection();
    void updateCommandPreview();
    void updateServerHint();
    void updateCountLabel();
    void setConfigEnabled(bool enabled);
    void applyRecommendedPerformance();
    QListWidgetItem *itemForPath(const QString &path) const;
    bool hasSelection() const;

    AppSettings *m_settings = nullptr;
    ServerController *m_controller = nullptr;

    QLineEdit *m_search = nullptr;
    QLabel *m_countLabel = nullptr;
    QListWidget *m_list = nullptr;
    QStackedWidget *m_listStack = nullptr;
    QFrame *m_banner = nullptr;

    QLabel *m_modelName = nullptr;
    QLabel *m_modelMeta = nullptr;
    Badge *m_quantBadge = nullptr;

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
    QPushButton *m_recommendButton = nullptr;
    ToggleSwitch *m_flashAttn = nullptr;
    ToggleSwitch *m_jinja = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_extraArgs = nullptr;
    QPlainTextEdit *m_commandView = nullptr;
    QLabel *m_endpoint = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;

    int m_totalModels = 0;
};
