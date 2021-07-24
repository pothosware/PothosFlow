// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "GraphEditor/GraphEditorTabs.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "MainWindow/MainActions.hpp"
#include "MainWindow/MainSettings.hpp"
#include "MainWindow/MainSplash.hpp"
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QAction>
#include <QDir>
#include <QStandardPaths>
#include <Poco/Logger.h>
#include <iostream>
#include <cassert>

GraphEditorTabs::GraphEditorTabs(QWidget *parent):
    QTabWidget(parent)
{
    this->setDocumentMode(true);
    this->setTabsClosable(true);
    this->setMovable(true);
    this->setUsesScrollButtons(true);
    this->setTabPosition(QTabWidget::North);
    this->setStyleSheet(
        QString("QTabBar::close-button {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-16.png"))+
        QString("QTabBar::close-button:hover {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-hover-16.png"))+
        QString("QTabBar::close-button:pressed {image: url(%1);}").arg(makeIconPath("standardbutton-closetab-down-16.png")));

    auto actions = MainActions::global();
    connect(actions->newAction, &QAction::triggered, [=](void){this->handleNew();});
    connect(actions->openAction, &QAction::triggered, [=](void){this->handleOpen();});
    connect(actions->saveAction, &QAction::triggered, [=](void){this->handleSave();});
    connect(actions->saveAsAction, &QAction::triggered, [=](void){this->handleSaveAs();});
    connect(actions->saveAllAction, &QAction::triggered, [=](void){this->handleSaveAll();});
    connect(actions->reloadAction, &QAction::triggered, [=](void){this->handleReload();});
    connect(actions->closeAction, &QAction::triggered, [=](void){this->handleClose();});
    connect(actions->exportAction, &QAction::triggered, [=](void){this->handleExport();});
    connect(actions->exportAsAction, &QAction::triggered, [=](void){this->handleExportAs();});
    connect(this, &GraphEditorTabs::tabCloseRequested, [=](int i){this->handleCloseIndex(i);});
    connect(this->tabBar(), &QTabBar::tabMoved, [=](int, int){this->saveState();});
}

GraphEditor *GraphEditorTabs::getGraphEditor(const int i) const
{
    return qobject_cast<GraphEditor *>(this->widget(i));
}

GraphEditor *GraphEditorTabs::getCurrentGraphEditor(void) const
{
    return this->getGraphEditor(this->currentIndex());
}

void GraphEditorTabs::handleNew(void)
{
    auto editor = new GraphEditor(this);
    this->addTab(editor, "");
    this->setCurrentWidget(editor);
    editor->load();
    this->saveState();
}

void GraphEditorTabs::doReloadDialog(GraphEditor *editor)
{
    if (not editor->hasUnsavedChanges()) return;

    //yes/no dialog if we have unsaved changes
    this->setCurrentWidget(editor);
    const auto reply = QMessageBox::question(this,
        tr("Reload: unsaved changes!"),
        tr("Unsaved changes %1!\nAre you sure that you want to reload?").arg(editor->getCurrentFilePath()),
        QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) editor->load();
}

void GraphEditorTabs::handleOpen(void)
{
    auto settings = MainSettings::global();
    auto lastPath = settings->value("GraphEditorTabs/lastFile").toString();
    if(lastPath.isEmpty()) {
        lastPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }
    assert(not lastPath.isEmpty());
    auto filePaths = QFileDialog::getOpenFileNames(this,
                        tr("Open Files"),
                        lastPath,
                        tr("Pothos Topologies (*.pothos)"));

    for (const auto &file : filePaths)
    {
        const auto filePath = QDir(file).absolutePath();
        settings->setValue("GraphEditorTabs/lastFile", filePath);
        this->handleOpen(filePath);
    }
}

void GraphEditorTabs::handleOpen(const QString &filePath)
{
    //filter out files that are already open
    for (int j = 0; j < this->count(); j++)
    {
        auto editor = qobject_cast<GraphEditor *>(this->widget(j));
        assert(editor != nullptr);
        if (editor->getCurrentFilePath() == filePath)
        {
            return this->doReloadDialog(editor);
        }
    }

    //open a new editor with the specified file
    auto editor = new GraphEditor(this);
    editor->setCurrentFilePath(filePath);
    this->addTab(editor, "");
    editor->load();
    this->setCurrentWidget(editor);

    this->saveState();
}

void GraphEditorTabs::handleSave(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);
    this->handleSave(editor);
}

void GraphEditorTabs::handleSave(GraphEditor *editor)
{
    //no file path? redirect to save as
    if (editor->getCurrentFilePath().isEmpty()) this->handleSaveAs();

    //otherwise, just save the topology
    else editor->save();
}

static QString defaultSavePath(void)
{
    const auto defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    assert(!defaultPath.isEmpty());
    return QDir(defaultPath).absoluteFilePath("untitled.pothos");
}

void GraphEditorTabs::handleSaveAs(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);

    QString lastPath = editor->getCurrentFilePath();
    if (lastPath.isEmpty()) lastPath = defaultSavePath();

    this->setCurrentWidget(editor);
    auto filePath = QFileDialog::getSaveFileName(this,
                        tr("Save As"),
                        lastPath,
                        tr("Pothos Topologies (*.pothos)"));
    if (filePath.isEmpty()) return;
    if (not filePath.endsWith(".pothos")) filePath += ".pothos";
    filePath = QDir(filePath).absolutePath();
    auto settings = MainSettings::global();
    settings->setValue("GraphEditorTabs/lastFile", filePath);
    editor->setCurrentFilePath(filePath);
    editor->save();
    this->saveState();
}

void GraphEditorTabs::handleReload(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);
    this->doReloadDialog(editor);
}

void GraphEditorTabs::handleSaveAll(void)
{
    for (int i = 0; i < this->count(); i++)
    {
        auto editor = qobject_cast<GraphEditor *>(this->widget(i));
        assert(editor != nullptr);
        this->handleSave(editor);
    }
    this->saveState();
}

void GraphEditorTabs::handleClose(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);
    this->handleClose(editor);
}

void GraphEditorTabs::handleCloseIndex(int index)
{
    auto editor = qobject_cast<GraphEditor *>(this->widget(index));
    assert(editor != nullptr);
    this->handleClose(editor);
}

void GraphEditorTabs::handleClose(GraphEditor *editor)
{
    if (editor->hasUnsavedChanges())
    {
        //yes/no dialog if we have unsaved changes
        this->setCurrentWidget(editor);
        const auto reply = QMessageBox::question(this,
            tr("Close: unsaved changes!"),
            tr("Unsaved changes %1!\nWould you like to save changes?").arg(editor->getCurrentFilePath()),
            QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Yes) this->handleSave();
    }
    delete editor;
    this->ensureOneEditor();
    this->saveState();
}

void GraphEditorTabs::handleExit(QCloseEvent *event)
{
    //dont save state for any further tab selection changes
    disconnect(this, &GraphEditorTabs::currentChanged, this, &GraphEditorTabs::handleChanged);

    //exit logic -- save changes dialogs
    for (int i = 0; i < this->count(); i++)
    {
        auto editor = qobject_cast<GraphEditor *>(this->widget(i));
        assert(editor != nullptr);
        if (not editor->hasUnsavedChanges()) continue;

        this->setCurrentIndex(i); //select this editor

        //yes/no dialog if we have unsaved changes
        const auto reply = QMessageBox::question(this,
            tr("Exit: unsaved changes!"),
            tr("Unsaved changes %1!\nWould you like to save changes?").arg(editor->getCurrentFilePath()),
            QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
        if (reply == QMessageBox::Yes) this->handleSave();
    }

    event->accept();
}

void GraphEditorTabs::handleExport(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);

    auto path = editor->getCurrentFilePath();
    if (path.endsWith(".pothos")) path = path.left(path.size()-7);
    path += ".json";

    editor->exportToJSONTopology(path);
}

void GraphEditorTabs::handleExportAs(void)
{
    auto editor = qobject_cast<GraphEditor *>(this->currentWidget());
    assert(editor != nullptr);

    QString lastPath = editor->getCurrentFilePath();
    if (lastPath.isEmpty()) lastPath = defaultSavePath();
    if (lastPath.endsWith(".pothos")) lastPath = lastPath.left(lastPath.size()-7);
    lastPath += ".json";

    this->setCurrentWidget(editor);
    auto filePath = QFileDialog::getSaveFileName(this,
                        tr("Export As"),
                        lastPath,
                        tr("Exported JSON Topologies (*.json)"));
    if (filePath.isEmpty()) return;
    if (not filePath.endsWith(".json")) filePath += ".json";
    filePath = QDir(filePath).absolutePath();

    editor->exportToJSONTopology(filePath);
}

void GraphEditorTabs::handleChanged(int)
{
    this->saveState();
}

void GraphEditorTabs::loadState(void)
{
    MainSplash::global()->postMessage(tr("Restoring graph editor..."));

    //load option topologies from file list
    auto settings = MainSettings::global();
    auto files = settings->value("GraphEditorTabs/files").toStringList();
    for (int i = 0; i < files.size(); i++)
    {
        if (files.at(i).isEmpty()) continue; //skip empty files
        if (not QFile::exists(files.at(i)))
        {
            static auto &logger = Poco::Logger::get("PothosFlow.GraphEditorTabs");
            logger.error("File %s does not exist", files.at(i).toStdString());
            continue;
        }
        auto editor = new GraphEditor(this);
        editor->setCurrentFilePath(files.at(i));
        this->addTab(editor, "");
        editor->load();
    }

    //Nothing? make sure we have at least one editor
    this->ensureOneEditor();

    //restore the active index setting
    this->setCurrentIndex(settings->value("GraphEditorTabs/activeIndex").toInt());

    //become sensitive to tab selected index changes
    connect(this, &GraphEditorTabs::currentChanged, this, &GraphEditorTabs::handleChanged);
}

void GraphEditorTabs::ensureOneEditor(void)
{
    if (this->count() > 0) return;
    this->handleNew();
    this->saveState();
}

void GraphEditorTabs::saveState(void)
{
    //save the file paths for the editors
    QStringList files;
    for (int i = 0; i < this->count(); i++)
    {
        auto editor = qobject_cast<GraphEditor *>(this->widget(i));
        assert(editor != nullptr);
        files.push_back(editor->getCurrentFilePath());
    }
    auto settings = MainSettings::global();
    settings->setValue("GraphEditorTabs/files", files);

    //save the currently selected editor tab
    settings->setValue("GraphEditorTabs/activeIndex", this->currentIndex());
}
