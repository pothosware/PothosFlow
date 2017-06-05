// Copyright (c) 2015-2017 Josh Blum
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

    TopologyStatsDialog(EvalEngine *evalEngine, QWidget *parent):
        QDialog(parent),
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
        this->setWindowTitle(tr("Topology stats viewer"));
        auto formsLayout = new QHBoxLayout();
        _topLayout->addLayout(formsLayout);
        formsLayout->addWidget(_manualReloadButton);
        formsLayout->addWidget(_autoReloadButton);
        _topLayout->addWidget(_statsScroller);

        //setup the refresh buttons
        _autoReloadButton->setCheckable(true);

        //setup the JSON stats tree
        _statsTree->setHeaderLabels(QStringList(tr("Block Stats")));
        _statsScroller->setWidget(_statsTree);
        _statsScroller->setWidgetResizable(true);

        //connect the signals
        connect(_manualReloadButton, SIGNAL(pressed(void)), this, SLOT(handleManualReload(void)));
        connect(_autoReloadButton, SIGNAL(clicked(bool)), this, SLOT(handleAutomaticReload(bool)));
        connect(_timer, SIGNAL(timeout(void)), this, SLOT(handleManualReload(void)));
        connect(_watcher, SIGNAL(finished(void)), this, SLOT(handleWatcherDone(void)));

        //initialize
        this->handleManualReload();
    }

private slots:
    void handleManualReload(void)
    {
        _watcher->setFuture(QtConcurrent::run(std::bind(&EvalEngine::getTopologyJSONStats, _evalEngine)));
    }

    void handleAutomaticReload(const bool enb)
    {
        if (enb) _timer->start(1000);
        else _timer->stop();
    }

    void handleWatcherDone(void)
    {
        const auto jsonStats = _watcher->result();
        if (jsonStats.isNull())
        {
            QMessageBox msgBox(QMessageBox::Critical, tr("Topology stats error"), tr("no stats - is the topology active?"));
            msgBox.exec();
        }
        else
        {
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
                    label->setStyleSheet("QLabel{background:white;margin:1px;}");
                    label->setWordWrap(true);
                    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
                    auto subItem = new QTreeWidgetItem(item);
                    _statsTree->setItemWidget(subItem, 0, label);
                }

                label->setText(QJsonDocument(dataObj).toJson(QJsonDocument::Indented));
            }
        }
    }

private:
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
    if (not this->isVisible()) return;

    //create the dialog
    auto dialog = new TopologyStatsDialog(_evalEngine, this);
    dialog->show();
    dialog->adjustSize();
    dialog->setWindowState(Qt::WindowMaximized);
    dialog->exec();
    delete dialog;
}

#include "GraphEditorTopologyStats.moc"
