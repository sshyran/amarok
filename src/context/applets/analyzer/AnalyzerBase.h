/****************************************************************************************
 * Copyright (c) 2004 Max Howell <max.howell@methylblue.com>                            *
 * Copyright (c) 2009 Martin Sandsmark <martin.sandsmark@kde.org>                       *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef ANALYZERBASE_H
#define ANALYZERBASE_H

#ifdef __FreeBSD__
#include <sys/types.h>
#endif

#include "fht.h"     //stack allocated and convenience

#include <QGLWidget>
#include <QPixmap> //stack allocated and convenience
#include <QTimer>  //stack allocated
#include <QWidget> //baseclass

#include <vector>    //included for convenience

#include <phonon/audiodataoutput.h>


namespace Analyzer
{

template<class W> class Base : public W
{
public:
    virtual void processData( const QMap<Phonon::AudioDataOutput::Channel, QVector<qint16> > &thescope );

protected:
    Base( QWidget* );
    ~Base()
    {
        delete m_fht;
    }

    virtual void demo();

    virtual void transform( QVector<float>& );
    virtual void analyze( const QVector<float>& ) = 0;
    virtual void paused();

protected:
    FHT    *m_fht;
};


class Base2D : public Base<QWidget>
{
    Q_OBJECT

private slots:
    void demo() { Base<QWidget>::demo(); }

    void enableDemo( bool enable )
    {
        enable ? m_demoTimer.start() : m_demoTimer.stop();
    }
    void playbackStateChanged();

    void processData( const QMap<Phonon::AudioDataOutput::Channel, QVector<qint16> > &thescope )
    {
        Base<QWidget>::processData( thescope );
    }

    void connectSignals();

protected:
    Base2D( QWidget* );

private:
    QTimer m_demoTimer;
    QTimer m_renderTimer;
};


class Base3D : public Base<QGLWidget>
{
    Q_OBJECT

private slots:
    void demo() { Base<QGLWidget>::demo(); }

    void enableDemo( bool enable )
    {
        enable ? m_demoTimer.start() : m_demoTimer.stop();
    }
    void playbackStateChanged();

    void processData( const QMap<Phonon::AudioDataOutput::Channel, QVector<qint16> > &thescope )
    {
        Base<QGLWidget>::processData( thescope );
    }

    void connectSignals();

protected:
    Base3D( QWidget* );

private:
    QTimer m_demoTimer;
    QTimer m_renderTimer;
};


void interpolate( const QVector<float>&, QVector<float>& );

} //END namespace Analyzer


#endif
