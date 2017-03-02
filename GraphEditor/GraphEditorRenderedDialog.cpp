// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "GraphEditor/GraphEditor.hpp"
#include "EvalEngine/EvalEngine.hpp"
#include <Pothos/Framework/Topology.hpp>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QPixmap>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QProcess>

class RenderedGraphDialog : public QDialog
{
    Q_OBJECT
public:
    RenderedGraphDialog(EvalEngine *evalEngine, QWidget *parent):
        QDialog(parent),
        _evalEngine(evalEngine),
        _topLayout(new QVBoxLayout(this)),
        _modeOptions(new QComboBox(this)),
        _portOptions(new QComboBox(this)),
        _process(new QProcess(this)),
        _currentView(nullptr)
    {
        //create layouts
        this->setWindowTitle(tr("Rendered topology viewer"));
        auto formsLayout = new QHBoxLayout();
        _topLayout->addLayout(formsLayout);

        //create entry forms
        auto modeOptionsLayout = new QFormLayout();
        formsLayout->addLayout(modeOptionsLayout);
        _modeOptions->addItem(tr("Top level"), QString("top"));
        _modeOptions->addItem(tr("Flattened"), QString("flat"));
        _modeOptions->addItem(tr("Rendered"), QString("rendered"));
        modeOptionsLayout->addRow(tr("Display mode"), _modeOptions);

        auto portOptionsLayout = new QFormLayout();
        formsLayout->addLayout(portOptionsLayout);
        _portOptions->addItem(tr("Connected"), QString("connected"));
        _portOptions->addItem(tr("All ports"), QString("all"));
        portOptionsLayout->addRow(tr("Show ports"), _portOptions);

        //connect widget changed events
        connect(_modeOptions, SIGNAL(currentIndexChanged(int)), this, SLOT(handleChange(int)));
        connect(_portOptions, SIGNAL(currentIndexChanged(int)), this, SLOT(handleChange(int)));
        connect(_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(handleProcessDone(int, QProcess::ExitStatus)));

        //initialize
        QPixmap pixmap(parent->size());
        pixmap.fill(Qt::white);
        installNewView(pixmap);
        this->handleChange(0);
    }

private slots:
    void handleChange(int)
    {
        QJsonObject configObj;
        configObj["mode"] = _modeOptions->itemData(_modeOptions->currentIndex()).toString();
        configObj["port"] = _portOptions->itemData(_portOptions->currentIndex()).toString();
        const auto markup = _evalEngine->getTopologyDotMarkup(QJsonDocument(configObj).toJson());

        //create args
        QStringList args;
        args.push_back("-Tpng"); //yes png
        args.push_back("-o"); //output to file
        _tempFile.open();
        args.push_back(_tempFile.fileName());

        //launch
        auto dotExe = qgetenv("DOT_EXECUTABLE");
        if (dotExe.isNull()) dotExe = "dot";
        _process->start(dotExe, args);
        if (not _process->waitForStarted())
        {
            this->displayErrorMessage(tr("%1\nIs Graphviz installed?").arg(_process->errorString()));
        }

        //write the markup into dot
        _process->write(markup);
        _process->closeWriteChannel();
    }

    void handleProcessDone(int exitCode, QProcess::ExitStatus exitStatus)
    {
        if (exitCode != 0 or exitStatus != QProcess::NormalExit)
        {
            this->displayErrorMessage(_process->readAllStandardError());
        }

        else
        {
            this->installNewView(QPixmap(_tempFile.fileName(), "png"));
        }
    }

    void displayErrorMessage(const QString &errorMsg)
    {
        QMessageBox msgBox(QMessageBox::Critical, tr("Topology render error"), tr("Image generation failed!\n%1").arg(errorMsg));
        msgBox.exec();
    }

    void installNewView(const QPixmap &pixmap)
    {
        delete _currentView; //delete the previous view
        _currentView = new QWidget(this);
        auto layout = new QVBoxLayout(_currentView);
        auto scroll = new QScrollArea(_currentView);
        layout->addWidget(scroll);
        auto label = new QLabel(scroll);
        scroll->setWidget(label);
        scroll->setWidgetResizable(true);
        label->setPixmap(pixmap);
        _topLayout->addWidget(_currentView, 1);
    }

private:
    EvalEngine *_evalEngine;
    QVBoxLayout *_topLayout;
    QComboBox *_modeOptions;
    QComboBox *_portOptions;
    QProcess *_process;
    QTemporaryFile _tempFile;
    QWidget *_currentView;
};

void GraphEditor::handleShowRenderedGraphDialog(void)
{
    if (not this->isVisible()) return;

    //create the dialog
    auto dialog = new RenderedGraphDialog(_evalEngine, this);
    dialog->show();
    dialog->adjustSize();
    dialog->setWindowState(Qt::WindowMaximized);
    dialog->exec();
    delete dialog;
}

#include "GraphEditorRenderedDialog.moc"
