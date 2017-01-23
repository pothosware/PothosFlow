// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTreeWidget>
#include <QString>
#include <QList>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <map>

class QTimer;
class QMimeData;
class BlockTreeWidgetItem;
class GraphEditorTabs;

//! The tree widget part of the block tree top window
class BlockTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:

    BlockTreeWidget(QWidget *parent, GraphEditorTabs *editorTabs);

signals:
    void blockDescEvent(const Poco::JSON::Object::Ptr &, bool);

public slots:
    void handleBlockDescUpdate(const Poco::JSON::Array::Ptr &blockDescs);

private slots:
    void handleFilterTimerExpired(void);

    void handleFilter(const QString &filter);

    void handleSelectionChange(void);

    void handleItemDoubleClicked(QTreeWidgetItem *item, int);

private:

    void mousePressEvent(QMouseEvent *event);

    void mouseMoveEvent(QMouseEvent *event);

    void populate(void);

    bool blockDescMatchesFilter(const Poco::JSON::Object::Ptr &blockDesc);

    QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const;

    GraphEditorTabs *_editorTabs;
    QString _filter;
    QTimer *_filttimer;
    QPoint _dragStartPos;
    QTreeWidgetItem *_dragItem;
    Poco::JSON::Array::Ptr _blockDescs;
    std::map<std::string, BlockTreeWidgetItem *> _rootNodes;
};
