/*****************************************************************************
 * ProjectManager.cpp: Manager the project loading and saving.
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <hugo@vlmc.org>
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

#include "config.h"

#ifdef WITH_GUI
# ifdef WITH_CRASHHANDLER_GUI
#  include "CrashHandler.h"
# endif
#else
//We shouldn't have to do this.
#undef WITH_CRASHHANDLER_GUI
#endif

#include "Library.h"
#include "MainWorkflow.h"
#include "project/GuiProjectManager.h"
#include "ProjectManager.h"
#include "SettingsManager.h"

#include <QDir>
#include <QSettings>
#include <QtDebug>
#include <QXmlStreamWriter>

#include <errno.h>
#include <signal.h>

#ifdef WITH_CRASHHANDLER
void    ProjectManager::signalHandler( int sig )
{
    signal( sig, SIG_DFL );

#ifdef WITH_GUI
    GUIProjectManager::getInstance()->emergencyBackup();
#else
    ProjectManager::getInstance()->emergencyBackup();
#endif

    #ifdef WITH_CRASHHANDLER_GUI
        CrashHandler* ch = new CrashHandler( sig );
        ::exit( ch->exec() );
    #else
        ::exit( 1 );
    #endif
}
#endif

const QString   ProjectManager::unNamedProject = tr( "<Unnamed project>" );
const QString   ProjectManager::unSavedProject = tr( "<Unsaved project>" );

ProjectManager::ProjectManager() : m_projectFile( NULL ), m_needSave( false )
{
    QSettings s;
    m_recentsProjects = s.value( "RecentsProjects" ).toStringList();

#ifdef WITH_CRASHHANDLER
    signal( SIGSEGV, ProjectManager::signalHandler );
    signal( SIGFPE, ProjectManager::signalHandler );
    signal( SIGABRT, ProjectManager::signalHandler );
    signal( SIGILL, ProjectManager::signalHandler );
#endif

    VLMC_CREATE_PROJECT_DOUBLE( "video/VLMCOutputFPS", 29.97, "Output video FPS", "Frame Per Second used when previewing and rendering the project" );
    VLMC_CREATE_PROJECT_INT( "video/VideoProjectWidth", 480, "Video width", "Width resolution of the output video" );
    VLMC_CREATE_PROJECT_INT( "video/VideoProjectHeight", 300, "Video height", "Height resolution of the output video" );
    VLMC_CREATE_PROJECT_INT( "audio/AudioSampleRate", 0, "Audio samplerate", "Output project audio samplerate" );
    VLMC_CREATE_PROJECT_STRING( "general/VLMCWorkspace", QDir::homePath(), "Workspace location", "The place where all project's videos will be stored" );

    VLMC_CREATE_PROJECT_STRING( "general/ProjectName", unNamedProject, "Project name", "The project name" );
    SettingsManager::getInstance()->watchValue( "general/ProjectName", this,
                                                SLOT(projectNameChanged(QVariant) ),
                                                SettingsManager::Project );

}

ProjectManager::~ProjectManager()
{
    // Write uncommited change to the disk
    QSettings s;
    s.sync();

    if ( m_projectFile != NULL )
        delete m_projectFile;
}


QStringList ProjectManager::recentsProjects() const
{
    return m_recentsProjects;
}

void    ProjectManager::loadTimeline()
{
    QDomElement     root = m_domDocument->documentElement();

    MainWorkflow::getInstance()->loadProject( root );
    SettingsManager::getInstance()->load( root );
    emit projectUpdated( projectName(), true );
    emit projectLoaded();
    delete m_domDocument;
}

void    ProjectManager::loadProject( const QString& fileName )
{
    if ( fileName.isEmpty() == true )
        return;

    if ( closeProject() == false )
        return ;
    m_projectFile = new QFile( fileName );
    m_projectFile->open( QFile::ReadOnly );
    m_projectFile->close();

    m_domDocument = new QDomDocument;
    m_domDocument->setContent( m_projectFile );
#ifdef WITH_GUI
    m_needSave = false;
#endif
    if ( ProjectManager::isBackupFile( fileName ) == false )
        appendToRecentProject( QFileInfo( *m_projectFile ).absoluteFilePath() );
    else
    {
        //Delete the project file representation, so the next time the user
        //saves its project, vlmc will ask him where to save it.
        delete m_projectFile;
        m_projectFile = NULL;
    }

    QDomElement     root = m_domDocument->documentElement();

    connect( Library::getInstance(), SIGNAL( projectLoaded() ), this, SLOT( loadTimeline() ) );
    Library::getInstance()->loadProject( root );
}

void    ProjectManager::__saveProject( const QString &fileName )
{
    QFile               file( fileName );
    file.open( QFile::WriteOnly );

    QXmlStreamWriter    project( &file );

    project.setAutoFormatting( true );
    project.writeStartDocument();
    project.writeStartElement( "vlmc" );

    Library::getInstance()->saveProject( project );
    MainWorkflow::getInstance()->saveProject( project );
    SettingsManager::getInstance()->save( project );

    project.writeEndElement();
    project.writeEndDocument();
}

void    ProjectManager::emergencyBackup()
{
    QString     name;

    if ( m_projectFile != NULL )
    {
        name = m_projectFile->fileName();
        name += "backup";
        __saveProject( name );
    }
    else
    {
       name = QDir::currentPath() + "/unsavedproject.vlmcbackup";
        __saveProject( name );
    }
    QSettings   s;
    s.setValue( "EmergencyBackup", name );
    s.sync();
}

bool    ProjectManager::isBackupFile( const QString& projectFile )
{
    return projectFile.endsWith( "backup" );
}

void    ProjectManager::appendToRecentProject( const QString& projectFile )
{
    // Append the item to the recents list
    m_recentsProjects.removeAll( projectFile );
    m_recentsProjects.prepend( projectFile );
    while ( m_recentsProjects.count() > 15 )
        m_recentsProjects.removeLast();

    QSettings s;
    s.setValue( "RecentsProjects", m_recentsProjects );
}

QString ProjectManager::projectName() const
{
    if ( m_projectName.isEmpty() == true )
    {
        if ( m_projectFile != NULL )
        {
            QFileInfo       fInfo( *m_projectFile );
            return fInfo.baseName();
        }
        return ProjectManager::unSavedProject;
    }
    return m_projectName;
}

bool
ProjectManager::closeProject()
{
    if ( m_projectFile != NULL )
    {
        delete m_projectFile;
        m_projectFile = NULL;
    }
    m_projectName = QString();
    //This one is for the mainwindow, to update the title bar
    emit projectUpdated( projectName(), true );
    //This one is for every part that need to clean something when the project is closed.
    emit projectClosed();
    return true;
}

void
ProjectManager::saveProject( bool )
{
    __saveProject( m_projectFile->fileName() );
    emit projectSaved();
    emit projectUpdated( projectName(), true );
}

bool
ProjectManager::loadEmergencyBackup()
{
    QSettings   s;
    QString lastProject = s.value( "EmergencyBackup" ).toString();
    if ( QFile::exists( lastProject ) == true )
    {
        loadProject(  lastProject );
        m_needSave = true;
        return true;
    }
    return false;
}
