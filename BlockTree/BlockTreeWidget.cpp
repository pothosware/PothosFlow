// Copyright (c) 2014-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "BlockTree/BlockTreeWidget.hpp"
#include "BlockTree/BlockTreeWidgetItem.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphEditor/GraphEditorTabs.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include <QTreeWidgetItem>
#include <QApplication>
#include <QDrag>
#include <QMouseEvent>
#include <QMimeData>
#include <QTimer>
#include <QPainter>
#include <QJsonDocument>
#include <memory>

static const long UPDATE_TIMER_MS = 500;

BlockTreeWidget::BlockTreeWidget(QWidget *parent, GraphEditorTabs *editorTabs):
    QTreeWidget(parent),
    _editorTabs(editorTabs),
    _filttimer(new QTimer(this)),
    _dragItem(nullptr)
{
    QStringList columnNames;
    columnNames.push_back(tr("Available Blocks"));
    this->setColumnCount(columnNames.size());
    this->setHeaderLabels(columnNames);

    _filttimer->setSingleShot(true);
    _filttimer->setInterval(UPDATE_TIMER_MS);

    connect(this, &BlockTreeWidget::itemSelectionChanged, this, &BlockTreeWidget::handleSelectionChange);
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(handleItemDoubleClicked(QTreeWidgetItem *, int)));
    connect(_filttimer, &QTimer::timeout, this, &BlockTreeWidget::handleFilterTimerExpired);
}

void BlockTreeWidget::mousePressEvent(QMouseEvent *event)
{
    this->setFocus();
    //if the item under the mouse is the bottom of the tree (a block, not category)
    //then we set a dragstartpos
    if (not itemAt(event->pos()))
    {
        return QTreeWidget::mousePressEvent(event);
    }
    if (itemAt(event->pos())->childCount() == 0 and event->button() == Qt::LeftButton)
    {
        _dragStartPos = event->pos();
        _dragItem = itemAt(_dragStartPos);
    }
    else _dragItem = nullptr;
    //pass the event along
    QTreeWidget::mousePressEvent(event);
}

void BlockTreeWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (not (event->buttons() & Qt::LeftButton))
    {
        return QTreeWidget::mouseMoveEvent(event);
    }
    if ((event->pos() - _dragStartPos).manhattanLength() < QApplication::startDragDistance())
    {
        return QTreeWidget::mouseMoveEvent(event);
    }

    //do we have a valid item to drag?
    if (_dragItem == nullptr) return;

    //get the block data
    auto blockItem = dynamic_cast<BlockTreeWidgetItem *>(_dragItem);
    if (blockItem->getBlockDesc().isEmpty()) return;

    //create a block object to render the image
    auto draw = _editorTabs->getCurrentGraphEditor()->getCurrentGraphDraw();
    std::unique_ptr<GraphBlock> renderBlock(new GraphBlock(draw));
    renderBlock->setBlockDesc(blockItem->getBlockDesc());
    renderBlock->prerender(); //precalculate so we can get bounds
    const auto bounds = renderBlock->boundingRect();

    //draw the block's preview onto a mini pixmap
    QPixmap pixmap(bounds.size().toSize()+QSize(2,2));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.translate(-bounds.topLeft()+QPoint(1,1));
    //painter.scale(zoomScale, zoomScale); //TODO get zoomscale from draw
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::HighQualityAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderBlock->render(painter);
    renderBlock.reset();
    painter.end();

    //create the drag object
    auto mimeData = new QMimeData();
    const QJsonDocument jsonDoc(blockItem->getBlockDesc());
    mimeData->setData("binary/json/pothos_block", jsonDoc.toBinaryData());
    auto drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(-bounds.topLeft().toPoint());
    drag->exec(Qt::CopyAction | Qt::MoveAction);
}

void BlockTreeWidget::handleBlockDescUpdate(const QJsonArray &blockDescs)
{
    _blockDescs = blockDescs;
    this->populate();
    this->resizeColumnToContents(0);
}

void BlockTreeWidget::handleFilterTimerExpired(void)
{
    this->clear();
    _rootNodes.clear();
    this->populate();
    for (auto item : this->findItems("", Qt::MatchContains, 0)) item->setExpanded(not _filter.isEmpty());
}

void BlockTreeWidget::handleFilter(const QString &filter)
{
    _filter = filter;
    //use the timer if search gets expensive.
    //otherwise just call handleFilterTimerExpired here.
    //_filttimer->start(UPDATE_TIMER_MS);
    handleFilterTimerExpired();
}

void BlockTreeWidget::handleSelectionChange(void)
{
    for (auto item : this->selectedItems())
    {
        auto b = dynamic_cast<BlockTreeWidgetItem *>(item);
        if (b != nullptr) emit blockDescEvent(b->getBlockDesc(), false);
    }
}

void BlockTreeWidget::handleItemDoubleClicked(QTreeWidgetItem *item, int)
{
    auto b = dynamic_cast<BlockTreeWidgetItem *>(item);
    if (b != nullptr) emit blockDescEvent(b->getBlockDesc(), true);
}

void BlockTreeWidget::populate(void)
{
    for (const auto &blockDescVal : _blockDescs)
    {
        const auto blockDesc = blockDescVal.toObject();
        if (not this->blockDescMatchesFilter(blockDesc)) continue;
        const auto path = blockDesc["path"].toString();
        const auto name = blockDesc["name"].toString();
        for (const auto &categoryVal : blockDesc["categories"].toArray())
        {
            const auto category = categoryVal.toString().mid(1);
            const auto key = category.mid(0, category.indexOf('/'));
            if (_rootNodes.find(key) == _rootNodes.end()) _rootNodes[key] = new BlockTreeWidgetItem(this, key);
            _rootNodes[key]->load(blockDesc, category + "/" + name);
        }
    }

    //sort the columns alphabetically
    this->sortByColumn(0, Qt::AscendingOrder);

    emit this->blockDescEvent(QJsonObject(), false); //unselect
}

bool BlockTreeWidget::blockDescMatchesFilter(const QJsonObject &blockDesc)
{
    if (_filter.isEmpty()) return true;

    const auto path = blockDesc["path"].toString();
    const auto name = blockDesc["name"].toString();

    //construct a candidate string from path, name, categories, and keywords.
    QString candidate = path+name;
    for (const auto &categoryVal : blockDesc["categories"].toArray())
    {
        candidate += categoryVal.toString();
    }
    for (const auto &keywordVal : blockDesc["keywords"].toArray())
    {
        candidate += keywordVal.toString();
    }

    //reject if filter string not found in candidate
    candidate = candidate.toLower();
    const auto searchToken = _filter.toLower();
    return (candidate.indexOf(searchToken) != -1);
}

QMimeData *BlockTreeWidget::mimeData(const QList<QTreeWidgetItem *> items) const
{
    for (auto item : items)
    {
        auto b = dynamic_cast<BlockTreeWidgetItem *>(item);
        if (b == nullptr) continue;
        auto mimeData = new QMimeData();
        const QJsonDocument jsonDoc(b->getBlockDesc());
        mimeData->setData("binary/json/pothos_block", jsonDoc.toBinaryData());
        return mimeData;
    }
    return QTreeWidget::mimeData(items);
}
