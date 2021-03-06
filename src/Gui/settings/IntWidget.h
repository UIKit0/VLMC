/*****************************************************************************
 * IntWidget.h Handle integer settings.
 *****************************************************************************
 * Copyright (C) 2008-2014 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef INTWIDGET_H
#define INTWIDGET_H

#include "ISettingsCategoryWidget.h"
#include <stddef.h>

class   SettingValue;

class   QSpinBox;

class   IntWidget : public ISettingsCategoryWidget
{
    Q_OBJECT

    public:
        IntWidget( SettingValue *s, QWidget *parent = NULL );
        bool                    save();

    private slots:
        virtual void            changed( const QVariant& );

    private:
        QSpinBox                *m_spinbox;
};

#endif // INTWIDGET_H
