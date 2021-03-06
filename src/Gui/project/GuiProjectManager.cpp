/*****************************************************************************
 * GUIProjectManager.cpp: Handle the GUI part of the project managing
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

#include <QFileDialog>
#include <QMessageBox>
#include <QObject>

#include "GuiProjectManager.h"

bool
GUIProjectManager::shouldLoadBackupFile()
{
    return QMessageBox::question( NULL, QObject::tr( "Backup file" ),
                                            QObject::tr( "A backup file exists for this project. "
                                            "Do you want to load it?" ),
                                            QMessageBox::Ok | QMessageBox::No ) == QMessageBox::Ok;
}

bool
GUIProjectManager::shouldDeleteOutdatedBackupFile()
{
    return QMessageBox::question( NULL, QObject::tr( "Backup file" ),
                                    QObject::tr( "An outdated backup file was found. "
                                   "Do you want to erase it?" ),
                                  QMessageBox::Ok | QMessageBox::No ) == QMessageBox::Ok;
}

QString
GUIProjectManager::getProjectFileDestination( const QString &defaultPath )
{
    return QFileDialog::getSaveFileName( NULL, QObject::tr( "Enter the output file name" ),
                                  defaultPath, QObject::tr( "VLMC project file(*.vlmc)" ) );
}



IProjectUiCb::SaveMode
GUIProjectManager::shouldSaveBeforeClose()
{
    QMessageBox msgBox;
    msgBox.setText( QObject::tr( "The project has been modified." ) );
    msgBox.setInformativeText( QObject::tr( "Do you want to save it?" ) );
    msgBox.setStandardButtons( QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );
    msgBox.setDefaultButton( QMessageBox::Save );
    int     ret = msgBox.exec();
    switch ( ret )
    {
        case QMessageBox::Save:
            return Save;
        case QMessageBox::Discard:
            return Discard;
        case QMessageBox::Cancel:
        default:
            return Cancel;
    }
}

