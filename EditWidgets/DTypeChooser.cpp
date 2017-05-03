// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework/DType.hpp>
#include <Pothos/Plugin.hpp>
#include <QJsonObject>
#include <QJsonArray>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <limits>

/***********************************************************************
 * DTypeChooser for data type entry
 **********************************************************************/
class DTypeChooser : public QWidget
{
    Q_OBJECT
public:
    DTypeChooser(QWidget *parent, const bool editDimension):
        QWidget(parent),
        _comboBox(new QComboBox(this)),
        _spinBox(nullptr)
    {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(QMargins());

        layout->addWidget(_comboBox, 1);
        connect(_comboBox, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(handleWidgetChanged(const QString &)));
        connect(_comboBox, SIGNAL(editTextChanged(const QString &)), this, SLOT(handleEntryChanged(const QString &)));

        if (editDimension)
        {
            _spinBox = new QSpinBox(this);
            layout->addWidget(_spinBox, 0);
            _spinBox->setPrefix("x");
            _spinBox->setMinimum(1);
            _spinBox->setMaximum(std::numeric_limits<int>::max());
            connect(_spinBox, SIGNAL(editingFinished(void)), this, SIGNAL(widgetChanged(void)));
            connect(_spinBox, SIGNAL(valueChanged(const QString &)), this, SLOT(handleWidgetChanged(const QString &)));
        }

        _comboBox->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
        _comboBox->view()->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
    }

    QComboBox *comboBox(void) const
    {
        return _comboBox;
    }

public slots:
    QString value(void) const
    {
        //get the combo box name
        QString name;
        const auto index = _comboBox->currentIndex();
        if (index < 0 or _comboBox->currentText() != _comboBox->itemText(index)) name = _comboBox->currentText();
        else name = _comboBox->itemData(index).toString();

        //get the dimension
        size_t dimension = 1;
        if (_spinBox != nullptr)
        {
            dimension = _spinBox->value();
        }

        //return quoted data type markup
        const Pothos::DType dtype(unQuote(name).toStdString(), dimension);
        return QString("\"%1\"").arg(QString::fromStdString(dtype.toMarkup()));
    }

    void setValue(const QString &value)
    {
        //parse to data type
        Pothos::DType dtype;
        try
        {
            dtype = Pothos::DType(unQuote(value).toStdString());
        }
        catch(...){}
        const auto name = QString("\"%1\"").arg(QString::fromStdString(dtype.name()));

        //set combo box with type name
        int index = -1;
        for (int i = 0; i < _comboBox->count(); i++)
        {
            if (_comboBox->itemData(i).toString() == name) index = i;
        }
        if (index < 0) _comboBox->setEditText(name);
        else _comboBox->setCurrentIndex(index);

        //set spin box with vector size
        if (_spinBox != nullptr)
        {
            _spinBox->setValue(dtype.dimension());
        }
    }

signals:
    void commitRequested(void);
    void widgetChanged(void);
    void entryChanged(void);

private slots:
    void handleWidgetChanged(const QString &)
    {
        emit this->widgetChanged();
    }
    void handleEntryChanged(const QString &)
    {
        emit this->entryChanged();
    }

private:
    static QString unQuote(const QString &s)
    {
        if (s.startsWith("\"") and s.endsWith("\""))
        {
            return s.midRef(1, s.size()-2).toString();
        }
        return s;
    }

    QComboBox *_comboBox;
    QSpinBox *_spinBox;
};

/***********************************************************************
 * Factory function and registration
 **********************************************************************/
static QWidget *makeDTypeChooser(const QJsonArray &, const QJsonObject &kwargs, QWidget *parent)
{
    const bool editDimension = (kwargs["dim"].toInt(0)==0)?false:true;

    auto dtypeChooser = new DTypeChooser(parent, editDimension);
    auto comboBox = dtypeChooser->comboBox();
    for (int mode = 0; mode <= 1; mode++)
    {
        const QString keyPrefix((mode == 0)? "c":"");
        const QString namePrefix((mode == 0)? "Complex ":"");
        const QString aliasPrefix((mode == 0)? "complex_":"");
        for (int bytes = 64; bytes >= 32; bytes /= 2)
        {
            if (kwargs.contains(keyPrefix+"float")) comboBox->addItem(QString("%1Float%2").arg(namePrefix).arg(bytes), QString("\"%1float%2\"").arg(aliasPrefix).arg(bytes));
        }
        for (int bytes = 64; bytes >= 8; bytes /= 2)
        {
            if (kwargs[keyPrefix+"int"].toInt(0)) comboBox->addItem(QString("%1Int%2").arg(namePrefix).arg(bytes), QString("\"%1int%2\"").arg(aliasPrefix).arg(bytes));
            if (kwargs[keyPrefix+"uint"].toInt(0)) comboBox->addItem(QString("%1UInt%2").arg(namePrefix).arg(bytes), QString("\"%1uint%2\"").arg(aliasPrefix).arg(bytes));
        }
    }
    return dtypeChooser;
}

pothos_static_block(registerDTypeChooser)
{
    Pothos::PluginRegistry::add("/gui/EntryWidgets/DTypeChooser", Pothos::Callable(&makeDTypeChooser));
}

#include "DTypeChooser.moc"
