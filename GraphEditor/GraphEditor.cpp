// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "EvalEngine/EvalEngine.hpp"
#include "GraphEditor/GraphActionsDock.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "GraphEditor/GraphDraw.hpp"
#include "GraphEditor/Constants.hpp"
#include "GraphObjects/GraphBlock.hpp"
#include "GraphObjects/GraphBreaker.hpp"
#include "GraphObjects/GraphConnection.hpp"
#include "GraphObjects/GraphWidget.hpp"
#include "BlockTree/BlockTreeDock.hpp"
#include "AffinitySupport/AffinityZonesDock.hpp"
#include "MainWindow/MainActions.hpp"
#include "MainWindow/MainMenu.hpp"
#include "MainWindow/MainSplash.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTabBar>
#include <QInputDialog>
#include <QAction>
#include <QMenu>
#include <QSignalMapper>
#include <QDockWidget>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QRegExp>
#include <QTimer>
#include <QUuid>
#include <QFileInfo>
#include <iostream>
#include <cassert>
#include <set>
#include <Pothos/Exception.hpp>
#include <algorithm> //min/max

//! How often to check for graph widget changes
static const size_t POLL_WIDGET_CHANGES_MS = 1000;

GraphEditor::GraphEditor(QWidget *parent):
    QTabWidget(parent),
    _logger(Poco::Logger::get("PothosFlow.GraphEditor")),
    _parentTabWidget(dynamic_cast<QTabWidget *>(parent)),
    _moveGraphObjectsMapper(new QSignalMapper(this)),
    _insertGraphWidgetsMapper(new QSignalMapper(this)),
    _stateManager(new GraphStateManager(this)),
    _evalEngine(new EvalEngine(this)),
    _isTopologyActive(false),
    _pollWidgetTimer(new QTimer(this)),
    _autoActivate(false),
    _lockTopology(false)
{
    this->setDocumentMode(true);
    this->setMovable(true);
    this->setUsesScrollButtons(true);
    this->setTabPosition(QTabWidget::West);
    this->makeDefaultPage();
    this->tabBar()->setStyleSheet("font-size:8pt;");
    auto actions = MainActions::global();
    auto mainMenu = MainMenu::global();

    //connect handlers that work at the page-level of control
    connect(QApplication::clipboard(), SIGNAL(dataChanged(void)), this, SLOT(handleClipboardDataChange(void)));
    connect(this, &GraphEditor::currentChanged, this, &GraphEditor::handleCurrentChanged);
    connect(_stateManager, &GraphStateManager::newStateSelected, this, &GraphEditor::handleResetState);
    connect(actions->createGraphPageAction, SIGNAL(triggered(void)), this, SLOT(handleCreateGraphPage(void)));
    connect(actions->renameGraphPageAction, SIGNAL(triggered(void)), this, SLOT(handleRenameGraphPage(void)));
    connect(actions->deleteGraphPageAction, SIGNAL(triggered(void)), this, SLOT(handleDeleteGraphPage(void)));
    connect(actions->inputBreakerAction, SIGNAL(triggered(void)), this, SLOT(handleCreateInputBreaker(void)));
    connect(actions->outputBreakerAction, SIGNAL(triggered(void)), this, SLOT(handleCreateOutputBreaker(void)));
    connect(actions->cutAction, SIGNAL(triggered(void)), this, SLOT(handleCut(void)));
    connect(actions->copyAction, SIGNAL(triggered(void)), this, SLOT(handleCopy(void)));
    connect(actions->pasteAction, SIGNAL(triggered(void)), this, SLOT(handlePaste(void)));
    connect(BlockTreeDock::global(), SIGNAL(addBlockEvent(const QJsonObject &)), this, SLOT(handleAddBlock(const QJsonObject &)));
    connect(actions->selectAllAction, SIGNAL(triggered(void)), this, SLOT(handleSelectAll(void)));
    connect(actions->deleteAction, SIGNAL(triggered(void)), this, SLOT(handleDelete(void)));
    connect(actions->rotateLeftAction, SIGNAL(triggered(void)), this, SLOT(handleRotateLeft(void)));
    connect(actions->rotateRightAction, SIGNAL(triggered(void)), this, SLOT(handleRotateRight(void)));
    connect(actions->objectPropertiesAction, SIGNAL(triggered(void)), this, SLOT(handleObjectProperties(void)));
    connect(actions->graphPropertiesAction, SIGNAL(triggered(void)), this, SLOT(handleGraphProperties(void)));
    connect(actions->zoomInAction, SIGNAL(triggered(void)), this, SLOT(handleZoomIn(void)));
    connect(actions->zoomOutAction, SIGNAL(triggered(void)), this, SLOT(handleZoomOut(void)));
    connect(actions->zoomOriginalAction, SIGNAL(triggered(void)), this, SLOT(handleZoomOriginal(void)));
    connect(actions->undoAction, SIGNAL(triggered(void)), this, SLOT(handleUndo(void)));
    connect(actions->redoAction, SIGNAL(triggered(void)), this, SLOT(handleRedo(void)));
    connect(actions->enableAction, SIGNAL(triggered(void)), this, SLOT(handleEnable(void)));
    connect(actions->disableAction, SIGNAL(triggered(void)), this, SLOT(handleDisable(void)));
    connect(actions->reevalAction, SIGNAL(triggered(void)), this, SLOT(handleReeval(void)));
    connect(mainMenu->affinityZoneMenu, SIGNAL(zoneClicked(const QString &)), this, SLOT(handleAffinityZoneClicked(const QString &)));
    connect(AffinityZonesDock::global(), SIGNAL(zoneChanged(const QString &)), this, SLOT(handleAffinityZoneChanged(const QString &)));
    connect(actions->showRenderedGraphAction, SIGNAL(triggered(void)), this, SLOT(handleShowRenderedGraphDialog(void)));
    connect(actions->showTopologyStatsAction, SIGNAL(triggered(void)), this, SLOT(handleShowTopologyStatsDialog(void)));
    connect(actions->activateTopologyAction, SIGNAL(toggled(bool)), this, SLOT(handleToggleActivateTopology(bool)));
    connect(actions->showPortNamesAction, SIGNAL(changed(void)), this, SLOT(handleBlockDisplayModeChange(void)));
    connect(actions->eventPortsInlineAction, SIGNAL(changed(void)), this, SLOT(handleBlockDisplayModeChange(void)));
    connect(actions->incrementAction, SIGNAL(triggered(void)), this, SLOT(handleBlockIncrement(void)));
    connect(actions->decrementAction, SIGNAL(triggered(void)), this, SLOT(handleBlockDecrement(void)));
    connect(_moveGraphObjectsMapper, SIGNAL(mapped(int)), this, SLOT(handleMoveGraphObjects(int)));
    connect(_insertGraphWidgetsMapper, SIGNAL(mapped(QObject *)), this, SLOT(handleInsertGraphWidget(QObject *)));
    connect(this, SIGNAL(newTitleSubtext(const QString &)), _parentTabWidget->parent(), SLOT(handleNewTitleSubtext(const QString &)));
    connect(_pollWidgetTimer, &QTimer::timeout, this, &GraphEditor::handlePollWidgetTimer);
    _pollWidgetTimer->start(POLL_WIDGET_CHANGES_MS);
}

GraphEditor::~GraphEditor(void)
{
    //stop the eval engine and its evaluator thread
    this->stopEvaluation();

    //the actions dock owns state manager for display purposes,
    //so delete it here when the graph editor is actually done with it
    delete _stateManager;
}

void GraphEditor::stopEvaluation(void)
{
    //stash the state of all graphical widgets
    for (auto obj : this->getGraphObjects(GRAPH_WIDGET))
    {
        obj->serialize(); //causes internal stashing
    }

    delete _evalEngine;
    _evalEngine = nullptr;
}

void GraphEditor::restartEvaluation(void)
{
    //force all blocks to reload the block description
    for (auto obj : this->getGraphObjects(GRAPH_BLOCK))
    {
        obj->deserialize(obj->serialize());
    }

    _evalEngine = new EvalEngine(this);
    _evalEngine->submitTopology(this->getGraphObjects());
    _evalEngine->submitActivateTopology(_isTopologyActive);
}

QString GraphEditor::newId(const QString &hint, const QStringList &blacklist) const
{
    std::set<QString> allIds;
    for (auto id : blacklist) allIds.insert(id);
    for (auto obj : this->getGraphObjects()) allIds.insert(obj->getId());

    //either use the hint or UUID if blank
    QString idBase = hint;
    if (idBase.isEmpty())
    {
        idBase = QUuid::createUuid().toString();
    }

    //find a reasonable name and index
    size_t index = 0;
    QRegExp rx("(.*)(\\d+)"); rx.indexIn(idBase);
    if (rx.captureCount() == 2 and not rx.cap(1).isEmpty() and not rx.cap(2).isEmpty())
    {
        idBase = rx.cap(1);
        index = rx.cap(2).toInt();
    }

    //loop for a unique ID name
    QString possibleId;
    do
    {
        possibleId = QString("%1%2").arg(idBase).arg(index++);
    } while (allIds.find(possibleId) != allIds.end());

    return possibleId;
}

void GraphEditor::showEvent(QShowEvent *event)
{
    //load our state monitor into the actions dock
    GraphActionsDock::global()->setActiveWidget(_stateManager);

    this->updateGraphEditorMenus();
    this->updateEnabledActions();
    QWidget::showEvent(event);
}

void GraphEditor::updateEnabledActions(void)
{
    if (not this->isVisible()) return;
    auto actions = MainActions::global();

    actions->undoAction->setEnabled(_stateManager->isPreviousAvailable());
    actions->redoAction->setEnabled(_stateManager->isSubsequentAvailable());
    actions->saveAction->setEnabled(not _stateManager->isCurrentSaved());
    actions->reloadAction->setEnabled(not this->getCurrentFilePath().isEmpty());
    actions->exportAction->setEnabled(not this->getCurrentFilePath().isEmpty());
    actions->activateTopologyAction->setChecked(_isTopologyActive);

    actions->enableAction->setEnabled(not _lockTopology);
    actions->disableAction->setEnabled(not _lockTopology);
    actions->cutAction->setEnabled(not _lockTopology);
    actions->pasteAction->setEnabled(not _lockTopology);
    actions->createGraphPageAction->setEnabled(not _lockTopology);
    actions->renameGraphPageAction->setEnabled(not _lockTopology);
    actions->deleteGraphPageAction->setEnabled(not _lockTopology);
    actions->inputBreakerAction->setEnabled(not _lockTopology);
    actions->outputBreakerAction->setEnabled(not _lockTopology);
    actions->rotateLeftAction->setEnabled(not _lockTopology);
    actions->rotateRightAction->setEnabled(not _lockTopology);
    actions->incrementAction->setEnabled(not _lockTopology);
    actions->decrementAction->setEnabled(not _lockTopology);

    //can we paste something from the clipboard?
    auto mimeData = QApplication::clipboard()->mimeData();
    const bool canPaste = mimeData->hasFormat("binary/json/pothos_object_array") and
                      not mimeData->data("binary/json/pothos_object_array").isEmpty();
    actions->pasteAction->setEnabled(canPaste);

    //update window title
    QString subtext = this->getCurrentFilePath();
    if (subtext.isEmpty()) subtext = tr("untitled");
    emit this->newTitleSubtext(tr("Editing ") + subtext);
}

void GraphEditor::handleCurrentChanged(int)
{
    if (not this->isVisible()) return;
    this->updateGraphEditorMenus();
}

void GraphEditor::handleCreateGraphPage(void)
{
    if (not this->isVisible()) return;
    const QString newName = QInputDialog::getText(this, tr("Create page"),
        tr("New page name"), QLineEdit::Normal, tr("untitled"));
    if (newName.isEmpty()) return;
    this->addTab(new GraphDraw(this), newName);
    this->updateGraphEditorMenus();

    handleStateChange(GraphState("document-new", tr("Create graph page ") + newName));
}

void GraphEditor::handleRenameGraphPage(void)
{
    if (not this->isVisible()) return;
    const auto oldName = this->tabText(this->currentIndex());
    const QString newName = QInputDialog::getText(this, tr("Rename page"),
        tr("New page name"), QLineEdit::Normal, oldName);
    if (newName.isEmpty()) return;
    this->setTabText(this->currentIndex(), newName);
    this->updateGraphEditorMenus();

    handleStateChange(GraphState("edit-rename", tr("Rename graph page ") + oldName + " -> " + newName));
}

void GraphEditor::handleDeleteGraphPage(void)
{
    if (not this->isVisible()) return;
    const auto oldName = this->tabText(this->currentIndex());
    this->removeTab(this->currentIndex());
    if (this->count() == 0) this->makeDefaultPage();
    this->updateGraphEditorMenus();

    handleStateChange(GraphState("edit-delete", tr("Delete graph page ") + oldName));
}

GraphConnection *GraphEditor::makeConnection(const GraphConnectionEndpoint &ep0, const GraphConnectionEndpoint &ep1)
{
    //direction check
    if (ep0.getConnectableAttrs().direction == ep1.getConnectableAttrs().direction or
        (ep0.getConnectableAttrs().direction == GRAPH_CONN_INPUT and ep1.getConnectableAttrs().direction == GRAPH_CONN_SLOT) or
        (ep0.getConnectableAttrs().direction == GRAPH_CONN_OUTPUT and ep1.getConnectableAttrs().direction == GRAPH_CONN_SIGNAL) or
        (ep0.getConnectableAttrs().direction == GRAPH_CONN_SLOT and ep1.getConnectableAttrs().direction == GRAPH_CONN_INPUT) or
        (ep0.getConnectableAttrs().direction == GRAPH_CONN_SIGNAL and ep1.getConnectableAttrs().direction == GRAPH_CONN_OUTPUT))
    {
        throw Pothos::Exception("GraphEditor::makeConnection()", "cant connect endpoints of the same direction");
    }

    //duplicate check
    for (auto obj : this->getGraphObjects(GRAPH_CONNECTION))
    {
        auto conn = dynamic_cast<GraphConnection *>(obj);
        assert(conn != nullptr);
        if (
            (conn->getOutputEndpoint() == ep0 and conn->getInputEndpoint() == ep1) or
            (conn->getOutputEndpoint() == ep1 and conn->getInputEndpoint() == ep0)
        ) throw Pothos::Exception("GraphEditor::makeConnection()", "connection already exists");
    }

    auto conn = new GraphConnection(ep0.getObj()->draw());
    conn->setupEndpoint(ep0);
    conn->setupEndpoint(ep1);

    const auto idHint = QString("Connection_%1%2_%3%4").arg(
        conn->getOutputEndpoint().getObj()->getId(),
        conn->getOutputEndpoint().getKey().id,
        conn->getInputEndpoint().getObj()->getId(),
        conn->getInputEndpoint().getKey().id
    );
    conn->setId(this->newId(idHint));
    assert(conn->getInputEndpoint().isValid());
    assert(conn->getOutputEndpoint().isValid());

    return conn;
}

//TODO traverse breakers and find one in the node mass that already exists

static GraphBreaker *findInputBreaker(GraphEditor *editor, const GraphConnectionEndpoint &ep, const QString &signalName)
{
    for (auto obj : editor->getGraphObjects(GRAPH_CONNECTION))
    {
        auto conn = dynamic_cast<GraphConnection *>(obj);
        assert(conn != nullptr);
        if (not (conn->getOutputEndpoint().getObj()->scene() == conn->getInputEndpoint().getObj()->scene())) continue;
        if (not (conn->getOutputEndpoint() == ep)) continue;
        if (not signalName.isEmpty()) //filter by signal name (empty on regular endpoints)
        {
            const auto &pairs = conn->getSigSlotPairs();
            if (not pairs.empty() and pairs.at(0).first != signalName) continue;
        }
        auto breaker = dynamic_cast<GraphBreaker *>(conn->getInputEndpoint().getObj().data());
        if (breaker == nullptr) continue;
        return breaker;
    }
    return nullptr;
}

void GraphEditor::handleMoveGraphObjects(const int index)
{
    if (not this->isVisible()) return;
    if (index >= this->count()) return;
    auto draw = this->getCurrentGraphDraw();
    auto desc = tr("Move %1 to %2").arg(draw->getSelectionDescription(~GRAPH_CONNECTION), this->tabText(index));

    //move all selected objects
    for (auto obj : draw->getObjectsSelected())
    {
        obj->setSelected(false);
        this->getGraphDraw(index)->scene()->addItem(obj);
    }

    //reparent all connections based on endpoints:
    std::vector<GraphConnection *> boundaryConnections;
    for (auto obj : this->getGraphObjects(GRAPH_CONNECTION))
    {
        auto conn = dynamic_cast<GraphConnection *>(obj);
        assert(conn != nullptr);

        //Connection has the same endpoints, so make sure that the parent is corrected to the endpoint
        if (conn->getOutputEndpoint().getObj()->scene() == conn->getInputEndpoint().getObj()->scene())
        {
            if (conn->getOutputEndpoint().getObj()->scene() != conn->scene())
            {
                conn->getInputEndpoint().getObj()->scene()->addItem(conn);
            }
        }

        //otherwise stash it for more processing
        else
        {
            boundaryConnections.push_back(conn);
        }
    }

    //create breakers for output endpoints that have to cross
    for (auto conn : boundaryConnections)
    {
        const auto &epOut = conn->getOutputEndpoint();
        const auto &epIn = conn->getInputEndpoint();

        auto sigSlotPairs = conn->getSigSlotPairs();
        if (sigSlotPairs.empty()) sigSlotPairs.resize(1); //add empty
        for (const auto &sigSlotPair : sigSlotPairs)
        {
            auto breaker = findInputBreaker(this, epOut, sigSlotPair.first);
            if (breaker != nullptr) continue;

            breaker = new GraphBreaker(epOut.getObj()->draw());
            breaker->setInput(true);

            //come up with an id that could visibly represent this connection
            auto name = epOut.getObj()->getId();
            bool isPortNumeric; epOut.getKey().id.toInt(&isPortNumeric);
            if (not sigSlotPair.first.isEmpty()) name = sigSlotPair.first + name;
            else if (not isPortNumeric) name = epOut.getKey().id + name;

            breaker->setId(this->newId(name));
            breaker->setNodeName(breaker->getId()); //the first of its name
            breaker->setRotation(epIn.getObj()->rotation());
            breaker->setPos(epIn.getObj()->pos());

            auto outConn = this->makeConnection(epOut, GraphConnectionEndpoint(breaker, breaker->getConnectableKeys().at(0)));
            if (not sigSlotPair.first.isEmpty()) outConn->addSigSlotPair(std::make_pair(sigSlotPair.first, breaker->getConnectableKeys().at(0).id));
            if (outConn->scene() != breaker->scene()) breaker->scene()->addItem(outConn); //use desired parent
        }
    }

    //create breakers for input endpoints that have to cross
    for (auto conn : boundaryConnections)
    {
        const auto &epOut = conn->getOutputEndpoint();
        const auto &epIn = conn->getInputEndpoint();

        auto sigSlotPairs = conn->getSigSlotPairs();
        if (sigSlotPairs.empty()) sigSlotPairs.resize(1); //add empty
        for (const auto &sigSlotPair : sigSlotPairs)
        {
            //find the existing breaker or make a new one
            const auto name = findInputBreaker(this, epOut, sigSlotPair.first)->getNodeName();
            GraphBreaker *breaker = nullptr;
            for (auto obj : this->getGraphObjects(GRAPH_BREAKER))
            {
                if (obj->draw() != epIn.getObj()->draw()) continue;
                auto outBreaker = dynamic_cast<GraphBreaker *>(obj);
                assert(outBreaker != nullptr);
                if (outBreaker->isInput()) continue;
                if (outBreaker->getNodeName() != name) continue;
                breaker = outBreaker;
                break;
            }

            //make a new output breaker
            if (breaker == nullptr)
            {
                breaker = new GraphBreaker(epIn.getObj()->draw());
                breaker->setInput(false);
                breaker->setId(this->newId(name));
                breaker->setNodeName(name);
                breaker->setRotation(epOut.getObj()->rotation());
                breaker->setPos(epOut.getObj()->pos());
            }

            //connect to this breaker
            auto inConn = this->makeConnection(epIn, GraphConnectionEndpoint(breaker, breaker->getConnectableKeys().at(0)));
            if (not sigSlotPair.second.isEmpty()) inConn->addSigSlotPair(std::make_pair(breaker->getConnectableKeys().at(0).id, sigSlotPair.second));
            if (inConn->scene() != breaker->scene()) breaker->scene()->addItem(inConn); //use desired parent
        }

        delete conn;
    }

    handleStateChange(GraphState("transform-move", desc));
}

void GraphEditor::handleAddBlock(const QJsonObject &blockDesc)
{
    if (not this->isVisible()) return;
    QPointF where(std::rand()%100, std::rand()%100);

    //determine where, a nice point on the visible drawing area sort of upper left
    auto view = dynamic_cast<QGraphicsView *>(this->currentWidget());
    where += view->mapToScene(this->size().width()/4, this->size().height()/4);

    this->handleAddBlock(blockDesc, where);
}

void GraphEditor::handleAddBlock(const QJsonObject &blockDesc, const QPointF &where)
{
    if (blockDesc.isEmpty()) return;
    auto draw = this->getCurrentGraphDraw();
    auto block = new GraphBlock(draw);
    block->setBlockDesc(blockDesc);

    QString hint;
    const auto title = block->getTitle();
    for (int i = 0; i < title.length(); i++)
    {
        if (i == 0 and title.at(i).isNumber()) hint.append(QChar('_'));
        if (title.at(i).isLetterOrNumber() or title.at(i).toLatin1() == '_')
        {
            hint.append(title.at(i));
        }
    }
    block->setId(this->newId(hint));

    //set highest z-index on new block
    block->setZValue(draw->getMaxZValue()+1);

    block->setPos(where);
    block->setRotation(0);
    handleStateChange(GraphState("list-add", tr("Create block %1").arg(title)));
}

void GraphEditor::handleCreateBreaker(const bool isInput)
{
    if (not this->isVisible()) return;

    const auto dirName = isInput?tr("input"):tr("output");
    const auto newName = QInputDialog::getText(this, tr("Create %1 breaker").arg(dirName),
        tr("New breaker node name"), QLineEdit::Normal, tr("untitled"));
    if (newName.isEmpty()) return;

    auto draw = this->getCurrentGraphDraw();
    auto breaker = new GraphBreaker(draw);
    breaker->setInput(isInput);
    breaker->setNodeName(newName);
    breaker->setId(this->newId(newName));
    breaker->setPos(draw->getLastContextMenuPos());

    handleStateChange(GraphState("document-new", tr("Create %1 breaker %2").arg(dirName, newName)));
}

void GraphEditor::handleCreateInputBreaker(void)
{
    this->handleCreateBreaker(true);
}

void GraphEditor::handleCreateOutputBreaker(void)
{
    this->handleCreateBreaker(false);
}

void GraphEditor::handleInsertGraphWidget(QObject *obj)
{
    auto block = dynamic_cast<GraphBlock *>(obj);
    assert(block != nullptr);
    assert(block->isGraphWidget());

    auto draw = this->getCurrentGraphDraw();
    auto display = new GraphWidget(draw);
    display->setGraphBlock(block);
    display->setId(this->newId("Widget"+block->getId()));
    display->setZValue(draw->getMaxZValue()+1);
    display->setPos(draw->getLastContextMenuPos());
    display->setRotation(0);

    handleStateChange(GraphState("insert-image", tr("Insert widget %1").arg(block->getId())));
}

void GraphEditor::handleCut(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    auto desc = tr("Cut %1").arg(draw->getSelectionDescription());

    //load up the clipboard
    this->handleCopy();

    //delete all selected graph objects
    for (auto obj : draw->getObjectsSelected())
    {
        delete obj;
    }

    this->deleteFlagged();

    handleStateChange(GraphState("edit-cut", desc));
}

void GraphEditor::handleCopy(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();

    QJsonArray jsonObjs;
    for (auto obj : draw->getObjectsSelected())
    {
        jsonObjs.push_back(obj->serialize());
    }

    //to byte array
    const QJsonDocument jsonDoc(jsonObjs);
    const auto data = jsonDoc.toBinaryData();

    //load the clipboard
    auto mimeData = new QMimeData();
    mimeData->setData("binary/json/pothos_object_array", data);
    QApplication::clipboard()->setMimeData(mimeData);
}

/*!
 * paste only one object type so handlePaste can control the order of creation
 */
static GraphObjectList handlePasteType(GraphDraw *draw, const QJsonArray &graphObjects, const QString &type)
{
    GraphObjectList newObjects;
    for (const auto &jGraphVal : graphObjects)
    {
        const auto jGraphObj = jGraphVal.toObject();
        const auto what = jGraphObj["what"].toString();
        GraphObject *obj = nullptr;
        if (what != type) continue;
        if (what == "Block") obj = new GraphBlock(draw);
        if (what == "Breaker") obj = new GraphBreaker(draw);
        if (what == "Connection") obj = new GraphConnection(draw);
        if (what == "Widget") obj = new GraphWidget(draw);
        if (obj == nullptr) continue;
        try {obj->deserialize(jGraphObj);}
        catch (const Pothos::NotFoundException &)
        {
            delete obj;
            continue;
        }
        obj->setSelected(true);
        newObjects.push_back(obj);
    }
    return newObjects;
}

void GraphEditor::handlePaste(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();

    auto mimeData = QApplication::clipboard()->mimeData();
    const bool canPaste = mimeData->hasFormat("binary/json/pothos_object_array") and
                      not mimeData->data("binary/json/pothos_object_array").isEmpty();
    if (not canPaste) return;

    //extract object array
    const auto data = mimeData->data("binary/json/pothos_object_array");
    const auto jsonDoc = QJsonDocument::fromBinaryData(data);
    auto graphObjects = jsonDoc.array();

    //rewrite ids
    std::map<QString, QString> oldIdToNew;
    QStringList pastedIds; //prevents duplicates
    for (const auto &graphObjVal : graphObjects)
    {
        const auto jGraphObj = graphObjVal.toObject();
        const auto oldId = jGraphObj["id"].toString();
        const auto newId = this->newId(oldId, pastedIds);
        pastedIds.push_back(newId);
        oldIdToNew[oldId] = newId;
    }
    for (int objIndex = 0; objIndex < graphObjects.size();)
    {
        auto jGraphObj = graphObjects[objIndex].toObject();
        for (const auto &objKey : jGraphObj.keys())
        {
            if (objKey.endsWith("id", Qt::CaseInsensitive))
            {
                //if not in oldIdToNew, remove from list
                const auto value = jGraphObj[objKey].toString();
                if (oldIdToNew.count(value) == 0)
                {
                    graphObjects.removeAt(objIndex);
                    goto nextObj;
                }
                jGraphObj[objKey] = oldIdToNew[value];
            }
            graphObjects[objIndex] = jGraphObj; //write it back
        }
        objIndex++;
        nextObj: continue;
    }

    //unselect all objects
    draw->deselectAllObjs();

    //create objects
    GraphObjectList objsToMove;
    objsToMove.append(handlePasteType(draw, graphObjects, "Block"));
    objsToMove.append(handlePasteType(draw, graphObjects, "Breaker"));
    handlePasteType(draw, graphObjects, "Connection"); //dont append, connection position doesnt matter
    objsToMove.append(handlePasteType(draw, graphObjects, "Widget"));

    //deal with initial positions of pasted objects
    QPointF cornerest(1e6, 1e6);
    for (auto obj : objsToMove)
    {
        cornerest.setX(std::min(cornerest.x(), obj->pos().x()));
        cornerest.setY(std::min(cornerest.y(), obj->pos().y()));
    }

    //determine an acceptable position to center the paste
    auto view = dynamic_cast<QGraphicsView *>(this->currentWidget());
    auto pastePos = view->mapToScene(view->mapFromGlobal(QCursor::pos()));
    if (not view->sceneRect().contains(pastePos))
    {
        pastePos = view->mapToScene(this->size().width()/2, this->size().height()/2);
    }

    //move objects into position
    for (auto obj : objsToMove) obj->setPos(obj->pos()-cornerest+pastePos);

    handleStateChange(GraphState("edit-paste", tr("Paste %1").arg(draw->getSelectionDescription())));
}

void GraphEditor::handleClipboardDataChange(void)
{
    if (not this->isVisible()) return;
    this->updateEnabledActions();
}

void GraphEditor::handleSelectAll(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    for (auto obj : draw->getGraphObjects())
    {
        obj->setSelected(true);
    }
    this->render();
}

void GraphEditor::deleteFlagged(void)
{
    //delete all objects flagged for deletion
    while (true)
    {
        bool deletionOccured = false;
        for (auto obj : this->getGraphObjects())
        {
            if (obj->isFlaggedForDelete())
            {
                delete obj;
                deletionOccured = true;
            }
        }
        if (not deletionOccured) break;
    }
}

void GraphEditor::handleDelete(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    auto desc = tr("Delete %1").arg(draw->getSelectionDescription());

    //delete all selected graph objects
    for (auto obj : draw->getObjectsSelected())
    {
        delete obj;
    }

    this->deleteFlagged();

    handleStateChange(GraphState("edit-delete", desc));
}

void GraphEditor::handleRotateLeft(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    const auto objs = draw->getObjectsSelected(~GRAPH_CONNECTION);
    if (objs.isEmpty()) return;

    //TODO rotate group of objects around central point
    for (auto obj : objs)
    {
        obj->rotateLeft();
    }
    handleStateChange(GraphState("object-rotate-left", tr("Rotate %1 left").arg(draw->getSelectionDescription(~GRAPH_CONNECTION))));
}

void GraphEditor::handleRotateRight(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    const auto objs = draw->getObjectsSelected(~GRAPH_CONNECTION);
    if (objs.isEmpty()) return;

    //TODO rotate group of objects around central point
    for (auto obj : objs)
    {
        obj->rotateRight();
    }
    handleStateChange(GraphState("object-rotate-right", tr("Rotate %1 right").arg(draw->getSelectionDescription(~GRAPH_CONNECTION))));
}

void GraphEditor::handleObjectProperties(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    const auto objs = draw->getObjectsSelected();
    if (not objs.isEmpty()) emit draw->modifyProperties(objs.at(0));
}

void GraphEditor::handleGraphProperties(void)
{
    if (not this->isVisible()) return;
    emit this->getCurrentGraphDraw()->modifyProperties(this);
}

void GraphEditor::handleZoomIn(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    if (draw->zoomScale() >= GraphDrawZoomMax) return;
    draw->setZoomScale(draw->zoomScale() + GraphDrawZoomStep);
}

void GraphEditor::handleZoomOut(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    if (draw->zoomScale() <= GraphDrawZoomMin) return;
    draw->setZoomScale(draw->zoomScale() - GraphDrawZoomStep);
}

void GraphEditor::handleZoomOriginal(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    draw->setZoomScale(1.0);
}

void GraphEditor::handleUndo(void)
{
    if (not this->isVisible()) return;
    assert(_stateManager->isPreviousAvailable());
    this->handleResetState(_stateManager->getCurrentIndex()-1);
}

void GraphEditor::handleRedo(void)
{
    if (not this->isVisible()) return;
    assert(_stateManager->isSubsequentAvailable());
    this->handleResetState(_stateManager->getCurrentIndex()+1);
}

void GraphEditor::handleEnable(void)
{
    return this->handleSetEnabled(true);
}

void GraphEditor::handleDisable(void)
{
    return this->handleSetEnabled(false);
}

void GraphEditor::handleSetEnabled(const bool enb)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();

    //get a set of all selected objects that can be changed
    QSet<GraphObject *> objs;
    for (auto obj : draw->getObjectsSelected())
    {
        //stash the graph widget's actual block
        auto graphWidget = dynamic_cast<GraphWidget *>(obj);
        if (graphWidget != nullptr) obj = graphWidget->getGraphBlock();
        if (obj->isEnabled() != enb) objs.insert(obj);
    }
    if (objs.isEmpty()) return;

    //set the enable property on all objects in the set
    for (auto obj : objs)
    {
        obj->setEnabled(enb);
    }

    if (enb) handleStateChange(GraphState("document-import", tr("Enable %1").arg(draw->getSelectionDescription())));
    else handleStateChange(GraphState("document-export", tr("Disable %1").arg(draw->getSelectionDescription())));
}

void GraphEditor::handleReeval(void)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    if (_evalEngine == nullptr) return;
    _evalEngine->submitReeval(draw->getObjectsSelected(GRAPH_BLOCK));
}

void GraphEditor::handleResetState(int stateNo)
{
    if (not this->isVisible()) return;

    //Resets the state of whoever is modding the properties:
    //Do this before loading the state, otherwise a potential
    //call to handleCancel() afterwards can overwrite settings.
    auto draw = this->getCurrentGraphDraw();
    emit draw->modifyProperties(nullptr);

    //Restore the last displayed page in stateNo,
    //and not the saved viewed page in stateNo.
    const int lastPageNo = _stateToLastTabIndex.at(stateNo);

    _stateManager->resetTo(stateNo);
    this->loadState(_stateManager->current().dump);
    this->setCurrentIndex(lastPageNo);
    this->updateGraphEditorMenus();
    this->render();

    this->updateExecutionEngine();
}

void GraphEditor::handleAffinityZoneClicked(const QString &zone)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();

    for (auto obj : draw->getObjectsSelected(GRAPH_BLOCK))
    {
        auto block = dynamic_cast<GraphBlock *>(obj);
        assert(block != nullptr);
        block->setAffinityZone(zone);
    }
    handleStateChange(GraphState("document-export", tr("Set %1 affinity zone").arg(draw->getSelectionDescription(GRAPH_BLOCK))));
}

void GraphEditor::handleAffinityZoneChanged(const QString &zone)
{
    for (auto obj : this->getGraphObjects(GRAPH_BLOCK))
    {
        auto block = dynamic_cast<GraphBlock *>(obj);
        assert(block != nullptr);
        if (block->getAffinityZone() == zone) block->changed();
    }

    this->render();
    this->updateExecutionEngine();
}

void GraphEditor::handleStateChange(const GraphState &state)
{
    //empty states tell us to simply reset to the current known point
    if (state.iconName.isEmpty() and state.description.isEmpty())
    {
        return this->handleResetState(_stateManager->getCurrentIndex());
    }

    //serialize the graph into the state manager
    GraphState stateWithDump(state);
    stateWithDump.dump = this->dumpState();
    _stateManager->post(stateWithDump);
    this->render();

    this->updateGraphEditorMenus();
    this->updateExecutionEngine();
}

void GraphEditor::handleToggleActivateTopology(const bool enable)
{
    if (not this->isVisible()) return;
    if (_evalEngine == nullptr) return;
    _evalEngine->submitActivateTopology(enable);
    _isTopologyActive = enable;
    this->updateEnabledActions();
}

void GraphEditor::handleBlockDisplayModeChange(void)
{
    for (auto obj : this->getGraphObjects(GRAPH_BLOCK))
    {
        auto block = dynamic_cast<GraphBlock *>(obj);
        assert(block != nullptr);
        block->changed();
    }
    if (this->isVisible()) this->render();
}

void GraphEditor::handleBlockIncrement(void)
{
    this->handleBlockXcrement(+1);
}

void GraphEditor::handleBlockDecrement(void)
{
    this->handleBlockXcrement(-1);
}

void GraphEditor::handleBlockXcrement(const int adj)
{
    if (not this->isVisible()) return;
    auto draw = this->getCurrentGraphDraw();
    GraphObjectList changedObjects;
    for (auto obj : draw->getObjectsSelected(GRAPH_BLOCK))
    {
        auto block = dynamic_cast<GraphBlock *>(obj);
        assert(block != nullptr);
        for (const auto &propKey : block->getProperties())
        {
            auto paramDesc = block->getParamDesc(propKey);
            if (paramDesc["widgetType"].toString() == "SpinBox")
            {
                const auto newValue = block->getPropertyValue(propKey).toInt() + adj;
                block->setPropertyValue(propKey, QString("%1").arg(newValue));
                changedObjects.push_back(block);
                break;
            }
        }
    }

    if (changedObjects.empty()) return;
    const auto desc = (changedObjects.size() == 1)? changedObjects.front()->getId() : tr("selected");
    if (adj > 0) handleStateChange(GraphState("list-add", tr("Increment %1").arg(desc)));
    if (adj < 0) handleStateChange(GraphState("list-remove", tr("Decrement %1").arg(desc)));
}

void GraphEditor::updateExecutionEngine(void)
{
    this->deleteFlagged(); //scan+remove deleted before submit
    if (_evalEngine == nullptr) return;
    _evalEngine->submitTopology(this->getGraphObjects());
}

void GraphEditor::handleEvalEngineDeactivate(void)
{
    _isTopologyActive = false;
    this->updateEnabledActions();
}

void GraphEditor::save(void)
{
    assert(not this->getCurrentFilePath().isEmpty());

    const auto fileName = this->getCurrentFilePath();
    _logger.information("Saving %s", fileName.toStdString());

    QFile jsonFile(fileName);
    if (not jsonFile.open(QFile::WriteOnly) or jsonFile.write(this->dumpState()) == -1)
    {
        _logger.error("Error saving %s: %s", fileName.toStdString(), jsonFile.errorString().toStdString());
    }

    _stateManager->saveCurrent();
    this->render();
}

void GraphEditor::load(void)
{
    auto fileName = this->getCurrentFilePath();

    if (fileName.isEmpty())
    {
        _stateManager->resetToDefault();
        handleStateChange(GraphState("document-new", tr("Create new topology")));
        _stateManager->saveCurrent();
        this->render();
        return;
    }

    _logger.information("Loading %s", fileName.toStdString());
    MainSplash::global()->postMessage(tr("Loading %1").arg(fileName));

    QFile jsonFile(fileName);
    QByteArray data;
    if (not jsonFile.open(QFile::ReadOnly) or (data = jsonFile.readAll()).isEmpty())
    {
        _logger.error("Error loading %s: %s", fileName.toStdString(), jsonFile.errorString().toStdString());
    }
    else this->loadState(data);

    _stateManager->resetToDefault();
    handleStateChange(GraphState("document-new", tr("Load topology from file")));
    _stateManager->saveCurrent();
    this->updateGraphEditorMenus();
    this->render();

    if (_autoActivate)
    {
        _evalEngine->submitActivateTopology(true);
        _isTopologyActive = true;
        this->updateEnabledActions();
    }
}

void GraphEditor::render(void)
{
    //generate a title
    QString title = tr("untitled");
    if (not this->getCurrentFilePath().isEmpty())
    {
        title = QFileInfo(this->getCurrentFilePath()).baseName();
    }
    if (this->hasUnsavedChanges()) title += "*";

    //set the tab text
    for (int i = 0; i < _parentTabWidget->count(); i++)
    {
        if (_parentTabWidget->widget(i) != this) continue;
        _parentTabWidget->setTabText(i, title);
    }

    this->getCurrentGraphDraw()->render();
    this->updateEnabledActions();
}

void GraphEditor::updateGraphEditorMenus(void)
{
    if (not this->isVisible()) return;
    auto mainMenu = MainMenu::global();

    //always store the last visible page with the state
    //we use this to restore the last viewed page when undo/reset
    _stateToLastTabIndex[_stateManager->getCurrentIndex()] = this->currentIndex();

    auto menu = mainMenu->moveGraphObjectsMenu;
    menu->clear();
    for (int i = 0; i < this->count(); i++)
    {
        if (i == this->currentIndex()) continue;
        auto action = menu->addAction(QString("%1 (%2)").arg(this->tabText(i)).arg(i));
        connect(action, SIGNAL(triggered(void)), _moveGraphObjectsMapper, SLOT(map(void)));
        _moveGraphObjectsMapper->setMapping(action, i);
    }

    menu = mainMenu->insertGraphWidgetsMenu;
    menu->clear();
    bool hasGraphWidgetsToInsert = false;
    for (auto obj : this->getGraphObjects(GRAPH_BLOCK))
    {
        auto block = dynamic_cast<GraphBlock *>(obj);
        assert(block != nullptr);
        if (not block->isGraphWidget()) continue;

        //does block have an active graph display?
        for (auto subObj : this->getGraphObjects(GRAPH_WIDGET))
        {
            auto display = dynamic_cast<GraphWidget *>(subObj);
            assert(display != nullptr);
            if (display->getGraphBlock() == block) goto next_block;
        }

        //block is a display widget with no active displays:
        {
            auto action = menu->addAction(QString("%1 (%2)").arg(block->getTitle()).arg(block->getId()));
            connect(action, SIGNAL(triggered(void)), _insertGraphWidgetsMapper, SLOT(map(void)));
            _insertGraphWidgetsMapper->setMapping(action, block);
            hasGraphWidgetsToInsert = true;
        }

        next_block: continue;
    }
    menu->setEnabled(hasGraphWidgetsToInsert);
}

GraphDraw *GraphEditor::getGraphDraw(const int index) const
{
    auto draw = dynamic_cast<GraphDraw *>(this->widget(index));
    assert(draw != nullptr);
    return draw;
}

GraphDraw *GraphEditor::getCurrentGraphDraw(void) const
{
    return this->getGraphDraw(this->currentIndex());
}

GraphObjectList GraphEditor::getGraphObjects(const int selectionFlags) const
{
    GraphObjectList all;
    for (int i = 0; i < this->count(); i++)
    {
        for (auto obj : this->getGraphDraw(i)->getGraphObjects(selectionFlags))
        {
            all.push_back(obj);
        }
    }
    return all;
}

GraphObject *GraphEditor::getObjectById(const QString &id, const int selectionFlags)
{
    for (auto obj : this->getGraphObjects(selectionFlags))
    {
        if (obj->getId() == id) return obj;
    }
    return nullptr;
}

void GraphEditor::makeDefaultPage(void)
{
    this->insertTab(0, new GraphDraw(this), tr("Main"));
}


void GraphEditor::clearGlobals(void)
{
    _globalNames.clear();
    _globalExprs.clear();
}

void GraphEditor::reorderGlobals(const QStringList &names)
{
    _globalNames = names;
}

void GraphEditor::setGlobalExpression(const QString &name, const QString &expression)
{
    if (_globalExprs.count(name) == 0) _globalNames.push_back(name);
    _globalExprs[name] = expression;
}

const QString &GraphEditor::getGlobalExpression(const QString &name) const
{
    return _globalExprs.at(name);
}

const QStringList &GraphEditor::listGlobals(void) const
{
    return _globalNames;
}

void GraphEditor::commitGlobalsChanges(void)
{
    this->updateExecutionEngine();
}

void GraphEditor::handlePollWidgetTimer(void)
{
    if (this->isTopologyLocked()) return;

    //get a list of graph widgets changed since the last state
    QStringList changedIds;
    for (auto obj : this->getGraphObjects(GRAPH_WIDGET))
    {
        auto graphWidget = dynamic_cast<const GraphWidget *>(obj);
        assert(graphWidget != nullptr);
        if (not graphWidget->didWidgetStateChange()) continue;
        auto graphBlock = graphWidget->getGraphBlock();
        if (graphBlock == nullptr) continue;
        changedIds.append(graphBlock->getId());
    }
    if (changedIds.isEmpty()) return;

    //if the previous state is a widget change and its not saved
    //perform state compressions by combining and removing this one
    auto currentState = _stateManager->current();
    if (currentState.extraInfo.isValid() and _stateManager->isPreviousAvailable() and not _stateManager->isCurrentSaved())
    {
        for (const auto &obj : currentState.extraInfo.toStringList()) changedIds.append(obj);
        _stateManager->resetTo(_stateManager->getCurrentIndex()-1);
    }
    changedIds = changedIds.toSet().toList(); //unique list

    //emit a new graph state for the change
    const auto desc = (changedIds.size() == 1)? changedIds.front() : tr("multiple widgets");
    handleStateChange(GraphState("edit-select", tr("Modified %1").arg(desc), changedIds));
}
