// Copyright (c) 2013-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "MainWindow/IconUtils.hpp"
#include "HostExplorer/HostSelectionTable.hpp"
#include "MainWindow/MainSettings.hpp"
#include <QLineEdit>
#include <QHBoxLayout>
#include <QToolButton>
#include <QTimer>
#include <QToolTip>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <Pothos/Remote.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Util/Network.hpp>
#include <functional> //std::bind

/***********************************************************************
 * NodeInfo update implementation
 **********************************************************************/
void NodeInfo::update(void)
{
    auto settings = MainSettings::global();
    //determine if the host is online and update access times
    try
    {
        Pothos::RemoteClient client(this->uri.toStdString());
        if (this->nodeName.isEmpty())
        {
            auto env = client.makeEnvironment("managed");
            Pothos::System::HostInfo hostInfo = env->findProxy("Pothos/System/HostInfo").call("get");
            this->nodeName = QString::fromStdString(hostInfo.nodeName);
            settings->setValue("HostExplorer/"+this->uri+"/nodeName", this->nodeName);
        }
        this->isOnline = true;
        this->lastAccess = QDateTime::currentDateTime();
        settings->setValue("HostExplorer/"+this->uri+"/lastAccess", this->lastAccess);
    }
    //otherwise, fetch the information from the settings cache
    catch(const Pothos::RemoteClientError &)
    {
        this->isOnline = false;
        if (this->nodeName.isEmpty())
        {
            this->nodeName = settings->value("HostExplorer/"+this->uri+"/nodeName").toString();
        }
        this->lastAccess = settings->value("HostExplorer/"+this->uri+"/lastAccess").toDateTime();
    }
}

/***********************************************************************
 * global query for hosts known to system
 **********************************************************************/
static QStringList getHostUriList(void)
{
    auto settings = MainSettings::global();
    auto uris = settings->value("HostExplorer/uris").toStringList();
    uris.push_front(QString::fromStdString("tcp://"+Pothos::Util::getLoopbackAddr()));

    //sanitize duplicates
    QStringList noDups;
    for (const auto &uri : uris)
    {
        if (std::find(noDups.begin(), noDups.end(), uri) == noDups.end()) noDups.push_back(uri);
    }
    return noDups;
}

static void setHostUriList(const QStringList &uris)
{
    auto settings = MainSettings::global();
    settings->setValue("HostExplorer/uris", uris);
}

/***********************************************************************
 * Custom line editor for host URI entry
 **********************************************************************/
class HostUriQLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    HostUriQLineEdit(QWidget *parent):
        QLineEdit(parent)
    {
        this->setPlaceholderText(tr("Click to enter a new Host URI"));

        connect(
            this, SIGNAL(returnPressed(void)),
            this, SLOT(handleReturnPressed(void)));
    }

    QSize minimumSizeHint(void) const
    {
        auto size = QLineEdit::minimumSizeHint();
        size.setWidth(50);
        return size;
    }

public slots:
    void handleReturnPressed(void)
    {
        if (this->text() == getDefaultUri()) return;
        if (this->text().isEmpty()) return;
        emit handleUriEntered(this->text());
        this->setText("");
    }

signals:
    void handleUriEntered(const QString &);

private:
    void focusInEvent(QFocusEvent *)
    {
        if (this->text().isEmpty())
        {
            this->setText(getDefaultUri());
        }
    }

    void focusOutEvent(QFocusEvent *)
    {
        if (this->text() == getDefaultUri())
        {
            this->setText("");
        }
    }

    static QString getDefaultUri(void)
    {
        return "tcp://";
    }
};

/***********************************************************************
 * helper to disable cell editing for RO cells
 **********************************************************************/
static void disableEdit(QTableWidgetItem *i)
{
    if (i == nullptr) return;
    auto flags = i->flags();
    flags &= ~Qt::ItemIsEditable;
    i->setFlags(flags);
}

/***********************************************************************
 * helper to make tool buttons for add/remove nodes
 **********************************************************************/
static QToolButton *makeToolButton(QWidget *parent, const QString &theme)
{
    auto tb = new QToolButton(parent);
    tb->setCursor(Qt::PointingHandCursor);
    tb->setFocusPolicy(Qt::NoFocus);
    tb->setIcon(makeIconFromTheme(theme));
    tb->setStyleSheet("background: transparent; border: none;");
    return tb;
}

/***********************************************************************
 * node table implementation
 **********************************************************************/
HostSelectionTable::HostSelectionTable(QWidget *parent):
    QTableWidget(parent),
    _lineEdit(new HostUriQLineEdit(this)),
    _addButton(makeToolButton(this, "list-add")),
    _timer(new QTimer(this)),
    _watcher(new FutureWatcherT(this))
{
    this->setColumnCount(nCols);
    size_t col = 0;
    this->setHorizontalHeaderItem(col++, new QTableWidgetItem(tr("Action")));
    this->setHorizontalHeaderItem(col++, new QTableWidgetItem(tr("URI")));
    this->setHorizontalHeaderItem(col++, new QTableWidgetItem(tr("Name")));
    this->setHorizontalHeaderItem(col++, new QTableWidgetItem(tr("Last Access")));

    //create the data entry row
    this->setRowCount(1);
    this->setCellWidget(0, 0, _addButton);
    this->setCellWidget(0, 1, _lineEdit);
    this->setSpan(0, 1, 1, nCols-1);

    //connect entry related widgets
    connect(_addButton, &QToolButton::clicked, [=](void){_lineEdit->handleReturnPressed();});
    connect(_lineEdit, &HostUriQLineEdit::handleUriEntered, this, &HostSelectionTable::handleAdd);
    connect(_timer, &QTimer::timeout, this, &HostSelectionTable::handleUpdateStatus);
    connect(_watcher, &FutureWatcherT::finished, [=](void){this->reloadRows(_watcher->result());});
    connect(this, &HostSelectionTable::cellClicked, this, &HostSelectionTable::handleCellClicked);

    this->reloadTable();
    _timer->start(5000/*ms*/);
}

QStringList HostSelectionTable::hostUriList(void) const
{
    return getHostUriList();
}

void HostSelectionTable::handleCellClicked(const int row, const int col)
{
    if (col < 1) return;
    for (const auto &entry : _uriToRow)
    {
        if (entry.second != size_t(row)) continue;
        auto &info = _uriToInfo.at(entry.first);
        info.update();
        if (info.isOnline) emit hostInfoRequest(info.uri.toStdString());
        else this->showErrorMessage(tr("Host %1 is offline").arg(info.uri));
    }
}

void HostSelectionTable::handleRemove(const QString &uri)
{
    try
    {
        auto uris = getHostUriList();
        uris.erase(std::find(uris.begin(), uris.end(), uri));
        setHostUriList(uris);
        this->reloadTable();
        emit this->hostUriListChanged();
    }
    catch(const Pothos::Exception &ex)
    {
        this->showErrorMessage(QString::fromStdString(ex.displayText()));
    }
}

void HostSelectionTable::handleAdd(const QString &uri)
{
    try
    {
        auto uris = getHostUriList();
        if (std::find(uris.begin(), uris.end(), uri) != uris.end())
        {
            this->showErrorMessage(tr("%1 already exists").arg(uri));
            return;
        }
        uris.push_back(uri);
        setHostUriList(uris);
        this->reloadTable();
        emit this->hostUriListChanged();
    }
    catch(const Pothos::Exception &ex)
    {
        this->showErrorMessage(QString::fromStdString(ex.displayText()));
    }
}

void HostSelectionTable::handleUpdateStatus(void)
{
    //did the keys change?
    {
        auto uris = getHostUriList();
        bool changed = false;
        if (_uriToRow.size() != size_t(uris.size())) changed = true;
        for (const auto &uri : uris)
        {
            if (_uriToRow.count(uri) == 0) changed = true;
        }
        if (changed) return this->reloadTable();
    }

    std::vector<NodeInfo> nodes;
    for (const auto &entry : _uriToInfo)
    {
        nodes.push_back(entry.second);
    }
    this->reloadRows(nodes); //initial load, future will fill in the rest
    _watcher->setFuture(QtConcurrent::run(std::bind(&HostSelectionTable::peformNodeComms, nodes)));
}

std::vector<NodeInfo> HostSelectionTable::peformNodeComms(std::vector<NodeInfo> nodes)
{
    for (size_t i = 0; i < nodes.size(); i++) nodes[i].update();
    return nodes;
}

void HostSelectionTable::reloadRows(const std::vector<NodeInfo> &nodeInfos)
{
    for (size_t i = 0; i < nodeInfos.size(); i++)
    {
        const auto &info = nodeInfos[i];
        if (_uriToRow.find(info.uri) == _uriToRow.end()) continue;
        const size_t row = _uriToRow[info.uri];
        _uriToInfo[info.uri] = info;

        //gather information
        const auto timeStr = info.lastAccess.toString("h:mm:ss AP - MMM d yyyy");
        const auto accessTimeStr = QString(info.lastAccess.isNull()? tr("Never") : timeStr);
        QIcon statusIcon = makeIconFromTheme(
            info.isOnline?"network-transmit-receive":"network-offline");

        //load columns
        size_t col = 1;
        this->setItem(row, col++, new QTableWidgetItem(statusIcon, info.uri));
        this->setItem(row, col++, new QTableWidgetItem(info.nodeName));
        this->setItem(row, col++, new QTableWidgetItem(accessTimeStr));
        for (size_t c = 0; c < nCols; c++) disableEdit(this->item(row, c));
    }
    this->resizeColumnsToContents();
}

void HostSelectionTable::reloadTable(void)
{
    int row = 1;
    _uriToRow.clear();

    //enumerate the available hosts
    for (const auto &uri : getHostUriList())
    {
        this->setRowCount(row+1);
        _uriToInfo[uri].uri = uri;
        _uriToRow[uri] = row;
        auto removeButton = makeToolButton(this, "list-remove");
        this->setCellWidget(row, 0, removeButton);
        connect(removeButton, &QToolButton::clicked, [=](void){handleRemove(uri);});
        row++;
    }

    this->handleUpdateStatus(); //fills in status
}

void HostSelectionTable::showErrorMessage(const QString &errMsg)
{
    QToolTip::showText(_lineEdit->mapToGlobal(QPoint()), "<font color=\"red\">"+errMsg+"</font>");
}

#include "HostSelectionTable.moc"
