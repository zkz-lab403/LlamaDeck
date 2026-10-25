#include "KnowledgePage.h"

#include "Icons.h"
#include "KnowledgeStore.h"
#include "Theme.h"
#include "Widgets.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextOption>
#include <QVBoxLayout>

namespace {

QString formatSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}

} // namespace

KnowledgePage::KnowledgePage(KnowledgeStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    buildUi();
    connect(m_store, &KnowledgeStore::changed, this, &KnowledgePage::reload);
    reload();
}

void KnowledgePage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 10, 32, 24);
    layout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);
    splitter->addWidget(buildDocumentCard());
    splitter->addWidget(buildSearchCard());
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({760, 420});

    layout->addWidget(splitter, 1);
}

QWidget *KnowledgePage::buildDocumentCard()
{
    auto *card = makeCard(this);
    card->setMinimumWidth(480);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    titleBox->addWidget(makeLabel(QStringLiteral("知识库文档"), QStringLiteral("SectionTitle"), card));
    m_countLabel = makeLabel(QStringLiteral("0 个文档 · 0 个片段"), QStringLiteral("Hint"), card);
    titleBox->addWidget(m_countLabel);
    header->addLayout(titleBox);
    header->addStretch();

    auto *addButton = new QPushButton(Icons::icon(Icons::Glyph::Plus, Qt::white, 14),
                                       QStringLiteral("添加文件"), card);
    addButton->setObjectName(QStringLiteral("Primary"));
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setMinimumHeight(36);
    connect(addButton, &QPushButton::clicked, this, &KnowledgePage::addFiles);

    m_clearButton = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Muted, 14),
                                     QStringLiteral("清空"), card);
    m_clearButton->setObjectName(QStringLiteral("Ghost"));
    m_clearButton->setCursor(Qt::PointingHandCursor);
    m_clearButton->setMinimumHeight(36);
    connect(m_clearButton, &QPushButton::clicked, this, &KnowledgePage::clearAll);

    header->addWidget(m_clearButton);
    header->addWidget(addButton);
    layout->addLayout(header);

    m_stack = new QStackedWidget(card);

    m_table = new QTableWidget(m_stack);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("文档"), QStringLiteral("片段"),
                                        QStringLiteral("大小"), QStringLiteral("添加时间"),
                                        QStringLiteral("操作")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i <= 4; ++i)
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 62);
    m_table->setColumnWidth(2, 78);
    m_table->setColumnWidth(3, 118);
    m_table->setColumnWidth(4, 52);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(50);
    m_table->horizontalHeader()->setFixedHeight(36);
    m_table->setShowGrid(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setWordWrap(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_deleteButton->setEnabled(m_table->currentRow() >= 0);
    });

    m_stack->addWidget(m_table);
    m_stack->addWidget(buildEmptyState());
    layout->addWidget(m_stack, 1);

    auto *footer = new QHBoxLayout();
    m_chunkLabel = makeLabel(QStringLiteral("支持 txt / md / csv / json 等纯文本，单文件最大 1 MB"),
                             QStringLiteral("Hint"), card);
    m_chunkLabel->setWordWrap(true);
    footer->addWidget(m_chunkLabel, 1);

    m_deleteButton = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Danger, 13),
                                      QStringLiteral("删除所选"), card);
    m_deleteButton->setObjectName(QStringLiteral("Ghost"));
    m_deleteButton->setCursor(Qt::PointingHandCursor);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setMinimumHeight(30);
    connect(m_deleteButton, &QPushButton::clicked, this, &KnowledgePage::removeSelected);
    footer->addWidget(m_deleteButton);
    layout->addLayout(footer);

    return card;
}

QWidget *KnowledgePage::buildSearchCard()
{
    auto *card = makeCard(this);
    card->setMinimumWidth(360);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(1);
    titleBox->addWidget(makeLabel(QStringLiteral("检索测试"), QStringLiteral("SectionTitle"), card));
    titleBox->addWidget(makeLabel(QStringLiteral("输入问题，预览会注入对话的片段"),
                                  QStringLiteral("Hint"), card));
    header->addLayout(titleBox);
    header->addStretch();

    auto *copyButton = new QPushButton(Icons::icon(Icons::Glyph::Copy, Theme::Muted, 14),
                                        QString(), card);
    copyButton->setObjectName(QStringLiteral("IconButton"));
    copyButton->setFixedSize(30, 26);
    copyButton->setCursor(Qt::PointingHandCursor);
    copyButton->setToolTip(QStringLiteral("复制检索结果"));
    connect(copyButton, &QPushButton::clicked, this, &KnowledgePage::copyResults);
    header->addWidget(copyButton, 0, Qt::AlignTop);
    layout->addLayout(header);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);
    m_search = new QLineEdit(card);
    m_search->setPlaceholderText(QStringLiteral("例如：这份文档讲了什么？"));
    m_search->setMinimumHeight(36);
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::returnPressed, this, &KnowledgePage::runSearch);

    auto *searchButton = new QPushButton(Icons::icon(Icons::Glyph::Search, Qt::white, 14),
                                          QStringLiteral("检索"), card);
    searchButton->setObjectName(QStringLiteral("Primary"));
    searchButton->setCursor(Qt::PointingHandCursor);
    searchButton->setMinimumSize(84, 36);
    connect(searchButton, &QPushButton::clicked, this, &KnowledgePage::runSearch);

    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(searchButton);
    layout->addLayout(searchRow);

    m_searchMeta = makeLabel(QStringLiteral("尚未检索"), QStringLiteral("Hint"), card);
    layout->addWidget(m_searchMeta);

    m_results = new QPlainTextEdit(card);
    m_results->setObjectName(QStringLiteral("KnowledgeResults"));
    m_results->setReadOnly(true);
    m_results->setPlaceholderText(QStringLiteral("检索结果会显示在这里…"));
    m_results->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->addWidget(m_results, 1);

    auto *hint = makeLabel(QStringLiteral("本地关键词匹配（中文按二字词切分），无需嵌入模型"),
                            QStringLiteral("Hint"), card);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    return card;
}

QWidget *KnowledgePage::buildEmptyState()
{
    auto *empty = new QWidget();
    auto *layout = new QVBoxLayout(empty);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->addStretch();

    auto *icon = new QLabel(empty);
    icon->setPixmap(Icons::pixmap(Icons::Glyph::Book, QColor(78, 89, 108), 44));
    icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon);

    auto *title = makeLabel(QStringLiteral("还没有文档"), QStringLiteral("SectionTitle"), empty);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *hint = makeLabel(QStringLiteral("导入本地文本资料，对话时会自动检索相关片段。"),
                            QStringLiteral("Hint"), empty);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *button = new QPushButton(Icons::icon(Icons::Glyph::Plus, Qt::white, 14),
                                    QStringLiteral("添加文件"), empty);
    button->setObjectName(QStringLiteral("Primary"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(36);
    connect(button, &QPushButton::clicked, this, &KnowledgePage::addFiles);
    layout->addWidget(button, 0, Qt::AlignHCenter);
    layout->addStretch();
    return empty;
}

void KnowledgePage::reload()
{
    const QVector<KnowledgeDocument> documents = m_store->documents();
    m_countLabel->setText(QStringLiteral("%1 个文档 · %2 个片段")
                             .arg(documents.size())
                             .arg(m_store->chunkCount()));
    m_stack->setCurrentIndex(documents.isEmpty() ? 1 : 0);
    m_clearButton->setEnabled(!documents.isEmpty());

    m_table->clearContents();
    m_table->setRowCount(documents.size());

    for (int row = 0; row < documents.size(); ++row) {
        const KnowledgeDocument &document = documents.at(row);

        auto *nameItem = new QTableWidgetItem(document.title);
        nameItem->setData(Qt::UserRole, document.id);
        nameItem->setToolTip(document.sourcePath);
        QFont nameFont = nameItem->font();
        nameFont.setWeight(QFont::DemiBold);
        nameItem->setFont(nameFont);
        m_table->setItem(row, 0, nameItem);

        auto *chunkItem = new QTableWidgetItem(QString::number(document.chunkCount));
        chunkItem->setForeground(QColor(150, 161, 181));
        chunkItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, chunkItem);

        auto *sizeItem = new QTableWidgetItem(formatSize(document.sizeBytes));
        sizeItem->setForeground(QColor(150, 161, 181));
        sizeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, sizeItem);

        auto *timeItem = new QTableWidgetItem(
            document.addedAt.toString(QStringLiteral("MM-dd HH:mm")));
        timeItem->setForeground(QColor(130, 142, 162));
        timeItem->setTextAlignment(Qt::AlignCenter);
        timeItem->setToolTip(document.addedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        m_table->setItem(row, 3, timeItem);

        auto *remove = new QPushButton(Icons::icon(Icons::Glyph::Trash, Theme::Muted, 13),
                                        QString(), m_table);
        remove->setObjectName(QStringLiteral("IconButton"));
        remove->setFixedSize(30, 26);
        remove->setCursor(Qt::PointingHandCursor);
        remove->setToolTip(QStringLiteral("删除该文档"));
        connect(remove, &QPushButton::clicked, this, [this, id = document.id]() {
            m_store->removeDocument(id);
            emit toastRequested(QStringLiteral("文档已从知识库移除。"));
        });
        m_table->setCellWidget(row, 4, remove);
    }

    if (documents.isEmpty()) {
        m_searchMeta->setText(QStringLiteral("尚未检索"));
        m_results->clear();
    }
}

void KnowledgePage::addFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("导入知识库文档"), QString(),
        QStringLiteral("文本文件 (*.txt *.md *.markdown *.csv *.json *.log *.ini *.yaml *.yml);;所有文件 (*)"));
    if (paths.isEmpty())
        return;

    int failed = 0;
    const int imported = m_store->importFiles(paths, &failed);
    if (imported > 0) {
        emit toastRequested(QStringLiteral("已导入 %1 个文档%2")
                                .arg(imported)
                                .arg(failed > 0
                                         ? QStringLiteral("，%1 个未导入").arg(failed)
                                         : QString()));
    } else {
        emit toastRequested(m_store->lastError().isEmpty()
                                ? QStringLiteral("未导入任何文档。")
                                : m_store->lastError());
    }
}

void KnowledgePage::removeSelected()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item)
        return;
    m_store->removeDocument(item->data(Qt::UserRole).toString());
    emit toastRequested(QStringLiteral("文档已从知识库移除。"));
}

void KnowledgePage::clearAll()
{
    if (m_store->documentCount() == 0)
        return;

    const auto answer = QMessageBox::question(
        this, QStringLiteral("清空知识库"),
        QStringLiteral("确定删除全部 %1 个文档吗？此操作不可撤销。")
            .arg(m_store->documentCount()));
    if (answer == QMessageBox::Yes) {
        m_store->clearAll();
        emit toastRequested(QStringLiteral("知识库已清空。"));
    }
}

void KnowledgePage::runSearch()
{
    const QString query = m_search->text().trimmed();
    if (query.isEmpty())
        return;

    QElapsedTimer timer;
    timer.start();
    const QVector<KnowledgeHit> hits = m_store->search(query, 5);
    const qint64 elapsed = timer.elapsed();

    m_searchMeta->setText(QStringLiteral("命中 %1 段 · 用时 %2 ms")
                              .arg(hits.size())
                              .arg(elapsed));

    if (hits.isEmpty()) {
        m_results->setPlainText(QStringLiteral("没有找到相关内容。试试更短、更具体的关键词。"));
        return;
    }

    QString text;
    for (int i = 0; i < hits.size(); ++i) {
        const KnowledgeHit &hit = hits.at(i);
        text += QStringLiteral("[%1] %2  ·  相关度 %3\n%4\n")
                    .arg(i + 1)
                    .arg(hit.title)
                    .arg(hit.score, 0, 'f', 2)
                    .arg(hit.text);
        if (i + 1 < hits.size())
            text += QStringLiteral("\n────────────────────────\n\n");
    }
    m_results->setPlainText(text);
}

void KnowledgePage::copyResults()
{
    if (m_results->toPlainText().isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_results->toPlainText());
    emit toastRequested(QStringLiteral("检索结果已复制到剪贴板。"));
}
