/*****************************************************************************
 * MediaContainer.cpp: Implements the library basics
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

#include <QHash>
#include <QUuid>

#include "Clip.h"
#include "MediaContainer.h"
#include "Media.h"
#include "MetaDataManager.h"

MediaContainer::MediaContainer( Clip* parent /*= NULL*/ ) : m_parent( parent )
{
}

Clip*
MediaContainer::clip( const QUuid& uuid )
{
    QHash<QUuid, Clip*>::iterator   it = m_clips.find( uuid );
    if ( it != m_clips.end() )
        return it.value();
    return NULL;
}

void
MediaContainer::addMedia( Media *media )
{
    m_clips[media->baseClip()->uuid()] = media->baseClip();
    emit newClipLoaded( media->baseClip() );
}

bool
MediaContainer::addMedia( const QFileInfo& fileInfo, const QString& uuid )
{
    if ( QFile::exists( fileInfo.absoluteFilePath() ) == false )
        return false;
    Media* media = new Media( fileInfo.filePath(), uuid );

    foreach( Clip* it, m_clips.values() )
    {
        if ( it->getMedia()->fileInfo()->filePath() == media->fileInfo()->filePath() )
        {
            delete media;
            return false;
        }
    }
    MetaDataManager::getInstance()->computeMediaMetadata( media );
    addMedia( media );
    return true;
}

bool
MediaContainer::mediaAlreadyLoaded( const QFileInfo& fileInfo )
{
    foreach( Clip* clip, m_clips.values() )
    {
        if ( clip->getMedia()->fileInfo()->filePath() == fileInfo.filePath() )
            return true;
    }
    return false;
}

void
MediaContainer::addClip( Clip* clip )
{
    m_clips[clip->uuid()] = clip;
    emit newClipLoaded( clip );
}

void
MediaContainer::clear()
{
    QHash<QUuid, Clip*>::iterator  it = m_clips.begin();
    QHash<QUuid, Clip*>::iterator  end = m_clips.end();

    while ( it != end )
    {
        emit clipRemoved( it.value() );
        it.value()->clear();
        it.value()->deleteLater();
        ++it;
    }
    m_clips.clear();
}

void
MediaContainer::removeAll()
{
    QHash<QUuid, Clip*>::iterator  it = m_clips.begin();
    QHash<QUuid, Clip*>::iterator  end = m_clips.end();

    while ( it != end )
    {
        emit clipRemoved( it.value() );
        it.value()->clear();
        ++it;
    }
    m_clips.clear();
}

Clip*
MediaContainer::removeClip( const QUuid &uuid )
{
    QHash<QUuid, Clip*>::iterator  it = m_clips.find( uuid );
    if ( it != m_clips.end() )
    {
        Clip* clip = it.value();
        m_clips.remove( uuid );
        emit clipRemoved( clip );
        return clip;
    }
    return NULL;
}

Clip*
MediaContainer::removeClip( const Clip* clip )
{
    return removeClip( clip->uuid() );
}

const QHash<QUuid, Clip*>&
MediaContainer::clips() const
{
    return m_clips;
}

Clip*
MediaContainer::getParent()
{
    return m_parent;
}

quint32
MediaContainer::count() const
{
    return m_clips.size();
}