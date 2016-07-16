// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Config.hpp>
#include <QFormLayout>

//! make a form layout with consistent style across platforms
static inline QFormLayout *makeFormLayout(QWidget *parent = nullptr)
{
    auto layout = new QFormLayout(parent);
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setLabelAlignment(Qt::AlignLeft);
    layout->setContentsMargins(6, 6, 6, 6);
    return layout;
}
