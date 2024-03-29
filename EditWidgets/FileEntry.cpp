// Copyright (c) 2014-2021 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>

/***********************************************************************
 * Custom widget for file path entry
 **********************************************************************/
class FileEntry : public QWidget
{
    Q_OBJECT
public:
    FileEntry(const QString &mode, QWidget *parent):
        QWidget(parent),
        _mode(mode),
        _button(new QPushButton(QChar(0x2026), this)), //&hellip;
        _edit(new QLineEdit(this))
    {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->addWidget(_edit, 1);
        layout->addWidget(_button, 0, Qt::AlignRight);
        _button->setMaximumWidth(20);
        connect(_button, &QPushButton::pressed, this, &FileEntry::handlePressed);
        connect(_edit, &QLineEdit::textEdited, [=](const QString &){emit this->entryChanged();});
        connect(_edit, &QLineEdit::returnPressed, this, &FileEntry::commitRequested);
        _edit->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
    }

public slots:
    QString value(void) const
    {
        auto s = _edit->text();
        return QString("\"%1\"").arg(s.replace("\"", "\\\"")); //escape
    }

    void setValue(const QString &s)
    {
        if (s.startsWith("\"") and s.endsWith("\""))
        {
            auto s0 = s.mid(1, s.size()-2);
            _edit->setText(s0.replace("\\\"", "\"")); //unescape
        }
        else _edit->setText(s);
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handlePressed(void)
    {
        QString filePath;
        if (_mode == "open") filePath = QFileDialog::getOpenFileName(this);
        if (_mode == "save") filePath = QFileDialog::getSaveFileName(this);
        if (_mode == "directory") filePath = QFileDialog::getExistingDirectory(this);
        if (filePath.isEmpty()) return;
        this->setValue(filePath);
        emit this->widgetChanged();
    }

private:
    const QString _mode;
    QPushButton *_button;
    QLineEdit *_edit;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeFileEntry(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    return new FileEntry(kwargs["mode"].toString("save"), parent);
}

pothos_static_block(registerFileEntry)
{
    Pothos::PluginRegistry::add("/flow/EntryWidgets/FileEntry", Pothos::Callable(&makeFileEntry));
}

#include "FileEntry.moc"
