/****************************************************************************************
 * Copyright (c) 2008 Daniel Winter <dw@danielwinter.de>                                *
 * Copyright (c) 2012 Phalgun Guduthur <me@phalgun.in>                                  *
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

#define DEBUG_PREFIX "NepomukCollection"

#include "NepomukCollection.h"
#include "meta/NepomukTrack.h"
#include "meta/NepomukAlbum.h"
#include "meta/NepomukArtist.h"
#include "meta/NepomukComposer.h"
#include "meta/NepomukGenre.h"

#include "core/meta/Meta.h"
#include "core/meta/support/MetaKeys.h"
#include "core/collections/QueryMaker.h"
#include "core/support/Debug.h"
#include "core-impl/collections/support/MemoryCollection.h"
#include "core-impl/collections/support/MemoryQueryMaker.h"

#include <Nepomuk/Resource>
#include <Nepomuk/Variant>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Query/ComparisonTerm>
#include <Nepomuk/Query/Query>
#include <Nepomuk/Query/Term>
#include <Nepomuk/Query/Result>
#include <Nepomuk/Query/QueryServiceClient>
#include <Nepomuk/Query/ResourceTypeTerm>
#include <Nepomuk/Vocabulary/NFO>
#include <Nepomuk/Vocabulary/NMM>
#include <Nepomuk/Vocabulary/NIE>

#include <KIcon>
#include <QString>
#include <QMap>

using namespace Meta;
using namespace Collections;
using namespace Nepomuk::Query;

NepomukCollection::NepomukCollection()
    : Collection()
    , m_mc( new Collections::MemoryCollection() )
{
    // check if Nepomuk is available, if yes, initialize.
    if( Nepomuk::ResourceManager::instance()->initialized() )
    {
        m_nepomukCollectionReady = true;

        if( buildCollection() )
            debug() << "successful! :)";
        else debug() << "not successful :(";
    }

    else m_nepomukCollectionReady = false;

}

NepomukCollection::~NepomukCollection()
{
    m_nepomukCollectionReady = false;
}

Collections::QueryMaker*
NepomukCollection::queryMaker()
{
    return new MemoryQueryMaker( m_mc.toWeakRef(), collectionId() );
}

QString
NepomukCollection::uidUrlProtocol() const
{
    static const QString uid( "amarok-nepomuk" );
    return uid;
}

QString
NepomukCollection::collectionId() const
{
    return QString( "%1://" ).arg( uidUrlProtocol() );
}

QString
NepomukCollection::prettyName() const
{
    return QString( "Nepomuk Collection" );
}

KIcon
NepomukCollection::icon() const
{
    return KIcon( "nepomuk" );
}

bool
NepomukCollection::isWritable() const
{
    // Nepomuk if initialized is always writable
    // A check for nepomuk initialized will suffice
    return m_nepomukCollectionReady;
}

bool
NepomukCollection::buildCollection()
{
    DEBUG_BLOCK

    m_mc->acquireWriteLock();
    setupMetaMap();

    m_mc->releaseLock();

    // TODO
    // year??

    // only checking for trackMap now.
    // Should ideally check for more conditions.

    if( m_mc->trackMap().size() == 0 )
        return false;

    else return true;
}



void
NepomukCollection::setupMetaMap()
{
    DEBUG_BLOCK
    Query query;

    Term term =  ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Audio() );
    query.setTerm( term );
    QList<Nepomuk::Query::Result> queriedResults = QueryServiceClient::syncQuery( query );

    Q_FOREACH( const Nepomuk::Query::Result & result, queriedResults )
    {
        ArtistPtr artistPtr;
        GenrePtr genrePtr;
        ComposerPtr composerPtr;
        AlbumPtr albumPtr;
        Nepomuk::Resource trackResource = result.resource();

        //get and insert artist into artistMap

        // NMM::performer returns resource of contact, so first get uri
        QUrl artisturi = trackResource.property( Nepomuk::Vocabulary::NMM::performer() ).toUrl();
        // Construct resource using uri
        Nepomuk::Resource artistRes( artisturi );
        // get label using uri
        //TODO : use a constructor that takes in a resource as a parameter
        // for NepomukArtist
        QString artistLabel = artistRes.genericLabel();
        NepomukArtistPtr nepArtistPtr( new NepomukArtist( artistLabel ) );
        debug() << "inserting artist : " << artistLabel;
        artistPtr = Meta::ArtistPtr::staticCast( nepArtistPtr );

        //get and insert genre into genreMap
        QString genre = trackResource.property( Nepomuk::Vocabulary::NMM::genre() ).toString();
        NepomukGenrePtr nepGenrePtr( new NepomukGenre( genre ) );
        debug() << "inserting genre : " << genre;
        genrePtr = Meta::GenrePtr::staticCast( nepGenrePtr );


        // NMM::composer returns resource of contact, so first get uri
        QUrl composeruri = trackResource.property( Nepomuk::Vocabulary::NMM::composer() ).toUrl();
        // Construct resource using uri
        Nepomuk::Resource composerRes( composeruri );
        // get label using uri
        //TODO : use a constructor that takes in a resource as a parameter
        // for NepomukComposer
        QString composerLabel = composerRes.genericLabel();
        NepomukComposerPtr nepComposerPtr( new NepomukComposer( composerLabel ) );
        debug() << "inserting composer : " << composerLabel;
        composerPtr = Meta::ComposerPtr::staticCast( nepComposerPtr );
        m_mc->addComposer( composerPtr );

        // album, right now only musicAlbum -> TODO : Change to MusicAlbum
        // NMM::musicAlbum() returns resource of album, so get uri
        QUrl albumuri = trackResource.property( Nepomuk::Vocabulary::NMM::musicAlbum() ).toUrl();
        // Construct resource
        Nepomuk::Resource albumRes( albumuri );
        // get label using uri
        //TODO : use a constructor that takes in a resource as a parameter
        // for NepomukAlbum
        QString albumLabel = albumRes.genericLabel();
        NepomukAlbumPtr nepAlbumPtr( new NepomukAlbum( albumLabel ) );
        debug() << "inserting album : " << albumLabel;
        albumPtr = Meta::AlbumPtr::staticCast( nepAlbumPtr );


        // populated meta pointers, now add track to map.
        NepomukTrackPtr nepTrackPtr( new NepomukTrack( artistPtr,
                                     genrePtr,
                                     composerPtr,
                                     albumPtr,
                                     trackResource ) );
        TrackPtr trackPtr = Meta::TrackPtr::staticCast( nepTrackPtr );
        m_mc->addTrack( trackPtr );
        debug() << "inserting track with track name : " << trackPtr->name();
        //debug()<<"and artist "<<artistPtr->name();

        // add track to artistPtr
        nepArtistPtr->addTrack( trackPtr );
        m_mc->addArtist( Meta::ArtistPtr::staticCast( nepArtistPtr ) );

        // add track to genrePtr
        nepGenrePtr->addTrack( trackPtr );
        m_mc->addGenre( Meta::GenrePtr::staticCast( nepGenrePtr ) );

        // add track to composerPtr
        nepComposerPtr->addTrack( trackPtr );
        m_mc->addComposer( Meta::ComposerPtr::staticCast( nepComposerPtr ) );

        // add track to albumPtr
        nepAlbumPtr->addTrack( trackPtr );
        m_mc->addAlbum( Meta::AlbumPtr::staticCast( nepAlbumPtr ) );

    }

}
