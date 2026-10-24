#include "SettingsPage.h"

#include "AppSettings.h"
#include "Icons.h"
#include "Theme.h"
#include "Widgets.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QWidget *makeField(const QString &label, QWidget *input, const QString &hint = QString())
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(makeLabel(label, QStringLiteral("FieldLabel"), widget));
    layout->addWidget(input);
    if (!hint.isEmpty()) {
        auto *hintLabel = makeLabel(hint, QStringLiteral("Hint"), widget);
        hintLabel->setWordWrap(true);
        layout->addWidget(hintLabel);
    }
    return widget;
}

QWidget *makeToggleRow(const QString &title, const QString &hint, ToggleSwitch *toggle,
                       QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(14);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(1);
    textLayout->addWidget(makeLabel(title, QStringLiteral("Muted"), widget));
    auto *hintLabel = makeLabel(hint, QStringLiteral("Hint"), widget);
    hintLabel->setWordWrap(true);
    textLayout->addWidget(hintLabel);

    layout->addLayout(textLayout, 1);
    layout->addWidget(toggle, 0, Qt::AlignTop);
    return widget;
}

QSpinBox *makeSpin(QWidget *parent, int minimum, int maximum, int value, int step = 1)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setValue(value);
    spin->setMinimumHeight(34);
    return spin;
}

} // namespace

SettingsPage::SettingsPage(AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    buildUi();
    refresh();
}

void SettingsPage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 8, 28, 22);
    outer->setSpacing(14);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(14);

    auto *runtimeCard = makeCard(content);
    auto *runtime = new QVBoxLayout(runtimeCard);
    runtime->setContentsMargins(20, 18, 20, 18);
    runtime->setSpacing(10);
    runtime->addWidget(makeLabel(QStringLiteral("运行时路径"), QStringLiteral("SectionTitle"),
                                 runtimeCard));
    runtime->addWidget(makeLabel(QStringLiteral("llama.cpp 可执行文件与 GGUF 模型所在目录"),
                                 QStringLiteral("Hint"), runtimeCard));
    runtime->addSpacing(4);

    m_serverPath = new QLineEdit(runtimeCard);
    m_serverPath->setPlaceholderText(QStringLiteral("例如 C:/llama.cpp/llama-server.exe"));
    m_serverPath->setMinimumHeight(34);
    auto *serverBrowse = new QPushButton(QStringLiteral("浏览"), runtimeCard);
    serverBrowse->setObjectName(QStringLiteral("Ghost"));
    serverBrowse->setCursor(Qt::PointingHandCursor);
    auto *serverDetect = new QPushButton(QStringLiteral("自动检测"), runtimeCard);
    serverDetect->setObjectName(QStringLiteral("Ghost"));
    serverDetect->setCursor(Qt::PointingHandCursor);
    connect(serverBrowse, &QPushButton::clicked, this, &SettingsPage::browseServerPath);
    connect(serverDetect, &QPushButton::clicked, this, &SettingsPage::detectServer);

    auto *serverRow = new QHBoxLayout();
    serverRow->setSpacing(8);
    serverRow->addWidget(m_serverPath, 1);
    serverRow->addWidget(serverBrowse);
    serverRow->addWidget(serverDetect);

    auto *serverField = new QWidget(runtimeCard);
    auto *serverFieldLayout = new QVBoxLayout(serverField);
    serverFieldLayout->setContentsMargins(0, 0, 0, 0);
    serverFieldLayout->setSpacing(5);
    serverFieldLayout->addWidget(makeLabel(QStringLiteral("llama-server 路径"),
                                            QStringLiteral("FieldLabel"), serverField));
    serverFieldLayout->addLayout(serverRow);
    serverFieldLayout->addWidget(makeLabel(QStringLiteral("启动服务时将调用该可执行文件"),
                                            QStringLiteral("Hint"), serverField));
    runtime->addWidget(serverField);

    m_modelsDir = new QLineEdit(runtimeCard);
    m_modelsDir->setPlaceholderText(QStringLiteral("存放 .gguf 文件的目录"));
    m_modelsDir->setMinimumHeight(34);
    auto *modelsBrowse = new QPushButton(QStringLiteral("浏览"), runtimeCard);
    modelsBrowse->setObjectName(QStringLiteral("Ghost"));
    modelsBrowse->setCursor(Qt::PointingHandCursor);
    connect(modelsBrowse, &QPushButton::clicked, this, &SettingsPage::browseModelsDir);

    auto *modelsRow = new QHBoxLayout();
    modelsRow->setSpacing(8);
    modelsRow->addWidget(m_modelsDir, 1);
    modelsRow->addWidget(modelsBrowse);

    auto *modelsField = new QWidget(runtimeCard);
    auto *modelsFieldLayout = new QVBoxLayout(modelsField);
    modelsFieldLayout->setContentsMargins(0, 0, 0, 0);
    modelsFieldLayout->setSpacing(5);
    modelsFieldLayout->addWidget(makeLabel(QStringLiteral("模型目录"),
                                            QStringLiteral("FieldLabel"), modelsField));
    modelsFieldLayout->addLayout(modelsRow);
    modelsFieldLayout->addWidget(makeLabel(QStringLiteral("会递归扫描该目录下的所有 .gguf 文件"),
                                            QStringLiteral("Hint"), modelsField));
    runtime->addWidget(modelsField);

    layout->addWidget(runtimeCard);

    auto *defaultsCard = makeCard(content);
    auto *defaults = new QVBoxLayout(defaultsCard);
    defaults->setContentsMargins(20, 18, 20, 18);
    defaults->setSpacing(10);
    defaults->addWidget(makeLabel(QStringLiteral("默认启动参数"), QStringLiteral("SectionTitle"),
                                  defaultsCard));
    defaults->addWidget(makeLabel(QStringLiteral("新建启动配置时使用的默认值，仍可在模型页单独调整"),
                                  QStringLiteral("Hint"), defaultsCard));
    defaults->addSpacing(4);

    m_port = makeSpin(defaultsCard, 1024, 65535, m_settings->port);
    m_ctx = makeSpin(defaultsCard, 256, 1048576, m_settings->ctx, 512);
    m_gpuLayers = makeSpin(defaultsCard, -1, 999, m_settings->gpuLayers);
    m_gpuLayers->setSpecialValueText(QStringLiteral("自动"));
    m_threads = makeSpin(defaultsCard, 0, 256, m_settings->threads);
    m_threads->setSpecialValueText(QStringLiteral("自动"));

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->addWidget(makeField(QStringLiteral("默认端口"), m_port), 0, 0);
    grid->addWidget(makeField(QStringLiteral("默认上下文 (-c)"), m_ctx), 0, 1);
    grid->addWidget(makeField(QStringLiteral("默认 GPU 层数 (-ngl)"), m_gpuLayers), 1, 0);
    grid->addWidget(makeField(QStringLiteral("默认线程数 (-t)"), m_threads), 1, 1);
    defaults->addLayout(grid);

    m_flashAttn = new ToggleSwitch(defaultsCard);
    m_jinja = new ToggleSwitch(defaultsCard);
    defaults->addWidget(makeToggleRow(QStringLiteral("Flash Attention"),
                                      QStringLiteral("默认启用加速注意力计算"), m_flashAttn,
                                      defaultsCard));
    defaults->addWidget(makeToggleRow(QStringLiteral("Jinja 聊天模板"),
                                      QStringLiteral("默认启用聊天模板与工具调用支持"), m_jinja,
                                      defaultsCard));

    m_apiKey = new QLineEdit(defaultsCard);
    m_apiKey->setPlaceholderText(QStringLiteral("可选，留空表示无需鉴权"));
    m_apiKey->setMinimumHeight(34);
    defaults->addWidget(makeField(QStringLiteral("默认 API Key"), m_apiKey));

    m_extraArgs = new QLineEdit(defaultsCard);
    m_extraArgs->setPlaceholderText(QStringLiteral("例如：--no-mmap --mlock --parallel 2"));
    m_extraArgs->setMinimumHeight(34);
    defaults->addWidget(makeField(QStringLiteral("默认附加参数"), m_extraArgs));
    layout->addWidget(defaultsCard);

    auto *startupCard = makeCard(content);
    auto *startup = new QVBoxLayout(startupCard);
    startup->setContentsMargins(20, 18, 20, 18);
    startup->setSpacing(6);
    startup->addWidget(makeLabel(QStringLiteral("启动与退出"), QStringLiteral("SectionTitle"),
                                 startupCard));
    startup->addWidget(makeLabel(QStringLiteral("控制 LlamaDeck 自身的行为"),
                                 QStringLiteral("Hint"), startupCard));

    m_autoStartLast = new ToggleSwitch(startupCard);
    m_autoOpenApi = new ToggleSwitch(startupCard);
    m_autoOpenBrowser = new ToggleSwitch(startupCard);
    m_stopOnExit = new ToggleSwitch(startupCard);
    startup->addWidget(makeToggleRow(QStringLiteral("自动启动上次模型"),
                                     QStringLiteral("打开应用后自动启动最近使用的模型"),
                                     m_autoStartLast, startupCard));
    startup->addWidget(makeToggleRow(QStringLiteral("启动后打开 API 调试"),
                                     QStringLiteral("推理服务就绪后自动切换到接口调试页"),
                                     m_autoOpenApi, startupCard));
    startup->addWidget(makeToggleRow(QStringLiteral("启动后打开 Web UI"),
                                     QStringLiteral("推理服务就绪后在浏览器中打开服务页面"),
                                     m_autoOpenBrowser, startupCard));
    startup->addWidget(makeToggleRow(QStringLiteral("关闭窗口时停止服务"),
                                     QStringLiteral("关闭 LlamaDeck 时同时结束后台 llama-server"),
                                     m_stopOnExit, startupCard));
    layout->addWidget(startupCard);

    auto *controlCard = makeCard(content);
    auto *control = new QVBoxLayout(controlCard);
    control->setContentsMargins(20, 18, 20, 18);
    control->setSpacing(6);
    control->addWidget(makeLabel(QStringLiteral("GUI 控制 API"), QStringLiteral("SectionTitle"),
                                 controlCard));
    control->addWidget(makeLabel(QStringLiteral("通过 HTTP 接口查询状态、启动或停止模型"),
                                 QStringLiteral("Hint"), controlCard));

    m_controlApiEnabled = new ToggleSwitch(controlCard);
    control->addWidget(makeToggleRow(QStringLiteral("启用控制 API"),
                                      QStringLiteral("仅绑定 127.0.0.1，不对外网暴露"),
                                      m_controlApiEnabled, controlCard));

    m_controlApiPort = makeSpin(controlCard, 1024, 65535, m_settings->controlApiPort);
    auto *controlPortRow = new QHBoxLayout();
    controlPortRow->setSpacing(10);
    auto *controlPortField = makeField(QStringLiteral("监听端口"), m_controlApiPort);
    controlPortField->setMaximumWidth(240);
    controlPortRow->addWidget(controlPortField);
    controlPortRow->addStretch();
    control->addLayout(controlPortRow);
    control->addWidget(makeLabel(QStringLiteral("接口清单与调试入口见“API 接口”页面"),
                                  QStringLiteral("Hint"), controlCard));
    layout->addWidget(controlCard);

    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout();
    footer->setSpacing(10);
    footer->addWidget(makeLabel(QStringLiteral("修改后点击保存生效"), QStringLiteral("Hint"), this));
    footer->addStretch();
    auto *saveButton = new QPushButton(QStringLiteral("保存并应用"), this);
    saveButton->setObjectName(QStringLiteral("Primary"));
    saveButton->setMinimumSize(140, 40);
    saveButton->setCursor(Qt::PointingHandCursor);
    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    footer->addWidget(saveButton);
    outer->addLayout(footer);
}

void SettingsPage::refresh()
{
    m_serverPath->setText(m_settings->llamaServerPath);
    m_modelsDir->setText(m_settings->modelsDir);
    m_port->setValue(m_settings->port);
    m_ctx->setValue(m_settings->ctx);
    m_gpuLayers->setValue(m_settings->gpuLayers);
    m_threads->setValue(m_settings->threads);
    m_apiKey->setText(m_settings->apiKey);
    m_extraArgs->setText(m_settings->extraArgs);
    m_flashAttn->setChecked(m_settings->flashAttn);
    m_jinja->setChecked(m_settings->jinja);
    m_autoStartLast->setChecked(m_settings->autoStartLast);
    m_autoOpenApi->setChecked(m_settings->autoOpenApiTab);
    m_autoOpenBrowser->setChecked(m_settings->autoOpenBrowser);
    m_stopOnExit->setChecked(m_settings->stopOnExit);
    m_controlApiEnabled->setChecked(m_settings->controlApiEnabled);
    m_controlApiPort->setValue(m_settings->controlApiPort);
}

void SettingsPage::save()
{
    const QString serverPath = QDir::toNativeSeparators(m_serverPath->text().trimmed());
    if (!serverPath.isEmpty() && !QFileInfo::exists(serverPath)) {
        QMessageBox::warning(this, QStringLiteral("路径无效"),
                             QStringLiteral("指定的 llama-server 文件不存在：\n%1").arg(serverPath));
        return;
    }

    const QString modelsDir = QDir::toNativeSeparators(m_modelsDir->text().trimmed());
    if (!modelsDir.isEmpty() && !QDir(modelsDir).exists()) {
        QMessageBox::warning(this, QStringLiteral("目录无效"),
                             QStringLiteral("模型目录不存在：\n%1").arg(modelsDir));
        return;
    }

    if (m_controlApiEnabled->isChecked() && m_controlApiPort->value() == m_port->value()) {
        QMessageBox::warning(this, QStringLiteral("端口冲突"),
                             QStringLiteral("控制 API 端口不能与推理服务端口相同，请修改后再保存。"));
        return;
    }

    m_settings->llamaServerPath = serverPath;
    m_settings->modelsDir = modelsDir;
    m_settings->port = m_port->value();
    m_settings->ctx = m_ctx->value();
    m_settings->gpuLayers = m_gpuLayers->value();
    m_settings->threads = m_threads->value();
    m_settings->apiKey = m_apiKey->text().trimmed();
    m_settings->extraArgs = m_extraArgs->text().trimmed();
    m_settings->flashAttn = m_flashAttn->isChecked();
    m_settings->jinja = m_jinja->isChecked();
    m_settings->autoStartLast = m_autoStartLast->isChecked();
    m_settings->autoOpenApiTab = m_autoOpenApi->isChecked();
    m_settings->autoOpenBrowser = m_autoOpenBrowser->isChecked();
    m_settings->stopOnExit = m_stopOnExit->isChecked();
    m_settings->controlApiEnabled = m_controlApiEnabled->isChecked();
    m_settings->controlApiPort = m_controlApiPort->value();

    m_settings->save();
    emit settingsSaved();
    emit toastRequested(QStringLiteral("设置已保存并应用。"));
}

void SettingsPage::browseServerPath()
{
    const QString current = m_serverPath->text().trimmed();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 llama-server 可执行文件"),
        current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath(),
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*)"));
    if (!path.isEmpty())
        m_serverPath->setText(QDir::toNativeSeparators(path));
}

void SettingsPage::browseModelsDir()
{
    const QString current = m_modelsDir->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择模型目录"),
        current.isEmpty() ? QDir::homePath() : current);
    if (!dir.isEmpty())
        m_modelsDir->setText(QDir::toNativeSeparators(dir));
}

void SettingsPage::detectServer()
{
    const QString path = m_settings->detectServerPath();
    if (path.isEmpty()) {
        emit toastRequested(QStringLiteral("未自动找到 llama-server，请手动选择路径。"));
        return;
    }
    m_serverPath->setText(path);
    emit toastRequested(QStringLiteral("已找到 llama-server：%1").arg(path));
}
