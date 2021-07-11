// Copyright (c) 2015-2019 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "GraphEditor/GraphEditor.hpp"
#include "EvalEngine/EvalEngine.hpp"
#include <QDialog>
#include <QTimer>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QFuture>
#include <QFutureWatcher>
#include <QScrollArea>
#include <QTreeWidget>
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <functional> //std::bind

class TopologyStatsDialog : public QDialog
{
    Q_OBJECT
public:

    TopologyStatsDialog(EvalEngine *evalEngine, GraphEditor *parent):
        QDialog(parent),
        _graphEditor(parent),
        _evalEngine(evalEngine),
        _topLayout(new QVBoxLayout(this)),
        _manualReloadButton(new QPushButton(makeIconFromTheme("view-refresh"), tr("Manual Reload"), this)),
        _autoReloadButton(new QPushButton(makeIconFromTheme("view-refresh"), tr("Automatic Reload"), this)),
        _statsScroller(new QScrollArea(this)),
        _statsTree(new QTreeWidget(this)),
        _timer(new QTimer(this)),
        _watcher(new QFutureWatcher<QByteArray>(this))
    {
        //create layouts
        auto formsLayout = new QHBoxLayout();
        _topLayout->addLayout(formsLayout);
        formsLayout->addWidget(_manualReloadButton);
        formsLayout->addWidget(_autoReloadButton);
        _topLayout->addWidget(_statsScroller);

        //setup the refresh buttons
        _autoReloadButton->setCheckable(true);

        //setup the JSON stats tree
        _statsScroller->setWidget(_statsTree);
        _statsScroller->setWidgetResizable(true);

        //connect the signals
        connect(_manualReloadButton, &QPushButton::pressed, this, &TopologyStatsDialog::handleManualReload);
        connect(_autoReloadButton, &QPushButton::clicked, this, &TopologyStatsDialog::handleAutomaticReload);
        connect(_timer, &QTimer::timeout, this, &TopologyStatsDialog::handleManualReload);
        connect(_watcher, &QFutureWatcher<QByteArray>::finished, this, &TopologyStatsDialog::handleWatcherDone);
        connect(_graphEditor, &GraphEditor::windowTitleUpdated, this, &TopologyStatsDialog::handleWindowTitleUpdated);

        //initialize
        this->handleWindowTitleUpdated();
        this->handleManualReload();
    }

private slots:
    void handleManualReload(void)
    {
        this->updateStatusLabel(tr("Manual loading"));
        _watcher->setFuture(QtConcurrent::run(std::bind(&EvalEngine::getTopologyJSONStats, _evalEngine)));
    }

    void handleAutomaticReload(const bool enb)
    {
        if (enb) _timer->start(1000);
        else _timer->stop();
        if (enb) this->updateStatusLabel(tr("Automatic loading"));
        else this->updateStatusLabel(tr("Automatic stopped"));
    }

    void handleWatcherDone(void)
    {
        const auto jsonStats = _watcher->result();

        if (_timer->isActive())
        {
            if (jsonStats.isNull()) this->updateStatusLabel(tr("Automatic holding"));
            else this->updateStatusLabel(tr("Automatic acquisition"));
        }
        else
        {
            if (jsonStats.isNull()) this->updateStatusLabel(tr("Topology inactive"));
            else this->updateStatusLabel(tr("Manual acquisition"));
        }

        //the topology is not active, leave the stats up for display
        if (jsonStats.isNull()) return;

        //update the status display
        const auto result = QJsonDocument::fromJson(jsonStats);
        const auto topObj = result.object();
        for (const auto &name : topObj.keys())
        {
            const auto dataObj = topObj[name].toObject();

            auto &item = _statsItems[name];
            if (item == nullptr)
            {
                const auto title = dataObj["blockName"].toString();
                item = new QTreeWidgetItem(QStringList(title));
                _statsTree->addTopLevelItem(item);
            }

            auto &label = _statsLabels[name];
            if (label == nullptr)
            {
                label = new QLabel(_statsTree);
                label->setStyleSheet("QLabel{margin:1px;}");
                label->setWordWrap(true);
                label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                label->setTextInteractionFlags(Qt::TextSelectableByMouse);
                auto subItem = new QTreeWidgetItem(item);
                _statsTree->setItemWidget(subItem, 0, label);
            }

            label->setText(QJsonDocument(dataObj).toJson(QJsonDocument::Indented));
        }
    }

    void handleWindowTitleUpdated(void)
    {
        this->setWindowTitle(tr("Topology stats - %1").arg(_graphEditor->windowTitle()));
        this->setWindowModified(_graphEditor->isWindowModified());
    }

private:
    void updateStatusLabel(const QString &st)
    {
        _statsTree->setHeaderLabel(tr("Block Stats - %1").arg(st));
    }

    GraphEditor *_graphEditor;
    EvalEngine *_evalEngine;
    QVBoxLayout *_topLayout;
    QPushButton *_manualReloadButton;
    QPushButton *_autoReloadButton;
    QScrollArea *_statsScroller;
    QTreeWidget *_statsTree;
    QTimer *_timer;
    QFutureWatcher<QByteArray> *_watcher;
    std::map<QString, QTreeWidgetItem *> _statsItems;
    std::map<QString, QLabel *> _statsLabels;
};

void GraphEditor::handleShowTopologyStatsDialog(void)
{
    if (not this->isActive()) return;

    //create the dialog
    auto dialog = new TopologyStatsDialog(_evalEngine, this);
    dialog->show();
    dialog->adjustSize();
    dialog->setWindowState(Qt::WindowMaximized);
    dialog->exec();
    delete dialog;
}

#include "GraphEditorTopologyStats.moc"
