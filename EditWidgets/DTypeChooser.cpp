// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework/DType.hpp>
#include <Pothos/Plugin.hpp>
#include <Poco/JSON/Object.h>
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
    DTypeChooser(QWidget *parent, const bool enableVector):
        QWidget(parent),
        _comboBox(new QComboBox(this)),
        _spinBox(nullptr)
    {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(_comboBox);
        connect(_comboBox, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(handleWidgetChanged(const QString &)));
        connect(_comboBox, SIGNAL(editTextChanged(const QString &)), this, SLOT(handleEntryChanged(const QString &)));

        if (enableVector)
        {
            _spinBox = new QSpinBox(this);
            layout->addWidget(_spinBox);
            _spinBox->setMaximumSize(40, _spinBox->height());
            _spinBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            _spinBox->setPrefix("x");
            _spinBox->setMinimum(1);
            _spinBox->setMaximum(std::numeric_limits<int>::max());
            connect(_spinBox, SIGNAL(editingFinished(void)), this, SIGNAL(widgetChanged(void)));
            connect(_spinBox, SIGNAL(valueChanged(const QString &)), this, SLOT(handleWidgetChanged(const QString &)));
        }

        this->setObjectName("BlockPropertiesEditWidget"); //to pick up eval color style
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
static QWidget *makeDTypeChooser(const Poco::JSON::Object::Ptr &paramDesc, QWidget *parent)
{
    Poco::JSON::Object::Ptr widgetKwargs(new Poco::JSON::Object());
    if (paramDesc->has("widgetKwargs")) widgetKwargs = paramDesc->getObject("widgetKwargs");

    const bool enableVector = widgetKwargs->optValue<bool>("vector", false);

    auto dtypeChooser = new DTypeChooser(parent, enableVector);
    auto comboBox = dtypeChooser->comboBox();
    for (int mode = 0; mode <= 1; mode++)
    {
        const std::string keyPrefix((mode == 0)? "c":"");
        const QString namePrefix((mode == 0)? "Complex ":"");
        const QString aliasPrefix((mode == 0)? "complex_":"");
        for (int bytes = 64; bytes >= 32; bytes /= 2)
        {
            if (widgetKwargs->has(keyPrefix+"float")) comboBox->addItem(QString("%1Float%2").arg(namePrefix).arg(bytes), QString("\"%1float%2\"").arg(aliasPrefix).arg(bytes));
        }
        for (int bytes = 64; bytes >= 8; bytes /= 2)
        {
            if (widgetKwargs->has(keyPrefix+"int")) comboBox->addItem(QString("%1Int%2").arg(namePrefix).arg(bytes), QString("\"%1int%2\"").arg(aliasPrefix).arg(bytes));
            if (widgetKwargs->has(keyPrefix+"uint")) comboBox->addItem(QString("%1UInt%2").arg(namePrefix).arg(bytes), QString("\"%1uint%2\"").arg(aliasPrefix).arg(bytes));
        }
    }
    return dtypeChooser;
}

pothos_static_block(registerComboBox)
{
    Pothos::PluginRegistry::add("/gui/EntryWidgets/DTypeChooser", Pothos::Callable(&makeDTypeChooser));
}

#include "DTypeChooser.moc"
