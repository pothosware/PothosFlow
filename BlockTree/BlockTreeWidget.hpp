// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QTreeWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QList>
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
    void blockDescEvent(const QJsonObject &, bool);

public slots:
    void handleBlockDescUpdate(const QJsonArray &blockDescs);

    void handleFilter(const QString &filter);

private slots:
    void handleFilterTimerExpired(void);

    void handleSelectionChange(void);

    void handleItemDoubleClicked(QTreeWidgetItem *item, int);

private:

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void populate(void);

    bool blockDescMatchesFilter(const QJsonObject &blockDesc);

    //qt6 changed the function signature to be a reference
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    #define mimeDataRef(name) name
    #else
    #define mimeDataRef(name) &name
    #endif

    QMimeData *mimeData(const QList<QTreeWidgetItem *> mimeDataRef(items)) const override;

    GraphEditorTabs *_editorTabs;
    QString _filter;
    QTimer *_filttimer;
    QPoint _dragStartPos;
    QTreeWidgetItem *_dragItem;
    QJsonArray _blockDescs;
    std::map<QString, BlockTreeWidgetItem *> _rootNodes;
};
