/***************************************************************************
                      gstengine.h  -  GStreamer audio interface
                         -------------------
begin                : Jan 02 2003
copyright            : (C) 2003 by Mark Kretschmann
email                : markey@web.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef AMAROK_GSTENGINE_H
#define AMAROK_GSTENGINE_H

#include "enginebase.h"

#include <vector>

#include <qobject.h>
#include <qstringlist.h>

#include <gst/gst.h>

using std::vector;

class KURL;

class GstEngine : public EngineBase
{
        Q_OBJECT

    public:
                                                 GstEngine();
                                                 ~GstEngine();

        void                                     init( bool& restart, int scopeSize, bool restoreEffects ); 
                                                                                                  
        bool                                     initMixer( bool hardware );
        bool                                     canDecode( const KURL &url, mode_t mode, mode_t permissions );
        StreamingMode                            streamingMode() { return Signal; }
        QStringList                              getOutputsList() { return getPluginList( "Sink/Audio" ); }

        long                                     length() const { return 0; }
        long                                     position() const;
        EngineState                              state() const;
        bool                                     isStream() const;
        std::vector<float>*                      scope();

    public slots:
        const QObject*                           play( const KURL& );
        void                                     play();
        void                                     stop();
        void                                     pause();
        void                                     seek( long ms );
        void                                     setVolume( int percent );
        void                                     newStreamData( char* data, int size );

    private slots:
        void                                     handleFakesrc();
        void                                     handleError();
        void                                     stopAtEnd();
        
    private:
        /** Called at end of track */
        static void                              eos_cb( GstElement*, GstElement* );
        /** Duplicates audio data for application side processing */
        static void                              handoff_cb( GstElement*, GstBuffer*, gpointer );
        /** Called when pipeline needs new data from source */
        static void                              handoff_fakesrc_cb( GstElement*, GstBuffer*, GstPad, gpointer );
        static void                              typefindFound_cb( GstElement*, GstCaps*, GstElement* );
        static void                              error_cb( GstElement*, GstElement*, GError*, gchar*, gpointer );

        /** Get a list of available plugins from a specified Class */
        QStringList                              getPluginList( const QCString& classname );
        
        void                                     cleanPipeline();
        void                                     interpolate( const vector<float>& inVec, vector<float>& outVec );
        /////////////////////////////////////////////////////////////////////////////////////
        // ATTRIBUTES
        /////////////////////////////////////////////////////////////////////////////////////
        static GstBuffer*                        fakesrc_buf;
        static GError*                           error_msg;
        static GstEngine*                        self;
        
        GstElement*                              m_pThread;
        GstElement*                              m_pAudiosink;
        GstElement*                              m_pSpider;
        GstElement*                              m_pFilesrc;
        GstElement*                              m_pIdentity;
        GstElement*                              m_pVolume;
        GstElement*                              m_pAudioconvert;
        GstElement*                              m_pAudioscale;

        vector<float>                            m_scopeBuf;
        uint                                     m_scopeBufIndex;
        uint                                     m_scopeSize;
       
        vector<char>                             m_streamBuf;
        uint                                     m_streamBufIn;
        uint                                     m_streamBufOut;
        bool                                     m_playFlag;
        
        bool                                     m_typefindResult;
        bool                                     m_pipelineFilled;
};


#endif /*AMAROK_GSTENGINE_H*/

