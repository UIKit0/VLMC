/*****************************************************************************
 * EffectNode.cpp: Node of the patch
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Vincent Carrubba <cyberbouba@gmail.com>
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

#include "EffectNode.h"

#include "IEffectNode.h"
#include "IEffectPlugin.h"

#include <QObject>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QString>
#include <QWriteLocker>

EffectNodeFactory              EffectNode::s_renf;
QReadWriteLock                 EffectNode::s_srwl( QReadWriteLock::Recursive );

template class SemanticObjectManager< InSlot<LightVideoFrame> >;
template class SemanticObjectManager< OutSlot<LightVideoFrame> >;

// template class SemanticObjectManager<InSlot<AudioSoundSample> >;
// template class SemanticObjectManager<OutSlot<AudioSoundSample> >;

// template class SemanticObjectManager<InSlot<qreal> >;
// template class SemanticObjectManager<OutSlot<qreal> >;


//============================================================================================//
//                                      CTORS & DTOR                                          //
//============================================================================================//


EffectNode::EffectNode( IEffectPlugin* plugin ) : m_rwl( QReadWriteLock::Recursive ),
                                                  m_father( NULL ), m_plugin( plugin ),
                                                  m_visited( false )
{
    m_staticVideosInputs.setFather( this );
    m_staticVideosOutputs.setFather( this );
    m_staticVideosInputs.setScope( false );
    m_staticVideosOutputs.setScope( false );
    m_internalsStaticVideosInputs.setFather( this );
    m_internalsStaticVideosOutputs.setFather( this );
    m_internalsStaticVideosInputs.setScope( true );
    m_internalsStaticVideosOutputs.setScope( true );
    m_enf.setFather( this );
    m_plugin->init( this );
}


EffectNode::EffectNode() : m_father( NULL ),
                           m_plugin( NULL ),
                           m_visited( false )
{
    m_staticVideosInputs.setFather( this );
    m_staticVideosOutputs.setFather( this );
    m_staticVideosInputs.setScope( false );
    m_staticVideosOutputs.setScope( false );
    m_internalsStaticVideosInputs.setFather( this );
    m_internalsStaticVideosOutputs.setFather( this );
    m_internalsStaticVideosInputs.setScope( true );
    m_internalsStaticVideosOutputs.setScope( true );
    m_enf.setFather( this );
}

EffectNode::~EffectNode()
{
    delete m_plugin;
}


//============================================================================================//
//                                       RENDER & CO                                          //
//============================================================================================//


void
EffectNode::render( void )
{
    if ( m_plugin != NULL )
        m_plugin->render();
    else
    {
        if ( m_father != NULL)
        {
            QWriteLocker                        wl( &m_rwl );
            transmitDatasFromInputsToInternalsOutputs();
            renderSubNodes();
            transmitDatasFromInternalsInputsToOutputs();
            resetAllChildsNodesVisitState();
        }
        else
        {
            QWriteLocker                        wl( &m_rwl );
            renderSubNodes();
            resetAllChildsNodesVisitState();
        }
    }
}

void
EffectNode::renderSubNodes( void )
{
    QList<EffectNode*>                                effectsNodes = m_enf.getEffectNodeInstancesList();
    QList<EffectNode*>::iterator                      effectsNodesIt = effectsNodes.begin();
    QList<EffectNode*>::iterator                      effectsNodesEnd = effectsNodes.end();
    QList<OutSlot<LightVideoFrame>*>                  intOuts = m_connectedInternalsStaticVideosOutputs.getObjectsReferencesList() ;
    QList<OutSlot<LightVideoFrame>*>::iterator        intOutsIt = intOuts.begin();
    QList<OutSlot<LightVideoFrame>*>::iterator        intOutsEnd = intOuts.end();
    QQueue<EffectNode*>                               nodeQueue;
    EffectNode*                                       toQueueNode;
    EffectNode*                                       currentNode;
    InSlot<LightVideoFrame>*                          currentIn;

    for ( ; effectsNodesIt != effectsNodesEnd; ++effectsNodesIt )
    {
        if (
            ( (*effectsNodesIt)->getNBConnectedStaticsVideosInputs() == 0 ) &&
            ( (*effectsNodesIt)->getNBConnectedStaticsVideosOutputs() > 0 )
            )
        {
            (*effectsNodesIt)->setVisited();
            nodeQueue.enqueue( (*effectsNodesIt) );
        }
    }
    for ( ; intOutsIt != intOutsEnd; ++intOutsIt )
    {
        currentIn = (*intOutsIt)->getInSlotPtr();
        toQueueNode = currentIn->getPrivateFather();
        if ((toQueueNode != this) && ( toQueueNode->wasItVisited() == false ))
        {
            toQueueNode->setVisited();
            nodeQueue.enqueue( toQueueNode );
        }
    }

    while ( nodeQueue.empty() == false )
    {
        currentNode = nodeQueue.dequeue();
        QList<OutSlot<LightVideoFrame>*>                  outs = currentNode->getConnectedStaticsVideosOutputsList();
        QList<OutSlot<LightVideoFrame>*>::iterator        outsIt = outs.begin();
        QList<OutSlot<LightVideoFrame>*>::iterator        outsEnd = outs.end();

        currentNode->render();
        for ( ; outsIt != outsEnd; ++outsIt )
        {
            currentIn = (*outsIt)->getInSlotPtr();
            toQueueNode = currentIn->getPrivateFather();
            if ((toQueueNode != this) && ( toQueueNode->wasItVisited() == false ))
            {
                toQueueNode->setVisited();
                nodeQueue.enqueue( toQueueNode );
            }
        }
    }
}

void
EffectNode::transmitDatasFromInputsToInternalsOutputs( void )
{
    if ( m_staticVideosInputs.getNBObjects() != 0 )
    {
        QList<InSlot<LightVideoFrame>*>             ins = m_staticVideosInputs.getObjectsList();
        QList<OutSlot<LightVideoFrame>*>            intOuts = m_internalsStaticVideosOutputs.getObjectsList();
        QList<InSlot<LightVideoFrame>*>::iterator  insIt = ins.begin();
        QList<InSlot<LightVideoFrame>*>::iterator  insEnd = ins.end();
        QList<OutSlot<LightVideoFrame>*>::iterator  intOutsIt = intOuts.begin();
        QList<OutSlot<LightVideoFrame>*>::iterator  intOutsEnd = intOuts.end();

        for ( ; ( insIt != insEnd ) && ( intOutsIt != intOutsEnd ); ++insIt, ++intOutsIt )
            *(*intOutsIt) << *(*insIt);
    }

}

void
EffectNode::transmitDatasFromInternalsInputsToOutputs( void )
{
    if ( m_staticVideosOutputs.getNBObjects() != 0 )
    {
        QList<InSlot<LightVideoFrame>*>             intIns = m_internalsStaticVideosInputs.getObjectsList();
        QList<OutSlot<LightVideoFrame>*>            outs = m_staticVideosOutputs.getObjectsList();
        QList<InSlot<LightVideoFrame>*>::iterator  intInsIt = intIns.begin();
        QList<InSlot<LightVideoFrame>*>::iterator  intInsEnd = intIns.end();
        QList<OutSlot<LightVideoFrame>*>::iterator  outsIt = outs.begin();
        QList<OutSlot<LightVideoFrame>*>::iterator  outsEnd = outs.end();

        for ( ; ( intInsIt != intInsEnd ) && ( outsIt != outsEnd ); ++intInsIt, ++outsIt )
            *(*outsIt) << *(*intInsIt);
    }
}

void
EffectNode::resetAllChildsNodesVisitState( void )
{
    QList<EffectNode*>            childs = m_enf.getEffectNodeInstancesList();

    if ( childs.empty() == false )
    {
        QList<EffectNode*>::iterator  it = childs.begin();
        QList<EffectNode*>::iterator  end = childs.end();

        for ( ; it != end; ++it)
            (*it)->resetVisitState();
    }
}

void
EffectNode::setVisited( void )
{
    QWriteLocker wl( &m_rwl );
    m_visited = true;
}

void
EffectNode::resetVisitState( void )
{
    QWriteLocker wl( &m_rwl );
    m_visited = false;
}

bool
EffectNode::wasItVisited( void ) const
{
    QReadLocker rl( &m_rwl );
    return  m_visited;
}


//============================================================================================//
//                                  CONNECTIONS MANAGEMENT                                    //
//============================================================================================//



bool
EffectNode::primitiveConnection( quint32 outId,
                                 const QString &outName,
                                 bool outInternal,
                                 quint32 inId,
                                 const QString &inName,
                                 bool inInternal,
                                 quint32 nodeId,
                                 const QString &nodeName )
{
    OutSlot<LightVideoFrame>*   out;
    EffectNode*                 dest;
    InSlot<LightVideoFrame>*    in;

    /**
     * Get output, destination node and input
     */

    if ( outInternal == true )
    {
        if ( outId == 0 && outName.isEmpty() == false )
        {
            if ( ( out = m_internalsStaticVideosOutputs.getObject( outName ) ) == NULL )
                return false;
        }
        else if ( outId != 0 && outName.isEmpty() == true )
        {
            if ( ( out = m_internalsStaticVideosOutputs.getObject( outId ) ) == NULL )
                return false;
        }
        else
            return false;

        if ( inInternal == true )
        {

            /**
             * Connection internal to internal
             */

            dest = this;

            if ( nodeId != 0 || nodeName.size() != 0 )
                return false;

            if ( inId == 0 && inName.size() != 0 )
            {
                if ( ( in = m_internalsStaticVideosInputs.getObject( inName ) ) == NULL )
                    return false;
            }
            else if ( inId != 0 && inName.size() == 0 )
            {
                if ( ( in = m_internalsStaticVideosInputs.getObject( inId ) ) == NULL )
                    return false;
            }
            else
                return false;
        }
        else
        {

            /**
             * Connection internal to external ( so parent to child )
             */

            if ( nodeId == 0 && nodeName.size() != 0 )
            {
                if ( ( dest = m_enf.getEffectNodeInstance( nodeName ) ) == NULL )
                    return false;
            }
            else if ( nodeId != 0 && nodeName.size() == 0 )
            {
                if ( ( dest = m_enf.getEffectNodeInstance( nodeId ) ) == NULL )
                    return false;
            }
            else
                return false;

            if ( inId == 0 && inName.size() != 0 )
            {
                if ( ( in = dest->getStaticVideoInput( inName ) ) == NULL )
                    return false;
            }
            else if ( inId != 0 && inName.size() == 0 )
            {
                if ( ( in = dest->getStaticVideoInput( inId ) ) == NULL )
                    return false;
            }
            else
                return false;
        }

    }
    else
    {
        if ( outId == 0 && outName.isEmpty() == false )
        {
            if ( ( out = m_staticVideosOutputs.getObject( outName ) ) == NULL )
                return false;
        }
        else if ( outId != 0 && outName.isEmpty() == true )
        {
            if ( ( out = m_staticVideosOutputs.getObject( outId ) ) == NULL )
                return false;
        }
        else
            return false;

        if ( m_father == NULL )
            return false;

        if ( inInternal == true )
        {

            /**
             * Connection external to internal ( so child to parent )
             */

            dest = m_father;

            if ( nodeId != 0 || nodeName.size() != 0 )
                return false;

            if ( inId == 0 && inName.size() != 0 )
            {
                if ( ( in = m_father->getInternalStaticVideoInput( inName ) ) == NULL )
                    return false;
            }
            else if ( inId != 0 && inName.size() == 0 )
            {
                if ( ( in = m_father->getInternalStaticVideoInput( inId ) ) == NULL )
                    return false;
            }
            else
                return false;
        }
        else
        {

            /**
             * Connection external to external ( so brother to brother )
             */

            if ( nodeId == 0 && nodeName.size() != 0 )
            {
                if ( ( dest = m_father->getChild( nodeName ) ) == NULL )
                    return false;
            }
            else if ( nodeId != 0 && nodeName.size() == 0 )
            {
                if ( ( dest = m_father->getChild( nodeId ) ) == NULL )
                    return false;
            }
            else
                return false;

            if ( inId == 0 && inName.size() != 0 )
            {
                if ( dest != this )
                {
                    if ( ( in = dest->getStaticVideoInput( inName ) ) == NULL )
                        return false;
                }
                else
                {
                    if ( ( in = m_staticVideosInputs.getObject( inName ) ) == NULL )
                        return false;
                }
            }
            else if ( inId != 0 && inName.size() == 0 )
            {
                if ( dest != this )
                {
                    if ( ( in = dest->getStaticVideoInput( inId ) ) == NULL )
                        return false;
                }
                else
                {
                    if ( ( in = m_staticVideosInputs.getObject( inId ) ) == NULL )
                        return false;
                }
            }
            else
                return false;

        }

    }

    /**
     * Connect the output on the input
     */

    if ( out->connect( *in ) == false )
        return false;

    /**
     * Reference the output and the input as connected
     */

    if ( outInternal == true )
    {
         if ( m_connectedInternalsStaticVideosOutputs.addObjectReference( out ) == false )
             return false;
    }
    else
    {
         if ( m_connectedStaticVideosOutputs.addObjectReference( out ) == false )
             return false;
    }

    if ( inInternal == true )
    {
        if ( dest != this )
        {
            if ( dest->referenceInternalStaticVideoInputAsConnected( in ) == false )
                return false;
        }
        else
        {
            if ( m_connectedInternalsStaticVideosInputs.addObjectReference( in ) == false )
                return false;
        }
    }
    else
    {
        if ( dest != this )
        {
            if ( dest->referenceStaticVideoInputAsConnected( in ) == false )
                return false;
        }
        else
        {
            if ( m_connectedStaticVideosInputs.addObjectReference( in ) == false )
                return false;
        }
    }
    return true;
}

/**
 * Brother to brother connection
 */

bool
EffectNode::connectBrotherToBrother( const QString &outName, const QString &nodeName, const QString &inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                0,
                                inName,
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectBrotherToBrother( const QString &outName, const QString &nodeName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                inId,
                                "",
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectBrotherToBrother( const QString &outName, quint32 nodeId, const QString &inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                0,
                                inName,
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectBrotherToBrother( const QString &outName, quint32 nodeId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                inId,
                                "",
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectBrotherToBrother( quint32 outId, const QString &nodeName, const QString &inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                0,
                                inName,
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectBrotherToBrother( quint32 outId, const QString &nodeName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                inId,
                                "",
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectBrotherToBrother( quint32 outId, quint32 nodeId, const QString &inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                0,
                                inName,
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectBrotherToBrother( quint32 outId, quint32 nodeId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                inId,
                                "",
                                false,
                                nodeId,
                                "" );
}

/**
 * Parent to child connection
 */

bool
EffectNode::connectParentToChild( const QString & outName, const QString & nodeName, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                0,
                                inName,
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectParentToChild( const QString & outName, const QString & nodeName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                inId,
                                "",
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectParentToChild( quint32 outId, const QString & nodeName, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                0,
                                inName,
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectParentToChild( quint32 outId, const QString & nodeName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                inId,
                                "",
                                false,
                                0,
                                nodeName );
}

bool
EffectNode::connectParentToChild( const QString & outName, quint32 nodeId, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                0,
                                inName,
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectParentToChild( const QString & outName, quint32 nodeId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                inId,
                                "",
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectParentToChild( quint32 outId, quint32 nodeId, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                0,
                                inName,
                                false,
                                nodeId,
                                "" );
}

bool
EffectNode::connectParentToChild( quint32 outId, quint32 nodeId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                inId,
                                "",
                                false,
                                nodeId,
                                "" );
}

/**
 * Child to parent connection
 */

bool
EffectNode::connectChildToParent( const QString & outName, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                0,
                                inName,
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectChildToParent( const QString & outName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                false,
                                inId,
                                "",
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectChildToParent( quint32 outId, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                0,
                                inName,
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectChildToParent( quint32 outId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                false,
                                inId,
                                "",
                                true,
                                0,
                                "" );
}

/**
 * Internal bridge connection
 */

bool
EffectNode::connectInternalBridge( const QString & outName, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                0,
                                inName,
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectInternalBridge( const QString & outName, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( 0,
                                outName,
                                true,
                                inId,
                                "",
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectInternalBridge( quint32 outId, const QString & inName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                0,
                                inName,
                                true,
                                0,
                                "" );
}

bool
EffectNode::connectInternalBridge( quint32 outId, quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveConnection( outId,
                                "",
                                true,
                                inId,
                                "",
                                true,
                                0,
                                "" );
}

//
// PRIMITIVE DISCONNECTION
//

bool
EffectNode::primitiveDisconnection( quint32 nodeId,
                                    const QString & nodeName,
                                    bool internal )
{
    OutSlot<LightVideoFrame>*           out;
    InSlot<LightVideoFrame>*            in;
    EffectNode*                         father;


    if ( nodeId == 0 && nodeName.size() != 0 )
    {
        if ( ( out = m_staticVideosOutputs.getObject( nodeName ) ) == NULL )
            return false;
    }
    else if ( nodeId != 0 && nodeName.size() == 0 )
    {
        if ( ( out = m_staticVideosOutputs.getObject( nodeId ) ) == NULL )
            return false;
    }
    else
        return false;

    if ( internal == true )
    {
        if ( m_connectedStaticVideosOutputs.delObjectReference( out->getId() ) == false )
            return false;
    }
    else
    {
        if ( m_connectedInternalsStaticVideosOutputs.delObjectReference( out->getId() ) == false )
            return false;
    }

    in = out->getInSlotPtr();

    father = in->getPrivateFather();

    if ( internal == true )
    {
        if ( father == this )
        {
            if ( m_connectedInternalsStaticVideosInputs.delObjectReference( in->getId() ) == false )
                return false;
        }
        else
        {
            if ( father->dereferenceStaticVideoInputAsConnected( in->getId() ) == false )
                return false;
        }
    }
    else
    {
        if ( father == m_father )
        {
            if ( father->dereferenceInternalStaticVideoInputAsConnected( in->getId() ) == false )
                return false;
        }
        else if ( father == this )
        {
            if ( m_connectedStaticVideosInputs.delObjectReference( in->getId() ) == false )
                return false;
        }
        else
        {
            if ( father->dereferenceStaticVideoInputAsConnected( in->getId() ) == false )
                return false;
        }
    }

    if ( out->disconnect() == false )
        return false;

    return true;

}

//
// SLOTS DISCONNECTION
//

bool
EffectNode::disconnectStaticVideoOutput( quint32 nodeId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveDisconnection( nodeId, "", false );
}

bool
EffectNode::disconnectStaticVideoOutput( const QString & nodeName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveDisconnection( 0, nodeName, false );
}

//
// INTERNALS SLOTS DISCONNECTION
//

bool
EffectNode::disconnectInternalStaticVideoOutput( quint32 nodeId )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveDisconnection( nodeId, "", true );
}

bool
EffectNode::disconnectInternalStaticVideoOutput( const QString & nodeName )
{
    QWriteLocker                        wl( &m_rwl );

    return primitiveDisconnection( 0, nodeName, true );
}


//
// CONNECTED SLOTS REFERENCEMENT
//


bool
EffectNode::referenceStaticVideoInputAsConnected( InSlot<LightVideoFrame>* in )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedStaticVideosInputs.addObjectReference( in );
}

bool
EffectNode::referenceInternalStaticVideoOutputAsConnected( OutSlot<LightVideoFrame>* out )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedInternalsStaticVideosOutputs.addObjectReference( out );
}

bool
EffectNode::referenceStaticVideoOutputAsConnected( OutSlot<LightVideoFrame>* out )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedStaticVideosOutputs.addObjectReference( out );
}

bool
EffectNode::referenceInternalStaticVideoInputAsConnected( InSlot<LightVideoFrame>* in )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedInternalsStaticVideosInputs.addObjectReference( in );
}

bool
EffectNode::dereferenceStaticVideoInputAsConnected( quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedStaticVideosInputs.delObjectReference( inId );
}

bool
EffectNode::dereferenceInternalStaticVideoOutputAsConnected( quint32 outId )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedInternalsStaticVideosOutputs.delObjectReference(  outId );
}

bool
EffectNode::dereferenceStaticVideoOutputAsConnected( quint32 outId )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedStaticVideosOutputs.delObjectReference(  outId );
}

bool
EffectNode::dereferenceInternalStaticVideoInputAsConnected( quint32 inId )
{
    QWriteLocker                        wl( &m_rwl );

    return m_connectedInternalsStaticVideosInputs.delObjectReference( inId );
}

QList<InSlot<LightVideoFrame>*>
EffectNode::getConnectedStaticsVideosInputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedStaticVideosInputs.getObjectsReferencesList();
}

QList<OutSlot<LightVideoFrame>*>
EffectNode::getConnectedInternalsStaticsVideosOutputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedInternalsStaticVideosOutputs.getObjectsReferencesList();
}

QList<OutSlot<LightVideoFrame>*>
EffectNode::getConnectedStaticsVideosOutputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedStaticVideosOutputs.getObjectsReferencesList();
}

QList<InSlot<LightVideoFrame>*>
EffectNode::getConnectedInternalsStaticsVideosInputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedInternalsStaticVideosInputs.getObjectsReferencesList();
}

quint32
EffectNode::getNBConnectedStaticsVideosInputs( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedStaticVideosInputs.getNBObjectsReferences();
}

quint32
EffectNode::getNBConnectedInternalsStaticsVideosOutputs( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedInternalsStaticVideosOutputs.getNBObjectsReferences();
}

quint32
EffectNode::getNBConnectedStaticsVideosOutputs( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedStaticVideosOutputs.getNBObjectsReferences();
}

quint32
EffectNode::getNBConnectedInternalsStaticsVideosInputs( void ) const
{
    QReadLocker                        rl( &m_rwl );

    return m_connectedInternalsStaticVideosInputs.getNBObjectsReferences();
}


//==============================================================================//
//                            GET INTERNAL PLUGIN                               //
//==============================================================================//


IEffectPlugin*
EffectNode::getInternalPlugin( void )
{
    QReadLocker                        rl( &m_rwl );
    return m_plugin;
}


//==============================================================================//
//                 EFFECT INSTANCE AND EFFECT TYPE INFORMATIONS                 //
//==============================================================================//


void
EffectNode::setFather( EffectNode* father )
{
    QWriteLocker                        wl( &m_rwl );
    m_father = father;

}

IEffectNode*
EffectNode::getFather( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_father;
}

EffectNode*
EffectNode::getPrivateFather( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_father;
}



void
EffectNode::setTypeId( quint32 typeId )
{
    QWriteLocker                        wl( &m_rwl );
    m_typeId = typeId;

}

void
EffectNode::setTypeName( const QString & typeName )
{
    QWriteLocker                        wl( &m_rwl );
    m_typeName = typeName;

}

void
EffectNode::setInstanceId( quint32 instanceId )
{
    QWriteLocker                        wl( &m_rwl );
    m_instanceId = instanceId;

}

void
EffectNode::setInstanceName( const QString & instanceName )
{
    QWriteLocker                        wl( &m_rwl );
    m_instanceName = instanceName;

}

quint32
EffectNode::getTypeId( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_typeId;
}

const QString &
EffectNode::getTypeName( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_typeName;
}

quint32
EffectNode::getInstanceId( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_instanceId;
}

const QString &
EffectNode::getInstanceName( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_instanceName;
}

bool
EffectNode::isAnEmptyNode( void ) const
{
    QReadLocker                        rl( &m_rwl );
    if ( m_plugin )
        return false;
    return true;
}

//==============================================================================//
//                               NODES MANAGEMENT                               //
//==============================================================================//

//
// ROOT NODES
//

bool
EffectNode::createRootNode( const QString & rootNodeName )
{
    QWriteLocker                        wl( &s_srwl );
    return EffectNode::s_renf.createEmptyEffectNodeInstance( rootNodeName );
}

bool
EffectNode::deleteRootNode( const QString & rootNodeName )
{
    QWriteLocker                        wl( &s_srwl );
    return EffectNode::s_renf.deleteEffectNodeInstance( rootNodeName );
}

EffectNode*
EffectNode::getRootNode( const QString & rootNodeName )
{
    QReadLocker                        rl( &s_srwl );
    return EffectNode::s_renf.getEffectNodeInstance( rootNodeName );
}

//
// CHILDS TYPES INFORMATIONS
//

QList<QString>
EffectNode::getChildsTypesNamesList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeTypesNamesList();
}

QList<quint32>
EffectNode::getChildsTypesIdsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeTypesIdsList();
}

const QString
EffectNode::getChildTypeNameByTypeId( quint32 typeId ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeTypeNameByTypeId( typeId );
}

quint32
EffectNode::getChildTypeIdByTypeName( const QString & typeName ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeTypeIdByTypeName( typeName );
}

//
// CHILDS INFORMATIONS
//

QList<QString>
EffectNode::getChildsNamesList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstancesNamesList();
}

QList<quint32>
EffectNode::getChildsIdsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstancesIdsList();
}

const QString
EffectNode::getChildNameByChildId( quint32 childId ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstanceNameByInstanceId( childId );
}

quint32
EffectNode::getChildIdByChildName( const QString & childName ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstanceIdByInstanceName( childName );
}

//
// CREATE AND DELETE CHILDS
//

bool
EffectNode::createEmptyChild( void )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
    {
        m_enf.createEmptyEffectNodeInstance();
        return true;
    }
    return false;
}

bool
EffectNode::createEmptyChild( const QString & childName )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
        return m_enf.createEmptyEffectNodeInstance( childName );
    return false;
}

bool
EffectNode::createChild( quint32 typeId )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
        return m_enf.createEffectNodeInstance( typeId );
    return false;
}

bool
EffectNode::createChild( const QString & typeName )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
        return m_enf.createEffectNodeInstance( typeName );
    return false;
}

bool
EffectNode::deleteChild( quint32 childId )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
        return m_enf.deleteEffectNodeInstance( childId );
    return false;
}

bool
EffectNode::deleteChild( const QString & childName )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_plugin == NULL )
        return m_enf.deleteEffectNodeInstance( childName );
    return false;
}

//
// GETTING CHILDS
//

EffectNode*
EffectNode::getChild( quint32 childId ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstance( childId );
}

EffectNode*
EffectNode::getChild( const QString & childName ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstance( childName );
}

QList<EffectNode*>
EffectNode::getChildsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_enf.getEffectNodeInstancesList();
}


//==============================================================================//
//                               SLOTS MANAGEMENT                               //
//==============================================================================//

//
// SLOTS CREATION/DELETION
//

void
EffectNode::createStaticVideoInput( const QString & name )
{
    QWriteLocker                        wl( &m_rwl );
    m_staticVideosInputs.createObject( name );
    if ( m_plugin == NULL )
        m_internalsStaticVideosOutputs.createObject( name );

}

void
EffectNode::createStaticVideoOutput( const QString & name )
{
    QWriteLocker                        wl( &m_rwl );
    m_staticVideosOutputs.createObject( name );
    if ( m_plugin == NULL )
        m_internalsStaticVideosInputs.createObject( name );

}

void
EffectNode::createStaticVideoInput( void )
{
    QWriteLocker                        wl( &m_rwl );
    m_staticVideosInputs.createObject();
    if ( m_plugin == NULL )
        m_internalsStaticVideosOutputs.createObject();

}

void
EffectNode::createStaticVideoOutput( void )
{
    QWriteLocker                        wl( &m_rwl );
    m_staticVideosOutputs.createObject();
    if ( m_plugin == NULL )
        m_internalsStaticVideosInputs.createObject();

}

bool
EffectNode::removeStaticVideoInput( const QString & name )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_staticVideosInputs.deleteObject( name ) )
    {
        if ( m_plugin == NULL )
            if ( m_internalsStaticVideosOutputs.deleteObject( name ) == false )
                return false; // IF THIS CAS HAPPEND WE ARE SCREWED
        return true;
    }
    return false;
}

bool
EffectNode::removeStaticVideoOutput( const QString & name )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_staticVideosOutputs.deleteObject( name ) )
    {
        if ( m_plugin == NULL )
            if ( m_internalsStaticVideosInputs.deleteObject( name ) == false )
                return false; // IF THIS CAS HAPPEND WE ARE SCREWED
        return true;
    }
    return false;
}

bool
EffectNode::removeStaticVideoInput( quint32 id )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_staticVideosInputs.deleteObject( id ) )
    {
        if ( m_plugin == NULL )
            if ( m_internalsStaticVideosOutputs.deleteObject( id ) == false )
                return false; // IF THIS CAS HAPPEND WE ARE SCREWED
        return true;
    }
    return false;
}

bool
EffectNode::removeStaticVideoOutput( quint32 id )
{
    QWriteLocker                        wl( &m_rwl );
    if ( m_staticVideosOutputs.deleteObject( id ) )
    {
        if ( m_plugin == NULL )
            if ( m_internalsStaticVideosInputs.deleteObject( id ) == false )
                return false; // IF THIS CAS HAPPEND WE ARE SCREWED
        return true;
    }
    return false;
}

//
// GETTING SLOTS
//

InSlot<LightVideoFrame>*
EffectNode::getStaticVideoInput( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObject( name );
}

OutSlot<LightVideoFrame>*
EffectNode::getStaticVideoOutput( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObject( name );
}

InSlot<LightVideoFrame>*
EffectNode::getStaticVideoInput( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObject( id );
}

OutSlot<LightVideoFrame>*
EffectNode::getStaticVideoOutput( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObject( id );
}

QList<InSlot<LightVideoFrame>*>
EffectNode::getStaticsVideosInputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObjectsList();
}

QList<OutSlot<LightVideoFrame>*>
EffectNode::getStaticsVideosOutputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObjectsList();
}

//
// GETTING SLOTS INFOS ( SO EFFECT CONFIGURATION )
//

QList<QString>
EffectNode::getStaticsVideosInputsNamesList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObjectsNamesList();
}

QList<QString>
EffectNode::getStaticsVideosOutputsNamesList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObjectsNamesList();
}

QList<quint32>
EffectNode::getStaticsVideosInputsIdsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObjectsIdsList();
}

QList<quint32>
EffectNode::getStaticsVideosOutputsIdsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObjectsIdsList();
}

const QString
EffectNode::getStaticVideoInputNameById( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObjectNameByObjectId( id );
}

const QString
EffectNode::getStaticVideoOutputNameById( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObjectNameByObjectId( id );
}

quint32
EffectNode::getStaticVideoInputIdByName( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getObjectIdByObjectName( name );
}

quint32
EffectNode::getStaticVideoOutputIdByName( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getObjectIdByObjectName( name );
}

quint32
EffectNode::getNBStaticsVideosInputs( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosInputs.getNBObjects();
}

quint32
EffectNode::getNBStaticsVideosOutputs( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_staticVideosOutputs.getNBObjects();
}

//
// GET INTERNALS SLOTS ( JUST FOR EMPTY NODES)
//

InSlot<LightVideoFrame>*
EffectNode::getInternalStaticVideoInput( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosInputs.getObject( name );
}

OutSlot<LightVideoFrame>*
EffectNode::getInternalStaticVideoOutput( const QString & name ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosOutputs.getObject( name );
}


InSlot<LightVideoFrame>*
EffectNode::getInternalStaticVideoInput( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosInputs.getObject( id );
}

OutSlot<LightVideoFrame>*
EffectNode::getInternalStaticVideoOutput( quint32 id ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosOutputs.getObject( id );
}


QList<InSlot<LightVideoFrame>*>
EffectNode::getInternalsStaticsVideosInputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosInputs.getObjectsList();
}

QList<OutSlot<LightVideoFrame>*>
EffectNode::getInternalsStaticsVideosOutputsList( void ) const
{
    QReadLocker                        rl( &m_rwl );
    return m_internalsStaticVideosOutputs.getObjectsList();
}
