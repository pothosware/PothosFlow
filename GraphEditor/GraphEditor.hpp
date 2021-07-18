// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include "GraphObjects/GraphObject.hpp"
#include "GraphEditor/GraphState.hpp"
#include "GraphEditor/DockingTabWidget.hpp"
#include <Poco/Logger.h>
#include <QJsonObject>
#include <QPointer>

class GraphConnection;
class GraphDraw;
class QTabWidget;
class EvalEngine;
class QTimer;

class GraphEditor : public DockingTabWidget
{
    Q_OBJECT
public:
    GraphEditor(QWidget *parent);
    ~GraphEditor(void);

    //! Stop the evaluator and for plugin reload
    void stopEvaluation(void);

    //! Restore evaluator and from a plugin reload
    void restartEvaluation(void);

    QByteArray dumpState(void) const;

    void loadState(const QByteArray &data);

    /*!
     * Generate a new ID that is unique to the graph,
     * taking into account the IDs used by all objects within the graph.
     * The blacklist is currently used during the handle paste operation
     * to prevent ID duplicates before the pasted blocks are instantiated.
     * \param hint an optional string to guide the name of the new ID.
     * \param blacklist a list of IDs that are not allowed to be used
     * \return a new ID that is unique to the graph
     */
    QString newId(const QString &hint = "", const QStringList &blacklist = QStringList()) const;

    //! Serializes the editor and saves to file.
    void save(void);

    //! Deserializes the editor from the file.
    void load(void);

    //! Export the design to JSON topology format give the file path
    void exportToJSONTopology(const QString &fileName);

    const QString &getCurrentFilePath(void) const
    {
        return _currentFilePath;
    }

    void setCurrentFilePath(const QString &path)
    {
        _currentFilePath = path;
    }

    bool hasUnsavedChanges(void) const
    {
        return not _stateManager->isCurrentSaved();
    }

    void handleAddBlock(const QJsonObject &, const QPointF &, GraphDraw *draw);

    //! force a re-rendering of the graph page
    void render(void);

    GraphDraw *getGraphDraw(const int index) const;

    GraphDraw *getCurrentGraphDraw(void) const;

    //! Get a list of all graph objects in all pages
    GraphObjectList getGraphObjects(const int selectionFlags = ~0) const;

    //! Get the graph object with the specified ID or nullptr
    GraphObject *getObjectById(const QString &id, const int selectionFlags = ~0);

    //! Make a connection between two endpoints
    GraphConnection *makeConnection(const GraphConnectionEndpoint &ep0, const GraphConnectionEndpoint &ep1);

    //! clear all globals
    void clearGlobals(void);

    //! Set a global variable and its expression -- overwrite existing
    void setGlobalExpression(const QString &name, const QString &expression);

    //! Get a global variable's expression
    const QString &getGlobalExpression(const QString &name) const;

    //! Get a list of all globals by name (order they were added)
    const QStringList &listGlobals(void) const;

    //! Reorder globals based on a new name list
    void reorderGlobals(const QStringList &names);

    //! Tell the evaluator that globals have been modified
    void commitGlobalsChanges(void);

    //! Is auto activate enabled?
    bool isAutoActivate(void) const
    {
        return _autoActivate;
    }

    void setAutoActivate(const bool enb)
    {
        _autoActivate = enb;
    }

    //! Is lock topology enabled?
    bool isTopologyLocked(void) const
    {
        return _lockTopology;
    }

    void setLockTopology(const bool enb)
    {
        _lockTopology = enb;
        this->updateEnabledActions();
    }

    void setSceneSize(const QSize &size);

    QSize getSceneSize(void) const
    {
        return _sceneSize;
    }

signals:
    void windowTitleUpdated(void);

protected:
    //this widget is visible, populate menu with its tabs
    void showEvent(QShowEvent *event);

public slots:
    void handleStateChange(const GraphState &state);

private slots:
    void handleCreateGraphPage(void);
    void handleRenameGraphPage(void);
    void handleDeleteGraphPage(void);
    void handleMoveGraphObjects(const int index);
    void handleAddBlockSlot(const QJsonObject &);
    void handleCreateBreaker(const bool isInput);
    void handleCreateInputBreaker(void);
    void handleCreateOutputBreaker(void);
    void handleInsertGraphWidget(QObject *);
    void handleCut(void);
    void handleCopy(void);
    void handlePaste(void);
    void handleClipboardDataChange(void);
    void handleSelectAll(void);
    void handleDelete(void);
    void handleRotateLeft(void);
    void handleRotateRight(void);
    void handleObjectProperties(void);
    void handleGraphProperties(void);
    void handleZoomIn(void);
    void handleZoomOut(void);
    void handleZoomOriginal(void);
    void handleUndo(void);
    void handleRedo(void);
    void handleEnable(void);
    void handleSetEnabled(const bool enb);
    void handleDisable(void);
    void handleReeval(void);
    void handleResetState(int);
    void handleAffinityZoneClicked(const QString &zone);
    void handleAffinityZoneChanged(const QString &zone);
    void handleShowRenderedGraphDialog(void);
    void handleShowTopologyStatsDialog(void);
    void handleToggleActivateTopology(bool);
    void handleBlockDisplayModeChange(void);
    void handleBlockIncrement(void);
    void handleBlockDecrement(void);
    void handleBlockXcrement(const int adj);
    void handleEvalEngineDeactivate(void);
    void handlePollWidgetTimer(void);

private:
    Poco::Logger &_logger;
    QTabWidget *_parentTabWidget;

    void loadPages(const QJsonArray &pages, const QString &type);

    void updateGraphEditorMenus(void);

    void makeDefaultPage(void);

    void deleteFlagged(void);

    QString _currentFilePath;
    QPointer<GraphStateManager> _stateManager;
    std::map<size_t, QVariant> _stateToLastDisplayState;

    //! update enabled actions based on state - after a change or when editor becomes visible
    void updateEnabledActions(void);

    //! called after state changes
    void updateExecutionEngine(void);

    EvalEngine *_evalEngine;
    bool _isTopologyActive;
    QTimer *_pollWidgetTimer;

    //graph globals/constant expressions
    QStringList _globalNames;
    std::map<QString, QString> _globalExprs;
    bool _autoActivate;
    bool _lockTopology;
    QSize _sceneSize;
};
