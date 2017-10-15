// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "BlockTree/BlockTreeDock.hpp"
#include "BlockTree/BlockTreeWidget.hpp"
#include "BlockTree/BlockCache.hpp"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDockWidget>
#include <cassert>
#include <QAction>

static BlockTreeDock *globalBlockTreeDock = nullptr;

BlockTreeDock *BlockTreeDock::global(void)
{
    return globalBlockTreeDock;
}

BlockTreeDock::BlockTreeDock(QWidget *parent, BlockCache *blockCache, GraphEditorTabs *editorTabs):
    QDockWidget(parent),
    _searchBox(new QLineEdit(this))
{
    globalBlockTreeDock = this;
    this->setObjectName("BlockTreeDock");
    this->setWindowTitle(tr("Block Tree"));
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    this->setWidget(new QWidget(this));

    auto layout = new QVBoxLayout(this->widget());
    this->widget()->setLayout(layout);

    _searchBox->setPlaceholderText(tr("Search blocks"));
    #if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    _searchBox->setClearButtonEnabled(true);
    #endif
    layout->addWidget(_searchBox);

    _blockTree = new BlockTreeWidget(this->widget(), editorTabs);
    connect(blockCache, SIGNAL(blockDescUpdate(const QJsonArray &)),
        _blockTree, SLOT(handleBlockDescUpdate(const QJsonArray &)));
    connect(_blockTree, SIGNAL(blockDescEvent(const QJsonObject &, bool)),
        this, SLOT(handleBlockDescEvent(const QJsonObject &, bool)));
    connect(_searchBox, &QLineEdit::textChanged, _blockTree, &BlockTreeWidget::handleFilter);
    layout->addWidget(_blockTree);

    _addButton = new QPushButton(makeIconFromTheme("list-add"), "Add Block", this->widget());
    layout->addWidget(_addButton);
    connect(_addButton, &QPushButton::released, this, &BlockTreeDock::handleAdd);
    _addButton->setEnabled(false); //default disabled
}

void BlockTreeDock::activateFind(void)
{
    this->show();
    this->raise();
    //on ctrl-f or edit:find, set focus on search window and select all text
    _searchBox->setFocus();
    _searchBox->selectAll();
    _addButton->setEnabled(false);
}

void BlockTreeDock::handleAdd(void)
{
    emit addBlockEvent(_blockDesc);
}

void BlockTreeDock::handleBlockDescEvent(const QJsonObject &blockDesc, bool add)
{
    _blockDesc = blockDesc;
    _addButton->setEnabled(not blockDesc.isEmpty());
    if (add) emit addBlockEvent(_blockDesc);
}
