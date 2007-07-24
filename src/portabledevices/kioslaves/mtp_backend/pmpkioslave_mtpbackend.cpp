/*
 *  Copyright (c) 2007 Jeff Mitchell <kde-dev@emailgoeshere.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "pmpkioslave_mtpbackend.h"
#include "pmpkioslave.h"

#include <sys/stat.h>

#include <QCoreApplication>
#include <QString>
#include <QVariant>

#include <kcomponentdata.h>
#include <kdebug.h>
#include <kurl.h>
#include <kio/slavebase.h>
#include <solid/device.h>
#include <solid/genericinterface.h>

MTPBackend::MTPBackend( PMPProtocol* slave, const Solid::Device &device )
            : PMPBackend( slave, device )
            , m_device( 0 )
{
    kDebug() << "Creating MTPBackend" << endl;

    if( !m_slave->mtpInitialized() )
        LIBMTP_Init();

    if( LIBMTP_Get_Connected_Devices( &m_deviceList ) != LIBMTP_ERROR_NONE )
    {
        m_slave->error( KIO::ERR_INTERNAL, "Could not get a connected device list from libmtp." );
        return;
    }
    quint32 deviceCount = LIBMTP_Number_Devices_In_List( m_deviceList );
    if( deviceCount == 0 )
    {
        m_slave->error( KIO::ERR_INTERNAL, "libmtp found no devices." );
        return;
    }
}

MTPBackend::~MTPBackend()
{
    kDebug() << endl << "In MTPBackend destructor, releasing device" << endl << endl;
    if( m_device )
        LIBMTP_Release_Device( m_device );
}

void
MTPBackend::initialize()
{
    kDebug() << "Initializing MTPBackend for device " << m_solidDevice.udi() << endl;
    Solid::GenericInterface *gi = m_solidDevice.as<Solid::GenericInterface>();
    if( !gi )
    {
        m_slave->error( KIO::ERR_INTERNAL, "Error getting a GenericInterface to the device from Solid." );
        return;
    }
    if( !gi->propertyExists( "usb.serial" ) )
    {
        m_slave->error( KIO::ERR_INTERNAL, "Could not find serial number of MTP device in HAL.  When filing a bug please include the full output of \"lshal -lt\"." );
        return;
    }
    QVariant possibleSerial = gi->property( "usb.serial" );
    if( !possibleSerial.isValid() || possibleSerial.isNull() || !possibleSerial.canConvert( QVariant::String ) )
    {
        m_slave->error( KIO::ERR_INTERNAL, "Did not get an expected String from HAL." );
        return;
    }
    QString serial = possibleSerial.toString();
    kDebug() << endl << endl << "Case-insensitively looking for serial number starting with: " << serial << endl << endl;
    LIBMTP_mtpdevice_t *currdevice;
    for( currdevice = m_deviceList; currdevice != NULL; currdevice = currdevice->next )
    {
        kDebug() << "currdevice serial number = " << LIBMTP_Get_Serialnumber( currdevice ) << endl;
        //WARNING: a startsWith is done below, as the value reported by HAL for the serial number seems to be about half
        //the length of the value reported by libmtp...is this always true?  Could this cause two devices to
        //be recognized as the same one?
        if( QString( LIBMTP_Get_Serialnumber( currdevice ) ).startsWith( serial, Qt::CaseInsensitive ) )
        {
            kDebug() << endl << endl << "Found a matching serial!" << endl << endl;
            m_device = currdevice;
        }
        else
            LIBMTP_Release_Device( currdevice );
    }

    if( m_device )
        kDebug() << "FOUND THE MTP DEVICE WE WERE LOOKING FOR!" << endl;

    return;
}

QString
MTPBackend::getFriendlyName()
{
    kDebug() << "Getting MTPBackend friendly name" << endl;
    char* name = LIBMTP_Get_Friendlyname( m_device );
    QString friendlyName = QString::fromUtf8( name );
    free( name );
    return friendlyName;
}

void
MTPBackend::setFriendlyName( const QString &name )
{
    kDebug() << "Setting MTPBackend friendly name" << endl;
    if( LIBMTP_Set_Friendlyname( m_device, name.toUtf8() ) != 0 )
        m_slave->warning( "Failed to set friendly name on the device!" );
}

QString
MTPBackend::getModelName()
{
    kDebug() << "Getting MTPBackend model name" << endl;
    char* model = LIBMTP_Get_Modelname( m_device );
    QString modelName = QString::fromUtf8( model );
    free( model );
    return modelName;
}

void
MTPBackend::get( const KUrl &url )
{
   QString path = getFilePath( url );
   kDebug() << "in MTPBackend::get, path is: " << path << endl;
}

void
MTPBackend::listDir( const KUrl &url )
{
   QString path = getFilePath( url );
   kDebug() << "in MTPBackend::listDir, path is: " << path << endl;
   //first case: no specific folder chosen, display a list of available actions as folders
   if( path.isEmpty() || path == "/" )
   {
        QStringList folders;
        folders << "Playlists" << "Tracks";
        foreach( QString folder, folders )
        {
            KIO::UDSEntry entry;
            entry.insert( KIO::UDSEntry::UDS_NAME ,folder);
            entry.insert( KIO::UDSEntry::UDS_FILE_TYPE , S_IFDIR);
            entry.insert(KIO::UDSEntry::UDS_ACCESS ,S_IRUSR | S_IRGRP | S_IROTH);
            m_slave->listEntry( entry, false );
        }
        m_slave->listEntry( KIO::UDSEntry(), true );
   }
}

void
MTPBackend::rename( const KUrl &src, const KUrl &dest, bool overwrite )
{
    Q_UNUSED( src );
    Q_UNUSED( dest );
    Q_UNUSED( overwrite );
    //make sure they're renaming playlists to playlists, tracks to tracks, etc...
}

void
MTPBackend::stat( const KUrl &url )
{
   QString path = getFilePath( url );
   kDebug() << "in MTPBackend::stat, path is: " << path << endl;
   KIO::UDSEntry entry;
   entry.insert( KIO::UDSEntry::UDS_NAME,m_solidDevice.product());
   entry.insert(KIO::UDSEntry::UDS_FILE_TYPE,S_IFDIR);
   entry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);
   m_slave->statEntry( entry );
}

#include "pmpkioslave_mtpbackend.moc"

