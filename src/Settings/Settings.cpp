/*****************************************************************************
 * Settings.cpp: Backend settings manager
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

#include "Settings.h"
#include "SettingValue.h"
#include "Tools/VlmcDebug.h"

#include <QByteArray>
#include <QFile>
#include <QWriteLocker>
#include <QReadLocker>
#include <QStringList>
#include <QXmlStreamWriter>
#include <QFileInfo>
#include <QDir>

#include <QDomElement>


Settings::Settings(const QString &settingsFile)
    : m_settingsFile( NULL )
{
    setSettingsFile( settingsFile );
}

Settings::~Settings()
{
    delete m_settingsFile;
}

void
Settings::setSettingsFile(const QString &settingsFile)
{
    delete m_settingsFile;
    if ( settingsFile.isEmpty() == false )
    {
        QFileInfo fInfo( settingsFile );
        if ( fInfo.exists() == false )
        {
            QDir dir = fInfo.dir();
            if ( dir.exists() == false )
                dir.mkpath( fInfo.absolutePath() );
        }
        m_settingsFile = new QFile( settingsFile );
    }
    else
        m_settingsFile = NULL;
}

bool
Settings::save( QXmlStreamWriter& project )
{
    QReadLocker lock( &m_rwLock );

    SettingMap::const_iterator     it = m_settings.begin();
    SettingMap::const_iterator     end = m_settings.end();

    project.writeStartElement( "settings" );
    for ( ; it != end; ++it )
    {
        if ( ( (*it)->flags() & SettingValue::Runtime ) != 0 )
            continue ;
        project.writeStartElement( "setting" );
        project.writeAttribute( "key", (*it)->key() );
        if ( (*it)->get().canConvert<QString>() == false )
            vlmcWarning() << "Can't serialize" << (*it)->key();
        project.writeAttribute( "value", (*it)->get().toString() );
        project.writeEndElement();
    }
    project.writeEndElement();
    return true;
}

bool
Settings::load( const QDomDocument& document )
{
    QDomElement     element = document.firstChildElement( "settings" );
    if ( element.isNull() == true )
    {
        vlmcWarning() << "Invalid settings node";
        return false;
    }
    QDomElement s = element.firstChildElement( "setting" );
    while ( s.isNull() == false )
    {
        QString     key = s.attribute( "key" );
        QString     value = s.attribute( "value" );

        if ( key.isEmpty() == true )
            vlmcWarning() << "Invalid setting node.";
        else
        {
            vlmcDebug() << "Loading" << key << "=>" << value;
            if ( setValue( key, value ) == false )
                vlmcWarning() << "Loaded invalid project setting:" << key;
        }
        s = s.nextSiblingElement();
    }
    return true;
}

bool
Settings::load()
{
    QDomDocument    doc("root");
    if ( m_settingsFile->open( QFile::ReadOnly ) == false )
    {
        vlmcWarning() << "Failed to open settings file" << m_settingsFile->fileName();
        return false;
    }
    if ( doc.setContent( m_settingsFile ) == false )
    {
        vlmcWarning() << "Failed to load settings file" << m_settingsFile->fileName();
        return false;
    }
    bool res = load( doc );
    m_settingsFile->close();
    return res;
}

bool
Settings::save()
{
    if ( m_settingsFile == NULL )
        return false;
    QByteArray          settingsContent;
    QXmlStreamWriter    streamWriter( &settingsContent );
    streamWriter.setAutoFormatting( true );
    save( streamWriter );
    m_settingsFile->open( QFile::WriteOnly );
    m_settingsFile->write( settingsContent );
    m_settingsFile->close();
    return true;
}

bool
Settings::setValue(const QString &key, const QVariant &value)
{
    SettingMap::iterator   it = m_settings.find( key );
    if ( it != m_settings.end() )
    {
        (*it)->set( value );
        return true;
    }
    Q_ASSERT_X( false, __FILE__, "setting value without a created variable" );
    return false;
}

SettingValue*
Settings::value(const QString &key)
{
    QReadLocker lock( &m_rwLock );

    SettingMap::iterator it = m_settings.find( key );
    if ( it != m_settings.end() )
        return *it;
    Q_ASSERT_X( false, __FILE__, "fetching value without a created variable" );
    return NULL;
}

SettingValue*
Settings::createVar(SettingValue::Type type, const QString &key, const QVariant &defaultValue, const char *name, const char *desc, SettingValue::Flags flags)
{
    QWriteLocker lock( &m_rwLock );

    if ( m_settings.contains( key ) )
        return NULL;
    SettingValue* val = new SettingValue( key, type, defaultValue, name, desc, flags );
    m_settings.insert( key, val );
    return val;
}

Settings::SettingList
Settings::group(const QString &groupName) const
{
    QReadLocker lock( &m_rwLock );
    SettingMap::const_iterator          it = m_settings.begin();
    SettingMap::const_iterator          ed = m_settings.end();
    SettingList        ret;

    QString grp = groupName + '/';
    for ( ; it != ed; ++it )
    {
        if ( (*it)->key().startsWith( grp ) )
            ret.push_back( *it );
    }
    return ret;
}
