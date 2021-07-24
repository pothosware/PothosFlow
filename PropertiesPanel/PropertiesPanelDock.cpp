// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "PropertiesPanel/PropertiesPanelDock.hpp"
#include "PropertiesPanel/GraphPropertiesPanel.hpp"
#include "PropertiesPanel/BlockPropertiesPanel.hpp"
#include "PropertiesPanel/BreakerPropertiesPanel.hpp"
#include "PropertiesPanel/ConnectionPropertiesPanel.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphObjects/GraphBreaker.hpp"
#include "GraphObjects/GraphConnection.hpp"
#include "GraphObjects/GraphWidget.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>

static PropertiesPanelDock *globalPropertiesPanelDock = nullptr;

PropertiesPanelDock *PropertiesPanelDock::global(void)
{
    return globalPropertiesPanelDock;
}

PropertiesPanelDock::PropertiesPanelDock(QWidget *parent):
    QDockWidget(parent),
    _propertiesPanel(nullptr),
    _scroll(new QScrollArea(this)),
    _commitButton(nullptr),
    _cancelButton(nullptr)
{
    globalPropertiesPanelDock = this;
    this->setObjectName("PropertiesPanelDock");
    this->setWindowTitle(tr("Properties Panel"));
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    this->setWidget(new QWidget(this));

    //master layout for this widget
    auto layout = new QVBoxLayout(this->widget());

    //setup the scroller
    _scroll->setWidgetResizable(true);
    layout->addWidget(_scroll);

    //setup the buttons
    {
        auto buttonLayout = new QHBoxLayout();
        layout->addLayout(buttonLayout);
        _commitButton = new QPushButton(makeIconFromTheme("dialog-ok-apply"), tr("Commit"), this);
        connect(_commitButton, &QPushButton::pressed, this, &PropertiesPanelDock::handleDeletePanel);
        buttonLayout->addWidget(_commitButton);
        _cancelButton = new QPushButton(makeIconFromTheme("dialog-cancel"), tr("Cancel"), this);
        connect(_cancelButton, &QPushButton::pressed, this, &PropertiesPanelDock::handleDeletePanel);
        buttonLayout->addWidget(_cancelButton);
    }
}

void PropertiesPanelDock::launchEditor(QObject *obj)
{
    //clear old panel
    if (_propertiesPanel)
    {
        if (_currentGraphObject) emit this->replacePanel();
        delete _propertiesPanel;
    }

    //extract the graph object
    auto graph = qobject_cast<GraphEditor *>(obj);
    auto block = qobject_cast<GraphBlock *>(obj);
    auto breaker = qobject_cast<GraphBreaker *>(obj);
    auto connection = qobject_cast<GraphConnection *>(obj);
    auto widget = qobject_cast<GraphWidget *>(obj);
    auto graphObject = qobject_cast<GraphObject *>(obj);
    auto editor = (graphObject != nullptr)? graphObject->draw()->getGraphEditor() : graph;

    if (widget != nullptr) block = widget->getGraphBlock();
    if (graph != nullptr) installNewPanel(new GraphPropertiesPanel(graph, this), editor);
    else if (block != nullptr) installNewPanel(new BlockPropertiesPanel(block, this), editor);
    else if (breaker != nullptr) installNewPanel(new BreakerPropertiesPanel(breaker, this), editor);
    else if (connection != nullptr and connection->isSignalOrSlot()) installNewPanel(new ConnectionPropertiesPanel(connection, this), editor);
    else return;

    //set the widget and make the entire dock visible
    _currentGraphObject = obj;
    _scroll->setWidget(_propertiesPanel);
    this->show();
    this->raise();
}

template <typename T>
void PropertiesPanelDock::installNewPanel(T *panel, GraphEditor *editor)
{
    _propertiesPanel = panel;

    //connect panel signals and slots into dock events
    connect(panel, &T::destroyed, this, &PropertiesPanelDock::handlePanelDestroyed);
    connect(this, &PropertiesPanelDock::replacePanel, panel, &T::handleCommit);
    connect(_commitButton, &QPushButton::pressed, panel, &T::handleCommit);
    connect(_cancelButton, &QPushButton::pressed, panel, &T::handleCancel);

    //connect state change to the graph editor
    connect(panel, &T::stateChanged, editor, &GraphEditor::handleStateChange);
}

void PropertiesPanelDock::handlePanelDestroyed(QObject *)
{
    this->hide();
}

void PropertiesPanelDock::handleDeletePanel(void)
{
    if (_propertiesPanel) _propertiesPanel->deleteLater();
}
