/*****************************************************************************
 * WorkflowFileRenderer.h: Output the workflow to a file
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

#ifndef WORKFLOWFILERENDERER_H
#define WORKFLOWFILERENDERER_H

#include "config.h"
#include "Backend/ISourceRenderer.h"
#include "Workflow/MainWorkflow.h"
#include "WorkflowRenderer.h"

#include <QTime>

class   WorkflowFileRenderer : public WorkflowRenderer
{
    Q_OBJECT

public:
    WorkflowFileRenderer( Backend::IBackend* backend , MainWorkflow* workflow );
    virtual ~WorkflowFileRenderer();

    void                        run(const QString& outputFileName, quint32 width,
                                    quint32 height, double fps, quint32 vbitrate,
                                    quint32 abitrate);
    static int                  lock(void* datas, const char* cookie, int64_t *dts, int64_t *pts,
                                      unsigned int *flags, size_t *bufferSize, const void **buffer );
    virtual float               getFps() const;

private:
    quint8                      *m_renderVideoFrame;
#ifdef WITH_GUI
    QTime                       m_time;
#endif

protected:
    virtual Backend::ISourceRenderer::MemoryInputLockCallback getLockCallback();
    virtual Backend::ISourceRenderer::MemoryInputUnlockCallback getUnlockCallback();

private slots:
    void                        __frameChanged( qint64 frame,
                                                Vlmc::FrameChangedReason reason );

signals:
    void                        imageUpdated( const uchar* image );
    void                        frameChanged( qint64 );
    void                        renderComplete();
};

#endif // WORKFLOWFILERENDERER_H
