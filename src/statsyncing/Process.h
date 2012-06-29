/****************************************************************************************
 * Copyright (c) 2012 Matěj Laitl <matej@laitl.cz>                                      *
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

#ifndef STATSYNCING_PROCESS_H
#define STATSYNCING_PROCESS_H

#include "statsyncing/Options.h"

#include <QSharedPointer>
#include <QMap>

namespace ThreadWeaver {
    class Job;
}

namespace StatSyncing
{

class ChooseProvidersPage;
    class MatchedTracksPage;
    class MatchedTracksModel;
    class ProvidersModel;

    /**
     * Class that is responsible for one synchronization process from track matching
     * to commiting synchronized values back to storage. This class should live in a main
     * thread and is event-based.
     *
     * Process auto-deletes itself when it is done with its work.
     */
    class Process : public QObject
    {
        Q_OBJECT

        public:
            enum Mode {
                Interactive,
                NonInteractive,
            };

            /**
             * Creates the synchronization process that will offer user to synchronize
             * @param fields of @param providers. If @param mode is Interactive,
             * introductory dialog will be shown that allows subset of fields and
             * providers to be chosen. Otherwise performs the syncing ing the background
             * and shows a window only if conflict occurs.
             */
            Process( const QList<QSharedPointer<Provider> > &providers,
                     const QList<qint64> &fields, Mode mode, QObject *parent = 0 );
            ~Process();

        public slots:
            /**
             * Starts the process.
             */
            void start();

            /**
             * Raises and activates possible UI window related to this synchronization
             * process.
             */
            void raise();

        private slots:
            void slotMatchTracks();
            void slotTracksMatched( ThreadWeaver::Job* job );
            void slotSynchronize();

        private:
            Q_DISABLE_COPY( Process )

            Mode m_mode;
            Options m_options;
            ProvidersModel *m_providersModel;
            QList<qint64> m_fields;
            MatchedTracksModel *m_matchedTracksModel;

            QWeakPointer<ChooseProvidersPage> m_providersPage;
            QWeakPointer<MatchedTracksPage> m_tracksPage;
    };

} // namespace StatSyncing

#endif // STATSYNCING_PROCESS_H
