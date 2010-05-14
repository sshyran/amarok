/****************************************************************************************
 * Copyright (c) 2010 Nikhil Marathe <nsm.nikhil@gmail.com>                             *
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

#define DEBUG_PREFIX "UpnpCollection"

#include "UpnpCollection.h"

#include "core/support/Debug.h"
#include "MemoryQueryMaker.h"
#include "statusbar/StatusBar.h"

#include <QStringList>
#include <QTimer>
#include <QUuid>

#include <KLocale>

#include <HUpnp>
#include <HControlPoint>
#include <HDevice>
#include <HDeviceInfo>
#include <HService>

using namespace Herqq::Upnp;

namespace Collections {

AMAROK_EXPORT_COLLECTION( UpnpCollectionFactory, upnpcollection )

UpnpCollectionFactory::UpnpCollectionFactory( QObject *parent, const QVariantList &args )
    : Collections::CollectionFactory()
    , m_controlPoint(new HControlPoint(NULL, this))
{
    setParent( parent );
    Q_UNUSED( args );
}

UpnpCollectionFactory::~UpnpCollectionFactory()
{
    delete m_controlPoint;
}

void UpnpCollectionFactory::init()
{
    DEBUG_BLOCK
    connect(m_controlPoint,
            SIGNAL(rootDeviceOnline(Herqq::Upnp::HDevice *)),
            this,
            SLOT(rootDeviceOnline(Herqq::Upnp::HDevice *)));
    if( !m_controlPoint->init() ) {
        debug() << "HControlPoint::init failed" << m_controlPoint->errorDescription();
        return;
    }
    debug() << "Control point started?" << m_controlPoint->isStarted();
    emit newCollection( new UpnpCollection );
}

void UpnpCollectionFactory::rootDeviceOnline(HDevice *device)
{
// TODO should we check embedded devices?

    if( device->deviceInfo().deviceType().toString(HResourceType::TypeSuffix) == "MediaServer" ) {
        //emit newCollection(new UpnpCollection(device));
    }
}

//UpnpCollection

UpnpCollection::UpnpCollection()
    : Collection()
{
    DEBUG_BLOCK
}

UpnpCollection::~UpnpCollection()
{
}

void
UpnpCollection::startFullScan()
{
    DEBUG_BLOCK
        debug() << "+++++ FULL SCAN REQUESTED";
    //ignore
}

QueryMaker*
UpnpCollection::queryMaker()
{
        debug() << "+++++ Querymaker requested";
    return new MemoryQueryMaker( m_mc.toWeakRef(), collectionId() );
}

QString
UpnpCollection::collectionId() const
{
    return QString("upnp://") + "bazinga" + QUuid::createUuid().toString();
}

QString
UpnpCollection::prettyName() const
{
    return i18n("Upnp Bazinga");
}

bool
UpnpCollection::possiblyContainsTrack( const KUrl &url ) const
{
    debug() << "Requested track " << url;
    return false;
}

} //~ namespace
#include "UpnpCollection.moc"

