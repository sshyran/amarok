/****************************************************************************************
 * Copyright (c) 2008 Daniel Jones <danielcjones@gmail.com>                             *
 * Copyright (c) 2009 Leo Franchi <lfranchi@kde.org>                                    *
 * Copyright (c) 2010,2011 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#define DEBUG_PREFIX "Bias"

#include "Bias.h"
#include "BiasFactory.h"
#include "DynamicModel.h"
#include "biases/SearchQueryBias.h"

#include "core/support/Debug.h"
#include "DynamicBiasWidgets.h"

#include <QPainter>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <klocale.h>

// -------- AbstractBias -------------

Dynamic::AbstractBias::AbstractBias()
{ }

Dynamic::AbstractBias::~AbstractBias()
{
    // debug() << "destroying bias" << this;
}

void
Dynamic::AbstractBias::fromXml( QXmlStreamReader *reader )
{
    reader->skipCurrentElement();
}


void
Dynamic::AbstractBias::toXml( QXmlStreamWriter *writer ) const
{
    Q_UNUSED( writer );
}

QString
Dynamic::AbstractBias::sName()
{
    return QLatin1String( "abstractBias" );
}

QString
Dynamic::AbstractBias::name() const
{
    return Dynamic::AbstractBias::sName();
}

QWidget*
Dynamic::AbstractBias::widget( QWidget* parent )
{
    Q_UNUSED( parent );
    return 0;
}

void
Dynamic::AbstractBias::paintOperator( QPainter* painter, const QRect& rect, Dynamic::AbstractBias* bias )
{
    Q_UNUSED( painter );
    Q_UNUSED( rect );
    Q_UNUSED( bias );
}

void
Dynamic::AbstractBias::invalidate()
{ }

void
Dynamic::AbstractBias::replace( Dynamic::BiasPtr newBias )
{
    emit replaced( BiasPtr(const_cast<Dynamic::AbstractBias*>(this)), newBias );
}

double
Dynamic::AbstractBias::energy( const Meta::TrackList& playlist, int contextCount ) const
{
    Q_UNUSED( contextCount );

    if( playlist.count() <= contextCount )
        return 0.0;

    int matchCount = 0;
    for( int i = contextCount; i < playlist.count(); i++ )
    {
        if( trackMatches( i, playlist, contextCount ) )
            matchCount++;
    }

    return 1.0 - (double(matchCount) / (playlist.count() - contextCount));
}


// -------- RandomBias ------

Dynamic::RandomBias::RandomBias()
{ }

Dynamic::RandomBias::~RandomBias()
{ }

QString
Dynamic::RandomBias::sName()
{
    return QLatin1String( "randomBias" );
}

QString
Dynamic::RandomBias::name() const
{
    return Dynamic::RandomBias::sName();
}

QString
Dynamic::RandomBias::toString() const
{
    return i18nc("Random bias representation", "Random songs");
}

QWidget*
Dynamic::RandomBias::widget( QWidget* parent )
{
    Q_UNUSED( parent );
    return 0;
}

Dynamic::TrackSet
Dynamic::RandomBias::matchingTracks( int position,
                                  const Meta::TrackList& playlist, int contextCount,
                                  Dynamic::TrackCollectionPtr universe ) const
{
    Q_UNUSED( position );
    Q_UNUSED( playlist );
    Q_UNUSED( contextCount );
    return Dynamic::TrackSet( universe, true );
}

bool
Dynamic::RandomBias::trackMatches( int position,
                                   const Meta::TrackList& playlist,
                                   int contextCount ) const
{
    Q_UNUSED( position );
    Q_UNUSED( playlist );
    Q_UNUSED( contextCount );
    return true;
}

double
Dynamic::RandomBias::energy( const Meta::TrackList& playlist, int contextCount ) const
{
    Q_UNUSED( playlist );
    Q_UNUSED( contextCount );
    return 0.0;
}

// -------- UniqueBias ------

Dynamic::UniqueBias::UniqueBias()
{ }

Dynamic::UniqueBias::~UniqueBias()
{ }

QString
Dynamic::UniqueBias::sName()
{
    return QLatin1String( "uniqueBias" );
}

QString
Dynamic::UniqueBias::name() const
{
    return Dynamic::UniqueBias::sName();
}

QString
Dynamic::UniqueBias::toString() const
{
    return i18nc("Unique bias representation",
                 "Only once in the current playlist");
}

QWidget*
Dynamic::UniqueBias::widget( QWidget* parent )
{
    Q_UNUSED( parent );
    return 0;
}

Dynamic::TrackSet
Dynamic::UniqueBias::matchingTracks( int position,
                                  const Meta::TrackList& playlist, int contextCount,
                                  Dynamic::TrackCollectionPtr universe ) const
{
    Q_UNUSED( contextCount );

    Dynamic::TrackSet result = Dynamic::TrackSet( universe, true );
    for( int i = 0; i < position; i++ )
        result.subtract( playlist.at(i) );

    return result;
}

bool
Dynamic::UniqueBias::trackMatches( int position,
                                   const Meta::TrackList& playlist,
                                   int contextCount ) const
{
    Q_UNUSED( contextCount );

    for( int i = 0; i < position; i++ )
        if( playlist.at(position) == playlist.at(i) )
            return false;
    return true;
}


// -------- AndBias ------

Dynamic::AndBias::AndBias()
{ }

Dynamic::AndBias::~AndBias()
{ }

void
Dynamic::AndBias::fromXml( QXmlStreamReader *reader )
{
    while (!reader->atEnd()) {
        reader->readNext();

        if( reader->isStartElement() )
        {
            Dynamic::BiasPtr bias( Dynamic::BiasFactory::fromXml( reader ) );
            if( bias )
            {
                appendBias( bias );
            }
            else
            {
                warning()<<"Unexpected xml start element"<<reader->name()<<"in input";
                reader->skipCurrentElement();
            }
        }
        else if( reader->isEndElement() )
        {
            break;
        }
    }
}

void
Dynamic::AndBias::toXml( QXmlStreamWriter *writer ) const
{
    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        writer->writeStartElement( bias->name() );
        bias->toXml( writer );
        writer->writeEndElement();
    }
}

QString
Dynamic::AndBias::sName()
{
    return QLatin1String( "andBias" );
}

QString
Dynamic::AndBias::name() const
{
    return Dynamic::AndBias::sName();
}

QString
Dynamic::AndBias::toString() const
{
    return i18nc("And bias representation", "Match all");
}


QWidget*
Dynamic::AndBias::widget( QWidget* parent )
{
    Q_UNUSED( parent );
    return 0;
}

void
Dynamic::AndBias::paintOperator( QPainter* painter, const QRect& rect, Dynamic::AbstractBias* bias )
{
    if( m_biases.indexOf( Dynamic::BiasPtr(bias) ) > 0 )
    {
        painter->drawText( rect.adjusted(2, 0, -2, 0),
                           Qt::AlignRight,
                           i18nc("Prefix for AndBias. Shown in front of a bias in the dynamic playlist view", "and" ) );
    }
}

Dynamic::TrackSet
Dynamic::AndBias::matchingTracks( int position,
                                  const Meta::TrackList& playlist, int contextCount,
                                  Dynamic::TrackCollectionPtr universe ) const
{
    DEBUG_BLOCK;
debug() << "universe:" << universe.data();

    m_tracks = Dynamic::TrackSet( universe, true );
    m_outstandingMatches = 0;

    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        Dynamic::TrackSet tracks = bias->matchingTracks( position, playlist, contextCount, universe );
        if( tracks.isOutstanding() )
            m_outstandingMatches++;
        else
            m_tracks.intersect( tracks );

        //    debug() << "AndBias::matchingTracks" << bias->name() << "tracks:"<<tracks.trackCount() << "outstanding?" << tracks.isOutstanding() << "numOUt:" << m_outstandingMatches;

        if( m_tracks.isEmpty() )
            break;
    }

    // debug() << "AndBias::matchingTracks end: tracks:"<<m_tracks.trackCount() << "outstanding?" << m_tracks.isOutstanding() << "numOUt:" << m_outstandingMatches;

    if( m_outstandingMatches > 0 )
        return Dynamic::TrackSet();
    else
        return m_tracks;
}

bool
Dynamic::AndBias::trackMatches( int position,
                                const Meta::TrackList& playlist,
                                int contextCount ) const
{
    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        if( !bias->trackMatches( position, playlist, contextCount ) )
            return false;
    }
    return true;
}

void
Dynamic::AndBias::invalidate()
{
    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        bias->invalidate();
    }
    m_tracks = TrackSet();
}

void
Dynamic::AndBias::appendBias( Dynamic::BiasPtr bias )
{
    BiasPtr thisPtr( this );
    bool inModel = DynamicModel::instance()->index( thisPtr ).isValid();
    if( inModel )
        DynamicModel::instance()->beginInsertBias( thisPtr, m_biases.count() );
    m_biases.append( bias );
    if( inModel )
        DynamicModel::instance()->endInsertBias();

    connect( bias.data(), SIGNAL( resultReady( const Dynamic::TrackSet & ) ),
             this,  SLOT( resultReceived( const Dynamic::TrackSet & ) ) );
    connect( bias.data(), SIGNAL( replaced( Dynamic::BiasPtr, Dynamic::BiasPtr ) ),
             this, SLOT( biasReplaced( Dynamic::BiasPtr, Dynamic::BiasPtr ) ) );
    connect( bias.data(), SIGNAL( changed( Dynamic::BiasPtr ) ),
             this, SLOT( biasChanged( Dynamic::BiasPtr ) ) );
    emit biasAppended( bias );

    // creating a shared pointer and destructing it just afterwards would
    // also destruct this object.
    // so we give the object creating this bias a chance to increment the refcount
    emit changed( thisPtr );
}

void
Dynamic::AndBias::moveBias( int from, int to )
{
    // TODO: emit model changes
    m_biases.insert( to, m_biases.takeAt( from ) );
    emit biasMoved( from, to );
    emit changed( BiasPtr( this ) );
}


void
Dynamic::AndBias::resultReceived( const Dynamic::TrackSet &tracks )
{
    m_tracks.intersect( tracks );
    --m_outstandingMatches;
    // debug() << "AndBias::resultReceived" << m_outstandingMatches << "tr" << m_tracks.trackCount();

    if( m_outstandingMatches < 0 )
        warning() << "Received more results than expected.";
    else if( m_outstandingMatches == 0 )
        emit resultReady( m_tracks );
}

void
Dynamic::AndBias::biasReplaced( Dynamic::BiasPtr oldBias, Dynamic::BiasPtr newBias )
{
    DEBUG_BLOCK;
    BiasPtr thisPtr( this );
    int index = m_biases.indexOf( oldBias );
    Q_ASSERT( index >= 0 );

    disconnect( oldBias.data(), 0, this, 0 );
    bool inModel = DynamicModel::instance()->index( thisPtr ).isValid();
    if( inModel )
        DynamicModel::instance()->beginRemoveBias( thisPtr, index );
    m_biases.removeAt( index );
    if( inModel )
        DynamicModel::instance()->endRemoveBias();
    emit biasRemoved( index );

    if( newBias )
    {
        connect( newBias.data(), SIGNAL( resultReady( const Dynamic::TrackSet & ) ),
                 this,  SLOT( resultReceived( const Dynamic::TrackSet & ) ) );
        connect( newBias.data(), SIGNAL( replaced( Dynamic::BiasPtr, Dynamic::BiasPtr ) ),
                 this, SLOT( biasReplaced( Dynamic::BiasPtr, Dynamic::BiasPtr ) ) );
        connect( newBias.data(), SIGNAL( changed( Dynamic::BiasPtr ) ),
                 this, SIGNAL( changed( Dynamic::BiasPtr ) ) );

        if( inModel )
            DynamicModel::instance()->beginInsertBias( thisPtr, index );
        m_biases.insert( index, newBias );
        if( inModel )
            DynamicModel::instance()->endInsertBias();

        // we don't have an bias inserted signal
        emit biasAppended( newBias );
        emit biasMoved( m_biases.count()-1, index );
    }

    emit changed( thisPtr );
}

void
Dynamic::AndBias::biasChanged( Dynamic::BiasPtr bias )
{
    BiasPtr thisPtr( this );
    emit changed( thisPtr );
    bool inModel = DynamicModel::instance()->index( thisPtr ).isValid();
    if( inModel )
        DynamicModel::instance()->biasChanged( bias );
}

// -------- OrBias ------

Dynamic::OrBias::OrBias()
    : AndBias()
{ }

QString
Dynamic::OrBias::sName()
{
    return QLatin1String( "orBias" );
}

QString
Dynamic::OrBias::name() const
{
    return Dynamic::OrBias::sName();
}

void
Dynamic::OrBias::paintOperator( QPainter* painter, const QRect& rect, Dynamic::AbstractBias* bias )
{
    if( m_biases.indexOf( Dynamic::BiasPtr(bias) ) > 0 )
    {
        painter->drawText( rect.adjusted(2, 0, -2, 0),
                           Qt::AlignRight,
                           i18nc("Prefix for OrBias. Shown in front of a bias in the dynamic playlist view", "or" ) );
    }
}


QString
Dynamic::OrBias::toString() const
{
    return i18nc("Or bias representation", "Match any");
}

Dynamic::TrackSet
Dynamic::OrBias::matchingTracks( int position,
                                 const Meta::TrackList& playlist, int contextCount,
                                 Dynamic::TrackCollectionPtr universe ) const
{
    m_tracks = Dynamic::TrackSet( universe, false );
    m_outstandingMatches = 0;

    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        Dynamic::TrackSet tracks = bias->matchingTracks( position, playlist, contextCount, universe );
        if( tracks.isOutstanding() )
            m_outstandingMatches++;
        else
            m_tracks.unite( tracks );

        if( m_tracks.isFull() )
            break;
    }

    if( m_outstandingMatches > 0 )
        return Dynamic::TrackSet();
    else
        return m_tracks;
}

bool
Dynamic::OrBias::trackMatches( int position,
                               const Meta::TrackList& playlist,
                               int contextCount ) const
{
    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        if( bias->trackMatches( position, playlist, contextCount ) )
            return true;
    }
    return false;
}

void
Dynamic::OrBias::resultReceived( const Dynamic::TrackSet &tracks )
{
    m_tracks.unite( tracks );
    --m_outstandingMatches;

    if( m_outstandingMatches < 0 )
        warning() << "Received more results than expected.";
    else if( m_outstandingMatches == 0 )
        emit resultReady( m_tracks );
}

// -------- NotBias ------

Dynamic::NotBias::NotBias()
    : OrBias()
{ }

QString
Dynamic::NotBias::sName()
{
    return QLatin1String( "notBias" );
}

QString
Dynamic::NotBias::name() const
{
    return Dynamic::NotBias::sName();
}

QString
Dynamic::NotBias::toString() const
{
    return i18nc("Not bias representation", "Not");
}

Dynamic::TrackSet
Dynamic::NotBias::matchingTracks( int position,
                                 const Meta::TrackList& playlist, int contextCount,
                                 Dynamic::TrackCollectionPtr universe ) const
{
    m_tracks = Dynamic::TrackSet( universe, true );
    m_outstandingMatches = 0;

    foreach( Dynamic::BiasPtr bias, m_biases )
    {
        Dynamic::TrackSet tracks = bias->matchingTracks( position, playlist, contextCount, universe );
        if( tracks.isOutstanding() )
            m_outstandingMatches++;
        else
            m_tracks.subtract( tracks );

        if( m_tracks.isEmpty() )
            break;
    }

    if( m_outstandingMatches > 0 )
        return Dynamic::TrackSet();
    else
        return m_tracks;
}

bool
Dynamic::NotBias::trackMatches( int position,
                               const Meta::TrackList& playlist,
                               int contextCount ) const
{
    return ! Dynamic::OrBias::trackMatches( position, playlist, contextCount );
}

double
Dynamic::NotBias::energy( const Meta::TrackList& playlist, int contextCount ) const
{
    return 1.0 - Dynamic::OrBias::energy( playlist, contextCount );
}

void
Dynamic::NotBias::resultReceived( const Dynamic::TrackSet &tracks )
{
    m_tracks.subtract( tracks );
    --m_outstandingMatches;

    if( m_outstandingMatches < 0 )
        warning() << "Received more results than expected.";
    else if( m_outstandingMatches == 0 )
        emit resultReady( m_tracks );
}

#include "Bias.moc"

