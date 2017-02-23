// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/FormLayout.hpp"
#include "AffinitySupport/AffinityZoneEditor.hpp"
#include "AffinitySupport/CpuSelectionWidget.hpp"
#include "HostExplorer/HostExplorerDock.hpp"
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <Poco/Logger.h>
#include <QFormLayout>
#define QT_QTCOLORPICKER_IMPORT
#include <QtColorPicker>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QJsonArray>
#include <cassert>

static const int ARBITRARY_MAX_THREADS = 4096;

AffinityZoneEditor::AffinityZoneEditor(const QString &zoneName, QWidget *parent, HostExplorerDock *hostExplorer):
    QWidget(parent),
    _zoneName(zoneName),
    _hostExplorerDock(hostExplorer),
    _colorPicker(new QtColorPicker(this)),
    _hostsBox(new QComboBox(this)),
    _processNameEdit(new QLineEdit(this)),
    _numThreadsSpin(new QSpinBox(this)),
    _prioritySpin(new QSpinBox(this)),
    _cpuSelection(nullptr),
    _cpuSelectionContainer(new QVBoxLayout()),
    _yieldModeBox(new QComboBox(this))
{
    assert(_hostExplorerDock != nullptr);

    //bold title
    this->setStyleSheet("QGroupBox{font-weight: bold;}");

    //setup layout
    auto formLayout = makeFormLayout(this);
    this->setLayout(formLayout);

    //color picker
    {
        formLayout->addRow(tr("Affinity color"), _colorPicker);
        //https://en.wikipedia.org/wiki/List_of_colors_%28compact%29
        _colorPicker->insertColor(QColor(119,158,203), tr("Dark pastel blue"));
        _colorPicker->insertColor(QColor(3,192,60), tr("Dark pastel green"));
        _colorPicker->insertColor(QColor(150,111,214), tr("Dark pastel purple"));
        _colorPicker->insertColor(QColor(194,59,34), tr("Dark pastel red"));
        _colorPicker->insertColor(QColor(177,156,217), tr("Light pastel purple"));
        _colorPicker->insertColor(QColor(174,198,207), tr("Pastel blue"));
        _colorPicker->insertColor(QColor(130,105,83), tr("Pastel brown"));
        _colorPicker->insertColor(QColor(207,207,196), tr("Pastel gray"));
        _colorPicker->insertColor(QColor(119,221,119), tr("Pastel green"));
        _colorPicker->insertColor(QColor(244,154,194), tr("Pastel magenta"));
        _colorPicker->insertColor(QColor(255,179,71), tr("Pastel orange"));
        _colorPicker->insertColor(QColor(222,165,164), tr("Pastel pink"));
        _colorPicker->insertColor(QColor(179,158,181), tr("Pastel purple"));
        _colorPicker->insertColor(QColor(255,105,97), tr("Pastel red"));
        _colorPicker->insertColor(QColor(203,153,201), tr("Pastel violet"));
        _colorPicker->insertColor(QColor(253,253,150), tr("Pastel yellow"));
        _colorPicker->setCurrentColor(QColor(253,253,150)); //Pastel yellow
        _colorPicker->setToolTip(tr("Select a color to associate affinities in the graph editor"));
        connect(_colorPicker, SIGNAL(colorChanged(const QColor &)), this, SLOT(handleColorChanged(const QColor &)));
    }

    //host selection
    {
        formLayout->addRow(tr("Host URI"), _hostsBox);
        _hostsBox->setEditable(true);
        _hostsBox->setToolTip(tr("Select the URI for a local or remote host"));
        connect(_hostsBox, SIGNAL(activated(int)), this, SLOT(handleUriChanged(int)));
        connect(_hostExplorerDock, SIGNAL(hostUriListChanged(void)), this, SLOT(handleHostListChanged(void)));
        this->handleHostListChanged();
    }

    //process id
    {
        formLayout->addRow(tr("Process name"), _processNameEdit);
        _processNameEdit->setPlaceholderText(tr("The string name of a process"));
        _processNameEdit->setToolTip(tr("An arbitrary name to identify a process on a node"));
        connect(_processNameEdit, SIGNAL(editingFinished(void)), this, SLOT(handleProcessNameChanged(void)));
    }

    //num threads
    {
        formLayout->addRow(tr("Thread count"), _numThreadsSpin);
        _numThreadsSpin->setRange(0, ARBITRARY_MAX_THREADS);
        _numThreadsSpin->setToolTip(tr("Number of threads to allocate, 0 means automatic"));
        connect(_numThreadsSpin, SIGNAL(editingFinished(void)), this, SLOT(handleSpinSelChanged(void)));
    }

    //priority selection
    {
        formLayout->addRow(tr("Process priority %"), _prioritySpin);
        _prioritySpin->setRange(-100, +100);
        _prioritySpin->setToolTip(tr("A priority percentage between -100% and 100%"));
        connect(_prioritySpin, SIGNAL(editingFinished(void)), this, SLOT(handleSpinSelChanged(void)));
    }

    //cpu/node selection
    {
        formLayout->addRow(tr("CPU selection"), _cpuSelectionContainer);
        this->updateCpuSelection();
    }

    //yield mode
    {
        formLayout->addRow(tr("Yield mode"), _yieldModeBox);
        _yieldModeBox->addItem(tr("Default"), "");
        _yieldModeBox->addItem(tr("Condition"), "CONDITION");
        _yieldModeBox->addItem(tr("Hybrid"), "HYBRID");
        _yieldModeBox->addItem(tr("Spin"), "SPIN");
        _yieldModeBox->setToolTip(tr("Yield mode specifies the internal threading mechanisms"));
        connect(_yieldModeBox, SIGNAL(activated(int)), this, SLOT(handleComboChanged(int)));
    }
}

QColor AffinityZoneEditor::color(void) const
{
    return _colorPicker->currentColor();
}

void AffinityZoneEditor::handleHostListChanged(void)
{
    auto uri = _hostsBox->itemText(_hostsBox->currentIndex());
    _hostsBox->clear();
    _hostsBox->addItems(_hostExplorerDock->hostUriList());
    if (not uri.isEmpty()) this->selectThisUri(uri);
}

void AffinityZoneEditor::selectThisUri(const QString &uri)
{
    for (int i = 0; i < _hostsBox->count(); i++)
    {
        if (_hostsBox->itemText(i) == uri)
        {
            _hostsBox->setCurrentIndex(i);
            this->updateCpuSelection();
            return;
        }
    }

    //didnt select, make a new entry...
    _hostsBox->insertItem(0, uri);
    _hostsBox->setCurrentIndex(0);
    this->updateCpuSelection();
}

void AffinityZoneEditor::loadFromConfig(const QJsonObject &config)
{
    if (config.contains("color"))
    {
        _colorPicker->blockSignals(true);
        _colorPicker->setCurrentColor(QColor(config["color"].toString()));
        _colorPicker->blockSignals(false);
    }
    if (config.contains("hostUri"))
    {
        this->selectThisUri(config["hostUri"].toString());
    }
    if (config.contains("processName"))
    {
        _processNameEdit->setText(config["processName"].toString());
    }
    if (config.contains("numThreads"))
    {
        _numThreadsSpin->setValue(config["numThreads"].toInt());
    }
    if (config.contains("priority"))
    {
        _prioritySpin->setValue(int(config["priority"].toDouble()*100));
    }
    if (config.contains("affinityMode") and config.contains("affinity"))
    {
        const auto mask = config["affinity"].toArray();
        std::vector<int> selection;
        for (int i = 0; i < mask.size(); i++) selection.push_back(mask.at(i).toInt());
        _cpuSelection->setup(config["affinityMode"].toString(), selection);
    }
    if (config.contains("yieldMode"))
    {
        const auto mode = config["yieldMode"].toString();
        for (int i = 0; i < _yieldModeBox->count(); i++)
        {
            if (_yieldModeBox->itemData(i).toString() == mode) _yieldModeBox->setCurrentIndex(i);
        }
    }
}

QJsonObject AffinityZoneEditor::getCurrentConfig(void) const
{
    QJsonObject config;
    config["color"] = _colorPicker->currentColor().name();
    config["hostUri"] = _hostsBox->itemText(_hostsBox->currentIndex());
    config["processName"] = _processNameEdit->text();
    config["numThreads"] = _numThreadsSpin->value();
    config["priority"] = _prioritySpin->value()/100.0;
    assert(_cpuSelection != nullptr);
    config["affinityMode"] = _cpuSelection->mode();
    QJsonArray affinity;
    for (auto num : _cpuSelection->selection()) affinity.push_back(num);
    config["affinity"] = affinity;
    config["yieldMode"] = _yieldModeBox->itemData(_yieldModeBox->currentIndex()).toString();
    return config;
}

void AffinityZoneEditor::updateCpuSelection(void)
{
    //get node info and cache it
    auto uriStr = _hostsBox->itemText(_hostsBox->currentIndex());
    if (_uriToNumaInfo[uriStr].empty()) try
    {
        auto env = Pothos::RemoteClient(uriStr.toStdString()).makeEnvironment("managed");
        auto nodeInfos = env->findProxy("Pothos/System/NumaInfo").call<std::vector<Pothos::System::NumaInfo>>("get");
        _uriToNumaInfo[uriStr] = nodeInfos;
    }
    catch (const Pothos::Exception &){}

    delete _cpuSelection;
    _cpuSelection = new CpuSelectionWidget(_uriToNumaInfo[uriStr], this);
    connect(_cpuSelection, SIGNAL(selectionChanged(void)), this, SLOT(handleSpinSelChanged(void)));
    _cpuSelectionContainer->addWidget(_cpuSelection);
}
