/****************************************************************************************
 * Copyright (c) 2013 Konrad Zemek <konrad.zemek@gmail.com>                             *
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

#include "ImporterSqlTrack.h"

#include "ImporterSqlProvider.h"

using namespace StatSyncing;

ImporterSqlTrack::ImporterSqlTrack( const ImporterSqlProviderPtr &provider,
                                    const Meta::FieldHash &metadata,
                                    const QSet<QString> &labels )
    : SimpleWritableTrack( metadata, labels )
    , m_provider( provider )
{
}

ImporterSqlTrack::~ImporterSqlTrack()
{
}

void
ImporterSqlTrack::doCommit( const QSet<qint64> &fields )
{
    QMetaObject::invokeMethod( this, "slotSqlCommit",
                               m_provider->getBlockingConnectionType(),
                               Q_ARG( QSet<qint64>, fields ) );
}

void ImporterSqlTrack::slotSqlCommit( const QSet<qint64> &fields )
{
    m_provider->prepareConnection();
    QSqlDatabase db = QSqlDatabase::database( m_provider->m_connectionName );

    if( db.open() )
        sqlCommit( db, fields );
}
