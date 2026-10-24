#include "HistoryPage.h"

#include "HistoryStore.h"
#include "Icons.h"
#include "Theme.h"
#include "Widgets.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

HistoryPage::HistoryPage(HistoryStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    buildUi();
    connect(m_store, &HistoryStore::changed, this, &HistoryPage::reload);
    reload();
}

void HistoryPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 8, 28, 22);
    layout->setSpacing(14);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);
    m_countLabel = makeLabel(QStringLiteral("共 0 条记录"), QStringLiteral("Hint"), this);
    toolbar->addWidget(m_countLabel);
    toolbar->addStretch();

    auto *exportButton = new QPushButton(Icons::icon(Icons::Glyph::Export, Theme::Muted, 15),
                                         QStringLiteral("导出 CSV"), this);
    exportButton->setObjectName(QStringLiteral("Ghost"));
    exportButton->setCursor(Qt::PointingHandCursor);
    exportButton->setMinimumHeight(36);
    connect(exportButton, &QPushButton::clicked, this, &HistoryPage::exportCsv);

    auto *clearButton = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Muted, 15),
                                         QStringLiteral("清空历史"), this);
    clearButton->setObjectName(QStringLiteral("Ghost"));
    clearButton->setCursor(Qt::PointingHandCursor);
    clearButton->setMinimumHeight(36);
    connect(clearButton, &QPushButton::clicked, this, &HistoryPage::clearHistory);

    toolbar->addWidget(exportButton);
    toolbar->addWidget(clearButton);
    layout->addLayout(toolbar);

    m_stack = new QStackedWidget(this);

    m_table = new QTableWidget(m_stack);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("模型"),
                                        QStringLiteral("端口"), QStringLiteral("运行时长"),
                                        QStringLiteral("平均 (t/s)"), QStringLiteral("峰值 (t/s)"),
                                        QStringLiteral("状态"), QStringLiteral("操作")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int i = 2; i <= 7; ++i)
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 156);
    m_table->setColumnWidth(2, 64);
    m_table->setColumnWidth(3, 96);
    m_table->setColumnWidth(4, 90);
    m_table->setColumnWidth(5, 90);
    m_table->setColumnWidth(6, 92);
    m_table->setColumnWidth(7, 88);

    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(52);
    m_table->horizontalHeader()->setFixedHeight(38);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setWordWrap(false);

    m_stack->addWidget(m_table);
    m_stack->addWidget(buildEmptyState());
    layout->addWidget(m_stack, 1);
}

QWidget *HistoryPage::buildEmptyState()
{
    auto *empty = new QWidget();
    auto *layout = new QVBoxLayout(empty);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->addStretch();

    auto *icon = new QLabel(empty);
    icon->setPixmap(Icons::pixmap(Icons::Glyph::History, QColor(78, 89, 108), 44));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto *title = makeLabel(QStringLiteral("还没有启动记录"), QStringLiteral("SectionTitle"), empty);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *hint = makeLabel(QStringLiteral("启动模型后，这里会自动保存命令、运行时长与速度统计。"),
                            QStringLiteral("Hint"), empty);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch();
    return empty;
}

void HistoryPage::reload()
{
    const QVector<HistoryEntry> entries = m_store->entries();
    m_countLabel->setText(QStringLiteral("共 %1 条记录").arg(entries.size()));
    m_stack->setCurrentIndex(entries.isEmpty() ? 1 : 0);

    m_table->clearContents();
    m_table->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const HistoryEntry &entry = entries.at(row);
        const bool live = entry.status == QStringLiteral("运行中")
            || entry.status == QStringLiteral("启动中");
        qint64 duration = entry.durationSec;
        if (live && entry.startedAt.isValid())
            duration = entry.startedAt.secsTo(QDateTime::currentDateTime());

        auto *timeItem = new QTableWidgetItem(
            entry.startedAt.toString(QStringLiteral("MM-dd HH:mm:ss")));
        timeItem->setForeground(QColor(150, 161, 181));
        timeItem->setToolTip(entry.startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        m_table->setItem(row, 0, timeItem);

        auto *modelItem = new QTableWidgetItem(entry.modelName);
        modelItem->setToolTip(entry.modelPath);
        QFont modelFont = modelItem->font();
        modelFont.setWeight(QFont::DemiBold);
        modelItem->setFont(modelFont);
        m_table->setItem(row, 1, modelItem);

        auto *portItem = new QTableWidgetItem(QString::number(entry.options.port));
        portItem->setTextAlignment(Qt::AlignCenter);
        portItem->setForeground(QColor(168, 180, 200));
        m_table->setItem(row, 2, portItem);

        auto *durationItem = new QTableWidgetItem(formatDuration(duration));
        durationItem->setTextAlignment(Qt::AlignCenter);
        durationItem->setForeground(QColor(168, 180, 200));
        m_table->setItem(row, 3, durationItem);

        auto *avgItem = new QTableWidgetItem(
            entry.avgTps > 0.0 ? QString::number(entry.avgTps, 'f', 1) : QStringLiteral("—"));
        avgItem->setTextAlignment(Qt::AlignCenter);
        avgItem->setForeground(QColor(200, 211, 230));
        m_table->setItem(row, 4, avgItem);

        auto *peakItem = new QTableWidgetItem(
            entry.peakTps > 0.0 ? QString::number(entry.peakTps, 'f', 1) : QStringLiteral("—"));
        peakItem->setTextAlignment(Qt::AlignCenter);
        peakItem->setForeground(QColor(200, 211, 230));
        m_table->setItem(row, 5, peakItem);

        auto *statusWrap = new QWidget(m_table);
        auto *statusLayout = new QHBoxLayout(statusWrap);
        statusLayout->setContentsMargins(8, 0, 8, 0);
        auto *badge = new Badge(entry.status, statusVariant(entry.status), statusWrap);
        statusLayout->addWidget(badge);
        statusLayout->addStretch();
        m_table->setCellWidget(row, 6, statusWrap);

        auto *actions = new QWidget(m_table);
        auto *actionsLayout = new QHBoxLayout(actions);
        actionsLayout->setContentsMargins(6, 0, 6, 0);
        actionsLayout->setSpacing(2);

        auto *copyButton = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                            QString(), actions);
        copyButton->setObjectName(QStringLiteral("IconButton"));
        copyButton->setFixedSize(30, 28);
        copyButton->setToolTip(QStringLiteral("复制启动命令"));
        copyButton->setCursor(Qt::PointingHandCursor);
        connect(copyButton, &QPushButton::clicked, this, [this, entry]() {
            QGuiApplication::clipboard()->setText(entry.command);
            emit toastRequested(QStringLiteral("启动命令已复制到剪贴板。"));
        });

        auto *replayButton = new QPushButton(Icons::icon(Icons::Glyph::Restart, Theme::Muted, 14),
                                              QString(), actions);
        replayButton->setObjectName(QStringLiteral("IconButton"));
        replayButton->setFixedSize(30, 28);
        replayButton->setToolTip(QStringLiteral("使用相同配置重新启动"));
        replayButton->setCursor(Qt::PointingHandCursor);
        connect(replayButton, &QPushButton::clicked, this, [this, entry]() {
            emit replayRequested(entry);
        });

        actionsLayout->addWidget(copyButton);
        actionsLayout->addWidget(replayButton);
        actionsLayout->addStretch();
        m_table->setCellWidget(row, 7, actions);
    }

    m_table->resizeRowsToContents();
    for (int row = 0; row < m_table->rowCount(); ++row)
        m_table->setRowHeight(row, 52);
}

QString HistoryPage::statusVariant(const QString &status) const
{
    if (status == QStringLiteral("运行中"))
        return QStringLiteral("success");
    if (status == QStringLiteral("启动中"))
        return QStringLiteral("warning");
    if (status == QStringLiteral("启动失败") || status == QStringLiteral("异常退出")
        || status == QStringLiteral("异常结束"))
        return QStringLiteral("danger");
    if (status == QStringLiteral("已分离"))
        return QStringLiteral("warning");
    return QStringLiteral("muted");
}

void HistoryPage::exportCsv()
{
    const QString defaultPath = QDir::homePath() + QStringLiteral("/llamacpp-history.csv");
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出历史记录"),
                                                     defaultPath,
                                                     QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QString error;
    if (m_store->exportCsv(path, &error)) {
        emit toastRequested(QStringLiteral("历史记录已导出。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：%1").arg(error));
    }
}

void HistoryPage::clearHistory()
{
    if (m_store->entries().isEmpty())
        return;

    QMessageBox box(QMessageBox::Question, QStringLiteral("清空历史记录"),
                    QStringLiteral("确定要清空全部启动记录吗？此操作不可恢复。"),
                    QMessageBox::NoButton, this);
    QPushButton *confirm = box.addButton(QStringLiteral("清空"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != confirm)
        return;

    m_store->clear();
    emit toastRequested(QStringLiteral("历史记录已清空。"));
}
