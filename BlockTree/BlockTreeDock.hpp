// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QDockWidget>
#include <QJsonObject>

class QPushButton;
class QLineEdit;
class BlockTreeWidget;
class BlockCache;
class GraphEditorTabs;

//! A top level dock widget with a block tree top window
class BlockTreeDock : public QDockWidget
{
    Q_OBJECT
public:

    //! Get access to the global block tree dock
    static BlockTreeDock *global(void);

    BlockTreeDock(QWidget *parent, BlockCache *blockCache, GraphEditorTabs *editorTabs);

signals:
    void addBlockEvent(const QJsonObject &);

public slots:
    void activateFind(void);

private slots:
    void handleAdd(void);

    void handleBlockDescEvent(const QJsonObject &blockDesc, bool add);

private:
    QPushButton *_addButton;
    QLineEdit *_searchBox;
    QJsonObject _blockDesc;
    BlockTreeWidget *_blockTree;
};
