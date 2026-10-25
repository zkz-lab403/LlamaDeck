#include "ModelPage.h"

#include "AppSettings.h"
#include "Icons.h"
#include "ModelStore.h"
#include "ServerController.h"
#include "SystemMonitor.h"
#include "Theme.h"
#include "Widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextOption>
#include <QUrl>
#include <QVBoxLayout>

namespace {

enum ModelRole {
    PathRole = Qt::UserRole + 1,
    NameRole,
    MetaRole,
    FileRole,
    QuantRole,
    SizeRole
};

class ModelDelegate : public QStyledItemDelegate {
public:
    explicit ModelDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(280, 84);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRectF card = QRectF(option.rect).adjusted(2.0, 3.0, -2.0, -3.0);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        QColor background = Qt::transparent;
        if (selected)
            background = QColor(108, 140, 255, 32);
        else if (hovered)
            background = QColor(255, 255, 255, 9);

        painter->setPen(selected ? QPen(QColor(108, 140, 255, 120), 1.0)
                                 : QPen(QColor(31, 39, 54, 200), 1.0));
        painter->setBrush(background);
        painter->drawRoundedRect(card, 10.0, 10.0);

        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::Accent);
            painter->drawRoundedRect(QRectF(card.left(), card.top() + 16.0, 3.0,
                                           card.height() - 32.0), 1.5, 1.5);
        }

        const QString name = index.data(NameRole).toString();
        const QString meta = index.data(MetaRole).toString();
        const QString file = index.data(FileRole).toString();
        const QString quant = index.data(QuantRole).toString();
        const QString size = index.data(SizeRole).toString();

        const QRectF content = card.adjusted(16.0, 9.0, -14.0, -8.0);

        QFont nameFont = option.font;
        nameFont.setPixelSize(13);
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(QColor(236, 241, 250));
        const QRectF nameRect(content.left(), content.top(), content.width() - 86.0, 20.0);
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(nameFont).elidedText(name, Qt::ElideRight,
                                                            int(nameRect.width())));

        QFont metaFont = option.font;
        metaFont.setPixelSize(10);
        painter->setFont(metaFont);
        painter->setPen(QColor(126, 139, 160));
        const QRectF metaRect(content.left(), content.top() + 24.0, content.width() - 90.0, 15.0);
        painter->drawText(metaRect, Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(metaFont).elidedText(meta, Qt::ElideRight,
                                                            int(metaRect.width())));

        QFont fileFont = option.font;
        fileFont.setPixelSize(10);
        painter->setFont(fileFont);
        painter->setPen(QColor(88, 101, 123));
        const QRectF fileRect(content.left(), content.top() + 42.0, content.width() - 90.0, 14.0);
        painter->drawText(fileRect, Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(fileFont).elidedText(file, Qt::ElideMiddle,
                                                            int(fileRect.width())));

        if (!quant.isEmpty()) {
            QFont chipFont = option.font;
            chipFont.setPixelSize(9);
            chipFont.setWeight(QFont::DemiBold);
            painter->setFont(chipFont);
            const QFontMetrics chipMetrics(chipFont);
            const qreal chipWidth = chipMetrics.horizontalAdvance(quant) + 16.0;
            const QRectF chip(card.right() - 14.0 - chipWidth, card.top() + 11.0, chipWidth, 20.0);
            painter->setPen(QPen(QColor(108, 140, 255, 90), 1.0));
            painter->setBrush(QColor(108, 140, 255, 26));
            painter->drawRoundedRect(chip, 6.0, 6.0);
            painter->setPen(QColor(160, 182, 255));
            painter->drawText(chip, Qt::AlignCenter, quant);
        }

        if (!size.isEmpty()) {
            QFont sizeFont = option.font;
            sizeFont.setPixelSize(10);
            painter->setFont(sizeFont);
            painter->setPen(QColor(112, 125, 147));
            const QRectF sizeRect(card.right() - 110.0, card.top() + 36.0, 96.0, 16.0);
            painter->drawText(sizeRect, Qt::AlignRight | Qt::AlignVCenter, size);
        }

        painter->restore();
    }
};

QWidget *makeField(const QString &label, QWidget *input, const QString &hint = QString())
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    auto *title = makeLabel(label, QStringLiteral("FieldLabel"), widget);
    layout->addWidget(title);
    layout->addWidget(input);
    if (!hint.isEmpty()) {
        auto *hintLabel = makeLabel(hint, QStringLiteral("Hint"), widget);
        hintLabel->setWordWrap(true);
        layout->addWidget(hintLabel);
    }
    return widget;
}

QWidget *makeSwitchRow(const QString &title, const QString &hint, ToggleSwitch *toggle)
{
    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(12);

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

void styleSpin(QSpinBox *spin, int minimum, int maximum, int value, int step = 1)
{
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setValue(value);
    spin->setMinimumHeight(34);
    spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
}

} // namespace

ModelPage::ModelPage(AppSettings *settings, ServerController *controller, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_controller(controller)
{
    buildUi();

    connect(m_settings, &AppSettings::changed, this, [this]() {
        updateServerHint();
        syncDefaults();
        updateCommandPreview();
    });

    connect(m_controller, &ServerController::stateChanged, this, [this](ServerState state) {
        const bool busy = state == ServerState::Starting || state == ServerState::Running
            || state == ServerState::Stopping;
        setConfigEnabled(!busy);
        m_stopButton->setVisible(busy);
        m_startButton->setEnabled(!busy && hasSelection()
                                  && !m_settings->llamaServerPath.isEmpty());
        if (state == ServerState::Running)
            m_startButton->setText(QStringLiteral("服务运行中"));
        else if (state == ServerState::Starting)
            m_startButton->setText(QStringLiteral("正在启动…"));
        else if (state == ServerState::Stopping)
            m_startButton->setText(QStringLiteral("正在停止…"));
        else
            m_startButton->setText(QStringLiteral("一键启动"));
        updateCommandPreview();
    });

    refreshModels();
}

void ModelPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 10, 32, 24);
    layout->setSpacing(16);

    m_banner = new QFrame(this);
    m_banner->setObjectName(QStringLiteral("CardFlat"));
    auto *bannerLayout = new QHBoxLayout(m_banner);
    bannerLayout->setContentsMargins(14, 10, 12, 10);
    bannerLayout->setSpacing(10);
    auto *bannerDot = new QLabel(m_banner);
    bannerDot->setFixedSize(7, 7);
    bannerDot->setStyleSheet(QStringLiteral("background-color:#F5B84A;border-radius:3px;"));
    auto *bannerText = makeLabel(QStringLiteral("未检测到 llama-server，启动前请先指定可执行文件路径。"),
                                 QStringLiteral("Muted"), m_banner);
    auto *bannerButton = new QPushButton(QStringLiteral("前往设置"), m_banner);
    bannerButton->setObjectName(QStringLiteral("Ghost"));
    bannerButton->setCursor(Qt::PointingHandCursor);
    bannerLayout->addWidget(bannerDot);
    bannerLayout->addWidget(bannerText, 1);
    bannerLayout->addWidget(bannerButton);
    connect(bannerButton, &QPushButton::clicked, this, &ModelPage::openSettingsRequested);
    layout->addWidget(m_banner);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索模型名称、文件或架构…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumHeight(36);
    m_search->setMaximumWidth(380);
    m_search->addAction(Icons::icon(Icons::Glyph::Search, Theme::Dim, 16),
                        QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &ModelPage::filterModels);

    m_countLabel = makeLabel(QStringLiteral("0 个模型"), QStringLiteral("Hint"), this);

    auto *folderButton = new QPushButton(Icons::icon(Icons::Glyph::Folder, Theme::Muted, 15),
                                         QString(), this);
    folderButton->setObjectName(QStringLiteral("Ghost"));
    folderButton->setToolTip(QStringLiteral("打开模型目录"));
    folderButton->setCursor(Qt::PointingHandCursor);
    folderButton->setFixedWidth(38);
    connect(folderButton, &QPushButton::clicked, this, [this]() {
        const QDir dir(m_settings->modelsDir);
        if (dir.exists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        else
            emit toastRequested(QStringLiteral("模型目录不存在，请先在设置中配置。"));
    });

    auto *scanButton = new QPushButton(Icons::icon(Icons::Glyph::Refresh, Theme::Muted, 15),
                                        QStringLiteral("重新扫描"), this);
    scanButton->setObjectName(QStringLiteral("Ghost"));
    scanButton->setCursor(Qt::PointingHandCursor);
    scanButton->setMinimumHeight(36);
    connect(scanButton, &QPushButton::clicked, this, [this]() {
        refreshModels();
        emit toastRequested(QStringLiteral("已扫描模型目录。"));
    });

    toolbar->addWidget(m_search);
    toolbar->addWidget(m_countLabel);
    toolbar->addStretch();
    toolbar->addWidget(folderButton);
    toolbar->addWidget(scanButton);
    layout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);

    auto *listCard = makeCard(splitter);
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(8, 8, 8, 8);
    listLayout->setSpacing(0);

    m_listStack = new QStackedWidget(listCard);
    m_list = new QListWidget(m_listStack);
    m_list->setObjectName(QStringLiteral("ModelList"));
    m_list->setItemDelegate(new ModelDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMouseTracking(true);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(1);
    m_list->setFrameShape(QFrame::NoFrame);
    connect(m_list, &QListWidget::currentItemChanged, this, [this]() { updateSelection(); });

    m_listStack->addWidget(m_list);
    m_listStack->addWidget(buildEmptyState());
    listLayout->addWidget(m_listStack);

    auto *configPanel = new QWidget(splitter);
    configPanel->setMinimumWidth(430);
    auto *configPanelLayout = new QVBoxLayout(configPanel);
    configPanelLayout->setContentsMargins(0, 0, 0, 0);
    configPanelLayout->setSpacing(8);

    auto *configScroll = new QScrollArea(configPanel);
    configScroll->setWidgetResizable(true);
    configScroll->setFrameShape(QFrame::NoFrame);
    configScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *configCard = makeCard(configScroll);
    configCard->setMinimumWidth(420);
    auto *config = new QVBoxLayout(configCard);
    config->setContentsMargins(20, 18, 20, 18);
    config->setSpacing(12);

    auto *configHeader = new QHBoxLayout();
    auto *configTitle = makeLabel(QStringLiteral("启动配置"), QStringLiteral("SectionTitle"), configCard);
    m_quantBadge = new Badge(QStringLiteral("GGUF"), QStringLiteral("muted"), configCard);
    configHeader->addWidget(configTitle);
    configHeader->addStretch();
    configHeader->addWidget(m_quantBadge);
    config->addLayout(configHeader);

    m_modelName = new QLabel(QStringLiteral("选择一个模型"), configCard);
    m_modelName->setWordWrap(true);
    m_modelName->setStyleSheet(QStringLiteral("font-size:15px;font-weight:600;color:#EDF2FA;"));
    m_modelMeta = makeLabel(QStringLiteral("从左侧列表中选择 .gguf 文件"), QStringLiteral("Hint"),
                            configCard);
    m_modelMeta->setWordWrap(true);
    config->addWidget(m_modelName);
    config->addWidget(m_modelMeta);
    config->addWidget(makeDivider(configCard));

    m_port = new QSpinBox(configCard);
    m_port->setRange(1024, 65535);
    m_port->setValue(m_settings->port);
    m_port->setMinimumHeight(34);
    m_ctx = new QSpinBox(configCard);
    styleSpin(m_ctx, 256, 1048576, m_settings->ctx, 512);
    m_gpuLayers = new QSpinBox(configCard);
    m_gpuLayers->setRange(-1, 999);
    m_gpuLayers->setSpecialValueText(QStringLiteral("自动"));
    m_gpuLayers->setValue(m_settings->gpuLayers);
    m_gpuLayers->setMinimumHeight(34);
    m_threads = new QSpinBox(configCard);
    m_threads->setRange(0, 256);
    m_threads->setSpecialValueText(QStringLiteral("自动"));
    m_threads->setValue(m_settings->threads);
    m_threads->setMinimumHeight(34);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->addWidget(makeField(QStringLiteral("端口"), m_port), 0, 0);
    grid->addWidget(makeField(QStringLiteral("上下文 (-c)"), m_ctx), 0, 1);
    grid->addWidget(makeField(QStringLiteral("GPU 层数 (-ngl)"), m_gpuLayers), 1, 0);
    grid->addWidget(makeField(QStringLiteral("线程数 (-t)"), m_threads), 1, 1);
    config->addLayout(grid);

    config->addWidget(makeDivider(configCard));

    auto *perfHeader = new QHBoxLayout();
    auto *perfTitleBox = new QVBoxLayout();
    perfTitleBox->setSpacing(1);
    perfTitleBox->addWidget(makeLabel(QStringLiteral("性能参数"), QStringLiteral("FieldLabel"),
                                      configCard));
    auto *perfHint = makeLabel(QStringLiteral("批处理、并行槽位与 KV 缓存，影响吞吐和显存占用"),
                               QStringLiteral("Hint"), configCard);
    perfHint->setWordWrap(true);
    perfTitleBox->addWidget(perfHint);
    perfHeader->addLayout(perfTitleBox, 1);

    m_recommendButton = new QPushButton(QStringLiteral("按硬件推荐"), configCard);
    m_recommendButton->setObjectName(QStringLiteral("Ghost"));
    m_recommendButton->setCursor(Qt::PointingHandCursor);
    m_recommendButton->setMinimumHeight(26);
    perfHeader->addWidget(m_recommendButton, 0, Qt::AlignTop);
    config->addLayout(perfHeader);

    m_batch = new QSpinBox(configCard);
    styleSpin(m_batch, 64, 16384, m_settings->batch, 128);
    m_ubatch = new QSpinBox(configCard);
    styleSpin(m_ubatch, 64, 8192, m_settings->ubatch, 64);
    m_parallel = new QSpinBox(configCard);
    styleSpin(m_parallel, 1, 16, m_settings->parallel);

    m_kvCache = new QComboBox(configCard);
    m_kvCache->addItem(QStringLiteral("f16（默认，质量最好）"), QStringLiteral("f16"));
    m_kvCache->addItem(QStringLiteral("q8_0（省显存）"), QStringLiteral("q8_0"));
    m_kvCache->addItem(QStringLiteral("q4_0（更省显存）"), QStringLiteral("q4_0"));
    m_kvCache->setMinimumHeight(34);
    const int kvIndex = m_kvCache->findData(m_settings->kvCacheType);
    m_kvCache->setCurrentIndex(kvIndex >= 0 ? kvIndex : 0);

    auto *perfGrid = new QGridLayout();
    perfGrid->setHorizontalSpacing(12);
    perfGrid->setVerticalSpacing(10);
    perfGrid->addWidget(makeField(QStringLiteral("批大小 (-b)"), m_batch), 0, 0);
    perfGrid->addWidget(makeField(QStringLiteral("微批 (-ub)"), m_ubatch), 0, 1);
    perfGrid->addWidget(makeField(QStringLiteral("并行槽位 (-np)"), m_parallel), 1, 0);
    perfGrid->addWidget(makeField(QStringLiteral("KV 缓存类型"), m_kvCache), 1, 1);
    config->addLayout(perfGrid);

    m_mlock = new ToggleSwitch(configCard);
    m_mlock->setChecked(m_settings->mlock);
    m_noMmap = new ToggleSwitch(configCard);
    m_noMmap->setChecked(m_settings->noMmap);
    config->addWidget(makeSwitchRow(QStringLiteral("锁定内存 (--mlock)"),
                                    QStringLiteral("防止模型被换出，需要足够空闲内存"), m_mlock));
    config->addWidget(makeSwitchRow(QStringLiteral("禁用 mmap (--no-mmap)"),
                                    QStringLiteral("整模型读入内存，避免缺页抖动"), m_noMmap));

    config->addWidget(makeDivider(configCard));

    m_flashAttn = new ToggleSwitch(configCard);
    m_flashAttn->setChecked(m_settings->flashAttn);
    m_jinja = new ToggleSwitch(configCard);
    m_jinja->setChecked(m_settings->jinja);
    config->addWidget(makeSwitchRow(QStringLiteral("Flash Attention"),
                                    QStringLiteral("加速长上下文推理"), m_flashAttn));
    config->addWidget(makeSwitchRow(QStringLiteral("Jinja 聊天模板"),
                                    QStringLiteral("启用工具调用与模板渲染"), m_jinja));

    config->addWidget(makeDivider(configCard));

    m_apiKey = new QLineEdit(configCard);
    m_apiKey->setPlaceholderText(QStringLiteral("可选，留空表示无需鉴权"));
    m_apiKey->setMinimumHeight(34);
    m_apiKey->setText(m_settings->apiKey);
    config->addWidget(makeField(QStringLiteral("API Key"), m_apiKey));

    m_extraArgs = new QLineEdit(configCard);
    m_extraArgs->setPlaceholderText(QStringLiteral("例如：--no-mmap --mlock --parallel 2"));
    m_extraArgs->setMinimumHeight(34);
    m_extraArgs->setText(m_settings->extraArgs);
    config->addWidget(makeField(QStringLiteral("附加参数"), m_extraArgs,
                                QStringLiteral("空格分隔，将原样追加到命令末尾")));

    auto *commandHeader = new QHBoxLayout();
    commandHeader->addWidget(makeLabel(QStringLiteral("启动命令"), QStringLiteral("FieldLabel"),
                                       configCard));
    commandHeader->addStretch();
    auto *copyButton = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                        QString(), configCard);
    copyButton->setObjectName(QStringLiteral("IconButton"));
    copyButton->setToolTip(QStringLiteral("复制启动命令"));
    copyButton->setCursor(Qt::PointingHandCursor);
    copyButton->setFixedSize(28, 24);
    commandHeader->addWidget(copyButton);
    config->addLayout(commandHeader);

    m_commandView = new QPlainTextEdit(configCard);
    m_commandView->setObjectName(QStringLiteral("CommandView"));
    m_commandView->setReadOnly(true);
    m_commandView->setFixedHeight(88);
    m_commandView->setWordWrapMode(QTextOption::WrapAnywhere);
    config->addWidget(m_commandView);

    auto *endpointRow = new QHBoxLayout();
    endpointRow->setSpacing(8);
    endpointRow->addWidget(makeLabel(QStringLiteral("服务地址"), QStringLiteral("FieldLabel"),
                                     configCard));
    m_endpoint = makeLabel(QStringLiteral("http://127.0.0.1:8080"), QStringLiteral("Endpoint"),
                           configCard);
    m_endpoint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    endpointRow->addWidget(m_endpoint, 1);
    auto *endpointCopy = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                          QString(), configCard);
    endpointCopy->setObjectName(QStringLiteral("IconButton"));
    endpointCopy->setToolTip(QStringLiteral("复制服务地址"));
    endpointCopy->setCursor(Qt::PointingHandCursor);
    endpointCopy->setFixedSize(28, 24);
    endpointRow->addWidget(endpointCopy);
    config->addLayout(endpointRow);

    connect(copyButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_commandView->toPlainText());
        emit toastRequested(QStringLiteral("启动命令已复制到剪贴板。"));
    });
    connect(endpointCopy, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_endpoint->text());
        emit toastRequested(QStringLiteral("服务地址已复制到剪贴板。"));
    });

    config->addStretch();

    configScroll->setWidget(configCard);
    configPanelLayout->addWidget(configScroll, 1);

    auto *actionCard = makeCard(configPanel);
    auto *actionLayout = new QHBoxLayout(actionCard);
    actionLayout->setContentsMargins(14, 10, 14, 10);
    actionLayout->setSpacing(10);
    actionLayout->addStretch();

    m_stopButton = new QPushButton(Icons::icon(Icons::Glyph::Stop, Theme::Danger, 14),
                                    QStringLiteral("停止服务"), actionCard);
    m_stopButton->setObjectName(QStringLiteral("Danger"));
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setVisible(false);

    m_startButton = new QPushButton(Icons::icon(Icons::Glyph::Play, Qt::white, 14),
                                     QStringLiteral("一键启动"), actionCard);
    m_startButton->setObjectName(QStringLiteral("Primary"));
    m_startButton->setMinimumHeight(40);
    m_startButton->setMinimumWidth(150);
    m_startButton->setCursor(Qt::PointingHandCursor);

    actionLayout->addWidget(m_stopButton);
    actionLayout->addWidget(m_startButton);
    configPanelLayout->addWidget(actionCard);

    connect(m_startButton, &QPushButton::clicked, this, &ModelPage::startRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &ModelPage::stopRequested);

    splitter->addWidget(listCard);
    splitter->addWidget(configPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({760, 460});
    layout->addWidget(splitter, 1);

    connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_ctx, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_gpuLayers, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_threads, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_batch, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_ubatch, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_parallel, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_kvCache, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { updateCommandPreview(); });
    connect(m_mlock, &ToggleSwitch::toggled, this, [this]() { updateCommandPreview(); });
    connect(m_noMmap, &ToggleSwitch::toggled, this, [this]() { updateCommandPreview(); });
    connect(m_recommendButton, &QPushButton::clicked, this,
            &ModelPage::applyRecommendedPerformance);
    connect(m_flashAttn, &ToggleSwitch::toggled, this, [this]() { updateCommandPreview(); });
    connect(m_jinja, &ToggleSwitch::toggled, this, [this]() { updateCommandPreview(); });
    connect(m_apiKey, &QLineEdit::textChanged, this, [this]() { updateCommandPreview(); });
    connect(m_extraArgs, &QLineEdit::textChanged, this, [this]() { updateCommandPreview(); });
    connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_controller->state() != ServerState::Running
            && m_controller->state() != ServerState::Starting)
            m_endpoint->setText(QStringLiteral("http://127.0.0.1:%1").arg(value));
    });

    updateServerHint();
    updateCommandPreview();
    updateSelection();
}

QWidget *ModelPage::buildEmptyState()
{
    auto *empty = new QWidget();
    auto *layout = new QVBoxLayout(empty);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->addStretch();

    auto *icon = new QLabel(empty);
    icon->setPixmap(Icons::pixmap(Icons::Glyph::Models, QColor(78, 89, 108), 44));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto *title = makeLabel(QStringLiteral("还没有发现模型"), QStringLiteral("SectionTitle"), empty);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *hint = makeLabel(QString(), QStringLiteral("Hint"), empty);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    hint->setText(QStringLiteral("未在 %1 找到 .gguf 文件\n可前往设置修改模型目录").arg(m_settings->modelsDir));
    layout->addWidget(hint);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    buttons->addStretch();
    auto *openFolder = new QPushButton(Icons::icon(Icons::Glyph::Folder, Theme::Muted, 14),
                                       QStringLiteral("模型目录"), empty);
    openFolder->setObjectName(QStringLiteral("Ghost"));
    openFolder->setCursor(Qt::PointingHandCursor);
    connect(openFolder, &QPushButton::clicked, this, [this]() {
        const QDir dir(m_settings->modelsDir);
        if (dir.exists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        else
            emit openSettingsRequested();
    });
    auto *rescan = new QPushButton(Icons::icon(Icons::Glyph::Refresh, Theme::Muted, 14),
                                    QStringLiteral("重新扫描"), empty);
    rescan->setObjectName(QStringLiteral("Ghost"));
    rescan->setCursor(Qt::PointingHandCursor);
    connect(rescan, &QPushButton::clicked, this, &ModelPage::refreshModels);
    buttons->addWidget(openFolder);
    buttons->addWidget(rescan);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();
    return empty;
}

void ModelPage::refreshModels()
{
    const QString previous = m_list->currentItem()
        ? m_list->currentItem()->data(PathRole).toString()
        : (m_list->count() > 0 ? QString() : m_settings->lastModelPath);

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const QVector<ModelInfo> models = ModelStore::scan(m_settings->modelsDir);
    QGuiApplication::restoreOverrideCursor();

    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const ModelInfo &model : models) {
            auto *item = new QListWidgetItem(m_list);
            QStringList metaParts;
            if (model.parameterCount > 0)
                metaParts << QStringLiteral("%1 参数").arg(ModelStore::formatParams(model.parameterCount));
            if (model.contextLength > 0)
                metaParts << QStringLiteral("%1 上下文").arg(ModelStore::formatContext(model.contextLength));
            if (!model.architecture.isEmpty())
                metaParts << model.architecture;
            if (metaParts.isEmpty())
                metaParts << QStringLiteral("元数据未解析");

            item->setData(PathRole, model.path);
            item->setData(NameRole, model.name);
            item->setData(MetaRole, metaParts.join(QStringLiteral("  ·  ")));
            item->setData(FileRole, model.fileName);
            item->setData(QuantRole, model.quantization);
            item->setData(SizeRole, ModelStore::formatSize(model.sizeBytes));
            item->setToolTip(model.path);
            item->setSizeHint(QSize(280, 84));
        }
    }

    m_totalModels = m_list->count();
    filterModels(m_search->text());

    QListWidgetItem *target = nullptr;
    if (!previous.isEmpty())
        target = itemForPath(previous);
    if (!target && !m_settings->lastModelPath.isEmpty())
        target = itemForPath(m_settings->lastModelPath);
    if (!target && m_list->count() > 0)
        target = m_list->item(0);

    if (target) {
        m_list->setCurrentItem(target);
        m_list->scrollToItem(target);
    }

    m_listStack->setCurrentIndex(m_list->count() > 0 ? 0 : 1);
    updateSelection();
    updateCountLabel();
}

bool ModelPage::selectModelByPath(const QString &path)
{
    QListWidgetItem *item = itemForPath(path);
    if (!item && m_list->count() == 0) {
        refreshModels();
        item = itemForPath(path);
    }
    if (!item)
        return false;
    if (item->isHidden()) {
        m_search->clear();
        filterModels(QString());
    }
    m_list->setCurrentItem(item);
    m_list->scrollToItem(item);
    updateSelection();
    return true;
}

void ModelPage::applyOptions(const LaunchOptions &options)
{
    m_port->setValue(options.port);
    m_ctx->setValue(options.ctx);
    m_gpuLayers->setValue(options.gpuLayers);
    m_threads->setValue(options.threads);
    m_batch->setValue(options.batch);
    m_ubatch->setValue(options.ubatch);
    m_parallel->setValue(options.parallel);
    const int kvIndex = m_kvCache->findData(options.kvCacheType);
    m_kvCache->setCurrentIndex(kvIndex >= 0 ? kvIndex : 0);
    m_mlock->setChecked(options.mlock);
    m_noMmap->setChecked(options.noMmap);
    m_flashAttn->setChecked(options.flashAttn);
    m_jinja->setChecked(options.jinja);
    m_apiKey->setText(options.apiKey);
    m_extraArgs->setText(options.extraArgs);
    updateCommandPreview();
}

void ModelPage::syncDefaults()
{
    if (m_controller->state() == ServerState::Starting
        || m_controller->state() == ServerState::Running
        || m_controller->state() == ServerState::Stopping)
        return;

    m_port->setValue(m_settings->port);
    m_ctx->setValue(m_settings->ctx);
    m_gpuLayers->setValue(m_settings->gpuLayers);
    m_threads->setValue(m_settings->threads);
    m_batch->setValue(m_settings->batch);
    m_ubatch->setValue(m_settings->ubatch);
    m_parallel->setValue(m_settings->parallel);
    const int kvIndex = m_kvCache->findData(m_settings->kvCacheType);
    m_kvCache->setCurrentIndex(kvIndex >= 0 ? kvIndex : 0);
    m_mlock->setChecked(m_settings->mlock);
    m_noMmap->setChecked(m_settings->noMmap);
    m_flashAttn->setChecked(m_settings->flashAttn);
    m_jinja->setChecked(m_settings->jinja);
    m_apiKey->setText(m_settings->apiKey);
    m_extraArgs->setText(m_settings->extraArgs);
    updateCommandPreview();
}

LaunchOptions ModelPage::currentOptions() const
{
    LaunchOptions options;
    if (m_list->currentItem() && !m_list->currentItem()->isHidden())
        options.modelPath = m_list->currentItem()->data(PathRole).toString();
    options.port = m_port->value();
    options.ctx = m_ctx->value();
    options.gpuLayers = m_gpuLayers->value();
    options.threads = m_threads->value();
    options.batch = m_batch->value();
    options.ubatch = m_ubatch->value();
    options.parallel = m_parallel->value();
    options.kvCacheType = m_kvCache->currentData().toString();
    options.mlock = m_mlock->isChecked();
    options.noMmap = m_noMmap->isChecked();
    options.flashAttn = m_flashAttn->isChecked();
    options.jinja = m_jinja->isChecked();
    options.apiKey = m_apiKey->text().trimmed();
    options.extraArgs = m_extraArgs->text().trimmed();
    return options;
}

QString ModelPage::selectedModelName() const
{
    if (!hasSelection())
        return QString();
    return m_list->currentItem()->data(NameRole).toString();
}

void ModelPage::filterModels(const QString &query)
{
    const QString needle = query.trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const bool matches = needle.isEmpty()
            || item->data(NameRole).toString().contains(needle, Qt::CaseInsensitive)
            || item->data(FileRole).toString().contains(needle, Qt::CaseInsensitive)
            || item->data(MetaRole).toString().contains(needle, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
    updateSelection();
    updateCountLabel();
}

void ModelPage::updateSelection()
{
    QListWidgetItem *item = m_list->currentItem();
    const bool selected = item && !item->isHidden();

    if (selected) {
        const QString path = item->data(PathRole).toString();
        m_modelName->setText(item->data(NameRole).toString());
        m_modelMeta->setText(item->data(FileRole).toString());
        const QString quant = item->data(QuantRole).toString();
        m_quantBadge->setText(quant.isEmpty() ? QStringLiteral("GGUF") : quant);
        m_quantBadge->setVariant(quant.isEmpty() ? QStringLiteral("muted")
                                                 : QStringLiteral("accent"));
        m_settings->lastModelPath = path;
    } else {
        m_modelName->setText(QStringLiteral("选择一个模型"));
        m_modelMeta->setText(QStringLiteral("从左侧列表中选择 .gguf 文件"));
        m_quantBadge->setText(QStringLiteral("GGUF"));
        m_quantBadge->setVariant(QStringLiteral("muted"));
    }

    const ServerState state = m_controller->state();
    const bool busy = state == ServerState::Starting || state == ServerState::Running
        || state == ServerState::Stopping;
    m_startButton->setEnabled(selected && !busy && !m_settings->llamaServerPath.isEmpty());
    updateCommandPreview();
}

void ModelPage::updateCommandPreview()
{
    QString executable = m_settings->llamaServerPath;
    if (executable.isEmpty())
        executable = QStringLiteral("llama-server.exe");

    LaunchOptions options = currentOptions();
    if (options.modelPath.isEmpty())
        options.modelPath = QStringLiteral("<model.gguf>");

    m_commandView->setPlainText(
        ServerController::formatCommand(executable, ServerController::buildArguments(options)));

    int displayPort = options.port;
    if (m_controller->state() == ServerState::Running
        || m_controller->state() == ServerState::Starting)
        displayPort = m_controller->options().port;
    m_endpoint->setText(QStringLiteral("http://127.0.0.1:%1").arg(displayPort));
}

void ModelPage::applyRecommendedPerformance()
{
    const HardwareInfo hardware = SystemMonitor::queryHardware();

    m_parallel->setValue(1);
    m_batch->setValue(2048);
    m_ubatch->setValue(512);
    m_flashAttn->setChecked(true);

    const bool lowVram = hardware.nvidia && hardware.vramTotalMb > 0.0
        && hardware.vramTotalMb <= 8192.0;
    const QString cacheType = lowVram ? QStringLiteral("q8_0") : QStringLiteral("f16");
    const int index = m_kvCache->findData(cacheType);
    if (index >= 0)
        m_kvCache->setCurrentIndex(index);

    updateCommandPreview();

    if (hardware.gpuName.isEmpty()) {
        emit toastRequested(QStringLiteral(
            "已应用推荐参数：并行 1、批 2048/512；未检测到 NVIDIA GPU，KV 缓存保持 f16。"));
        return;
    }

    emit toastRequested(QStringLiteral("已应用推荐参数：并行 1、批 2048/512、KV %1（%2 · %3 GB 显存）")
                           .arg(cacheType, hardware.gpuName)
                           .arg(hardware.vramTotalMb / 1024.0, 0, 'f', 1));
}

void ModelPage::updateServerHint()
{
    const bool hasServer = !m_settings->llamaServerPath.isEmpty()
        && QFileInfo::exists(m_settings->llamaServerPath);
    m_banner->setVisible(!hasServer);
    if (!hasServer) {
        m_list->setEnabled(true);
    }
    const bool busy = m_controller->state() == ServerState::Starting
        || m_controller->state() == ServerState::Running
        || m_controller->state() == ServerState::Stopping;
    m_startButton->setEnabled(hasServer && hasSelection() && !busy);
}

void ModelPage::updateCountLabel()
{
    int visible = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (!m_list->item(i)->isHidden())
            ++visible;
    }
    if (visible == m_totalModels)
        m_countLabel->setText(QStringLiteral("%1 个模型").arg(m_totalModels));
    else
        m_countLabel->setText(QStringLiteral("显示 %1 / %2").arg(visible).arg(m_totalModels));
}

void ModelPage::setConfigEnabled(bool enabled)
{
    m_port->setEnabled(enabled);
    m_ctx->setEnabled(enabled);
    m_gpuLayers->setEnabled(enabled);
    m_threads->setEnabled(enabled);
    m_batch->setEnabled(enabled);
    m_ubatch->setEnabled(enabled);
    m_parallel->setEnabled(enabled);
    m_kvCache->setEnabled(enabled);
    m_mlock->setEnabled(enabled);
    m_noMmap->setEnabled(enabled);
    m_recommendButton->setEnabled(enabled);
    m_flashAttn->setEnabled(enabled);
    m_jinja->setEnabled(enabled);
    m_apiKey->setEnabled(enabled);
    m_extraArgs->setEnabled(enabled);
}

QListWidgetItem *ModelPage::itemForPath(const QString &path) const
{
    if (path.isEmpty())
        return nullptr;
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const QString candidate = QDir::cleanPath(QFileInfo(item->data(PathRole).toString()).absoluteFilePath());
        if (candidate.compare(normalized, Qt::CaseInsensitive) == 0)
            return item;
    }
    return nullptr;
}

bool ModelPage::hasSelection() const
{
    QListWidgetItem *item = m_list->currentItem();
    return item && !item->isHidden();
}
