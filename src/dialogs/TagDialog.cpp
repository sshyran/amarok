/****************************************************************************************
 * Copyright (c) 2004 Mark Kretschmann <kretschmann@kde.org>                            *
 * Copyright (c) 2004 Pierpaolo Di Panfilo <pippo_dp@libero.it>                         *
 * Copyright (c) 2005-2006 Alexandre Pereira de Oliveira <aleprj@gmail.com>             *
 * Copyright (c) 2008 Téo Mrnjavac <teo@kde.org>                                        *
 * Copyright (c) 2008 Leo Franchi <lfranchi@kde.org>                                    *
 * Copyright (c) 2009 Daniel Dewald <Daniel.Dewald@time-shift.de>                       *
 * Copyright (c) 2009 Pierre Dumuid <pmdumuid@gmail.com>                                *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
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

#define DEBUG_PREFIX "TagDialog"

#include "TagDialog.h"

#include "MainWindow.h"
#include "SvgHandler.h"
#include "core/collections/QueryMaker.h"
#include "core/interfaces/Logger.h"
#include "core/meta/Statistics.h"
#include "core/meta/TrackEditor.h"
#include "core/meta/support/MetaUtility.h"
#include "core/support/Amarok.h"
#include "core/support/Components.h"
#include "core/support/Debug.h"
#include "core-impl/collections/support/CollectionManager.h"
#include "covermanager/CoverFetchingActions.h"
#include "dialogs/MusicBrainzTagger.h"
#include "widgets/CoverLabel.h"
#include "widgets/FilenameLayoutWidget.h"
#include "ui_TagDialogBase.h" // needs to be after including CoverLabel, silly
#include "TagGuesserDialog.h"

#include <KLineEdit>
#include <KMenu>
#include <KRun>

namespace Meta {
namespace Field {
    const QString LABELS = "labels";
    const QString LYRICS = "lyrics";
    const QString TYPE = "type";
    const QString COLLECTION = "collection";
    const QString NOTE = "note";
}
}

TagDialog::TagDialog( const Meta::TrackList &tracks, QWidget *parent )
    : KDialog( parent )
    , m_perTrack( true )
    , m_currentTrackNum( 0 )
    , m_changed( false )
    , m_queryMaker( 0 )
    , ui( new Ui::TagDialogBase() )
{
    DEBUG_BLOCK

    foreach( Meta::TrackPtr track, tracks )
        addTrack( track );

    ui->setupUi( mainWidget() );
    resize( minimumSizeHint() );
    initUi();
    setCurrentTrack( 0 );
}

TagDialog::TagDialog( Meta::TrackPtr track, QWidget *parent )
    : KDialog( parent )
    , m_perTrack( true )
    , m_currentTrackNum( 0 )
    , m_changed( false )
    , m_queryMaker( 0 )
    , ui( new Ui::TagDialogBase() )
{
    DEBUG_BLOCK

    addTrack( track );
    ui->setupUi( mainWidget() );
    resize( minimumSizeHint() );
    initUi();
    setCurrentTrack( 0 );

    QTimer::singleShot( 0, this, SLOT(show()) );
}

TagDialog::TagDialog( Collections::QueryMaker *qm )
    : KDialog( The::mainWindow() )
    , m_perTrack( true )
    , m_currentTrackNum( 0 )
    , m_changed( false )
    , m_queryMaker( qm )
    , ui( new Ui::TagDialogBase() )
{
    DEBUG_BLOCK

    ui->setupUi( mainWidget() );
    resize( minimumSizeHint() );

    qm->setQueryType( Collections::QueryMaker::Track );
    connect( qm, SIGNAL(newResultReady(Meta::TrackList)), this, SLOT(resultReady(Meta::TrackList)), Qt::QueuedConnection );
    connect( qm, SIGNAL(queryDone()), this, SLOT(queryDone()), Qt::QueuedConnection );
    qm->run();
}

TagDialog::~TagDialog()
{
    DEBUG_BLOCK

    Amarok::config( "TagDialog" ).writeEntry( "CurrentTab", ui->kTabWidget->currentIndex() );

    if( m_currentTrack && m_currentTrack->album() )
        unsubscribeFrom( m_currentTrack->album() );

    delete ui;
}

void
TagDialog::metadataChanged( Meta::AlbumPtr album )
{
    if( !m_currentTrack || !m_currentTrack->album() )
        return;

    // If the metadata of the current album has changed, reload the cover
    if( album == m_currentTrack->album() )
        updateCover();

    // TODO: if the lyrics changed: should we show a warning and ask the user
    // if he wants to use the new lyrics?
}


////////////////////////////////////////////////////////////////////////////////
// PRIVATE SLOTS
////////////////////////////////////////////////////////////////////////////////

void
TagDialog::addTrack( Meta::TrackPtr &track )
{
    if( !m_tracks.contains( track ) )
    {
        m_tracks.append( track );
        m_storedTags.insert( track, getTagsFromTrack( track ) );
    }
}

void
TagDialog::resultReady( const Meta::TrackList &tracks )
{
    foreach( Meta::TrackPtr track, tracks )
        addTrack( track );
}

void
TagDialog::queryDone()
{
    delete m_queryMaker;
    if( !m_tracks.isEmpty() )
    {
        initUi();
        setCurrentTrack( 0 );

        QTimer::singleShot( 0, this, SLOT(show()) );
    }
    else
    {
        deleteLater();
    }
}

void
TagDialog::resultReady( const Meta::AlbumList &albums )
{
    foreach( const Meta::AlbumPtr &album, albums )
    {
        if( !album->name().isEmpty() )
            m_albums << album->name();

        if( album->hasAlbumArtist() && !album->albumArtist()->name().isEmpty() )
            m_albumArtists << album->albumArtist()->name();
    }
}

void
TagDialog::resultReady( const Meta::ArtistList &artists )
{
    foreach( const Meta::ArtistPtr &artist, artists )
    {
        if( !artist->name().isEmpty() )
            m_artists << artist->name();
    }
}

void
TagDialog::resultReady( const Meta::ComposerList &composers )
{
    foreach( const Meta::ComposerPtr &composer, composers )
    {
        if( !composer->name().isEmpty() )
            m_composers << composer->name();
    }
}

void
TagDialog::resultReady( const Meta::GenreList &genres )
{
    foreach( const Meta::GenrePtr &genre, genres )
    {
        if( !genre->name().isEmpty() )  // Where the heck do the empty genres come from?
            m_genres << genre->name();
    }
}


void
TagDialog::resultReady( const Meta::LabelList &labels )
{
    foreach( const Meta::LabelPtr &label, labels )
    {
        if( !label->name().isEmpty() )
            m_allLabels << label->name();
    }
}

void
TagDialog::dataQueryDone()
{
    // basically we want to ignore the fact that the fields are being
    // edited because we do it not the user, so it results in empty
    // tags being saved to files---data loss is BAD!
    bool oldChanged = m_changed;

    //we simply clear the completion data of all comboboxes
    //then load the current track again. that's more work than necessary
    //but the performance impact should be negligible
    // we do this because if we insert items and the contents of the textbox
    // are not in the list, it clears the textbox. which is bad --lfranchi 2.22.09
    QString saveText( ui->kComboBox_artist->lineEdit()->text() );
    QStringList artists = m_artists.toList();
    artists.sort();
    ui->kComboBox_artist->clear();
    ui->kComboBox_artist->insertItems( 0, artists );
    ui->kComboBox_artist->completionObject()->setItems( artists );
    ui->kComboBox_artist->lineEdit()->setText( saveText );

    saveText = ui->kComboBox_album->lineEdit()->text();
    QStringList albums = m_albums.toList();
    albums.sort();
    ui->kComboBox_album->clear();
    ui->kComboBox_album->insertItems( 0, albums );
    ui->kComboBox_album->completionObject()->setItems( albums );
    ui->kComboBox_album->lineEdit()->setText( saveText );

    saveText = ui->kComboBox_albumArtist->lineEdit()->text();
    QStringList albumArtists = m_albumArtists.toList();
    albumArtists.sort();
    ui->kComboBox_albumArtist->clear();
    ui->kComboBox_albumArtist->insertItems( 0, albumArtists );
    ui->kComboBox_albumArtist->completionObject()->setItems( albumArtists );
    ui->kComboBox_albumArtist->lineEdit()->setText( saveText );

    saveText = ui->kComboBox_composer->lineEdit()->text();
    QStringList composers = m_composers.toList();
    composers.sort();
    ui->kComboBox_composer->clear();
    ui->kComboBox_composer->insertItems( 0, composers );
    ui->kComboBox_composer->completionObject()->setItems( composers );
    ui->kComboBox_composer->lineEdit()->setText( saveText );

    saveText = ui->kComboBox_genre->lineEdit()->text();
    QStringList genres = m_genres.toList();
    genres.sort();
    ui->kComboBox_genre->clear();
    ui->kComboBox_genre->insertItems( 0, genres );
    ui->kComboBox_genre->completionObject()->setItems( genres );
    ui->kComboBox_genre->lineEdit()->setText( saveText );

    saveText = ui->kComboBox_label->lineEdit()->text();
    QStringList labels = m_allLabels.toList();
    labels.sort();
    ui->kComboBox_label->clear();
    ui->kComboBox_label->insertItems( 0, labels );
    ui->kComboBox_label->completionObject()->setItems( labels );
    ui->kComboBox_label->lineEdit()->setText( saveText );

    m_changed = oldChanged;
}

void
TagDialog::removeLabelPressed() //SLOT
{
    if( ui->labelsList->selectionModel()->hasSelection() )
    {
        QModelIndexList idxList = ui->labelsList->selectionModel()->selectedRows();
        QStringList selection;

        for( int x = 0; x < idxList.size(); ++x )
        {
            QString label = idxList.at(x).data( Qt::DisplayRole ).toString();
            selection.append( label );
        }

        m_labelModel->removeLabels( selection );

        ui->labelsList->selectionModel()->reset();
        labelSelected();

        checkChanged();
    }
}

void
TagDialog::addLabelPressed() //SLOT
{
    QString label = ui->kComboBox_label->currentText();

    if( !label.isEmpty() )
    {
        m_labelModel->addLabel( label );
        ui->kComboBox_label->setCurrentIndex( -1 );
        ui->kComboBox_label->completionObject()->insertItems( QStringList( label ) );

        if ( !ui->kComboBox_label->contains( label ) )
            ui->kComboBox_label->addItem( label );

        checkChanged();
    }
}

void
TagDialog::cancelPressed() //SLOT
{
    QApplication::restoreOverrideCursor(); // restore the cursor before closing the dialog (The musicbrainz dialog might have set it)
    reject();
}


void
TagDialog::accept() //SLOT
{
    ui->pushButton_ok->setEnabled( false ); //visual feedback
    saveTags();

    KDialog::accept();
}


inline void
TagDialog::openPressed() //SLOT
{
    new KRun( m_path, this );
}


inline void
TagDialog::previousTrack()
{
    setCurrentTrack( m_currentTrackNum - 1 );
}


inline void
TagDialog::nextTrack()
{
    setCurrentTrack( m_currentTrackNum + 1 );
}

inline void
TagDialog::perTrack( bool enabled )
{
    if( enabled == m_perTrack )
        return;

    setTagsToTrack();
    setPerTrack( enabled );
    setTagsToUi();
}


void
TagDialog::checkChanged() //SLOT
{
    QVariantMap oldTags;
    if( m_perTrack )
        oldTags = m_storedTags.value( m_currentTrack );
    else
        oldTags = getTagsFromMultipleTracks();
    QVariantMap newTags = getTagsFromUi( oldTags );

    ui->pushButton_ok->setEnabled( m_changed || !newTags.isEmpty() );
}

inline void
TagDialog::labelModified() //SLOT
{
    ui->addButton->setEnabled( ui->kComboBox_label->currentText().length()>0 );
}

inline void
TagDialog::labelSelected() //SLOT
{
    ui->removeButton->setEnabled( ui->labelsList->selectionModel()->hasSelection() );
}

//creates a KDialog and executes the FilenameLayoutWidget. Grabs a filename scheme, extracts tags (via TagGuesser) from filename and fills the appropriate fields on TagDialog.
void
TagDialog::guessFromFilename() //SLOT
{
    TagGuesserDialog dialog( m_currentTrack->playableUrl().path(), this );

    if( dialog.exec() == KDialog::Accepted )
    {
        dialog.onAccept();

        int cur = 0;

        QMap<qint64,QString> tags = dialog.guessedTags();

        if( !tags.isEmpty() )
        {

            if( tags.contains( Meta::valTitle ) )
                ui->kLineEdit_title->setText( tags[Meta::valTitle] );

            if( tags.contains( Meta::valArtist ) )
            {
                cur = ui->kComboBox_artist->currentIndex();
                ui->kComboBox_artist->setItemText( cur, tags[Meta::valArtist] );
            }

            if( tags.contains( Meta::valAlbum ) )
            {
                cur = ui->kComboBox_album->currentIndex();
                ui->kComboBox_album->setItemText( cur, tags[Meta::valAlbum] );
            }

            if( tags.contains( Meta::valAlbumArtist ) )
            {
                cur = ui->kComboBox_albumArtist->currentIndex();
                ui->kComboBox_albumArtist->setItemText( cur, tags[Meta::valAlbumArtist] );
            }

            if( tags.contains( Meta::valTrackNr ) )
                ui->qSpinBox_track->setValue( tags[Meta::valTrackNr].toInt() );

            if( tags.contains( Meta::valComment ) )
                ui->qPlainTextEdit_comment->setPlainText( tags[Meta::valComment] );

            if( tags.contains( Meta::valYear ) )
                ui->qSpinBox_year->setValue( tags[Meta::valYear].toInt() );

            if( tags.contains( Meta::valComposer ) )
            {
                cur = ui->kComboBox_composer->currentIndex();
                ui->kComboBox_composer->setItemText( cur, tags[Meta::valComposer] );
            }

            if( tags.contains( Meta::valGenre ) )
            {
                cur = ui->kComboBox_genre->currentIndex();
                ui->kComboBox_genre->setItemText( cur, tags[Meta::valGenre] );
            }

            if( tags.contains( Meta::valDiscNr ) )
            {
                ui->qSpinBox_discNumber->setValue( tags[Meta::valDiscNr].toInt() );
            }
        }
        else
        {
            debug() << "guessing tags from filename failed" << endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// PRIVATE
////////////////////////////////////////////////////////////////////////////////

void TagDialog::initUi()
{
    DEBUG_BLOCK
    // delete itself when closing
    setAttribute( Qt::WA_DeleteOnClose );
    setButtons( KDialog::None );

    KConfigGroup config = Amarok::config( "TagDialog" );

    ui->kTabWidget->addTab( ui->summaryTab   , i18n( "Summary" ) );
    ui->kTabWidget->addTab( ui->tagsTab      , i18n( "Tags" ) );
    ui->kTabWidget->addTab( ui->lyricsTab    , i18n( "Lyrics" ) );
    ui->kTabWidget->addTab( ui->labelsTab , i18n( "Labels" ) );

    ui->kComboBox_label->completionObject()->setIgnoreCase( true );
    ui->kComboBox_label->setCompletionMode( KGlobalSettings::CompletionPopup );

    m_labelModel = new LabelListModel( QStringList(), this );
    ui->labelsList->setModel( m_labelModel );
    ui->labelsTab->setEnabled( true );

    ui->kTabWidget->setCurrentIndex( config.readEntry( "CurrentTab", 0 ) );

    ui->kComboBox_artist->completionObject()->setIgnoreCase( true );
    ui->kComboBox_artist->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->kComboBox_album->completionObject()->setIgnoreCase( true );
    ui->kComboBox_album->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->kComboBox_albumArtist->completionObject()->setIgnoreCase( true );
    ui->kComboBox_albumArtist->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->kComboBox_composer->completionObject()->setIgnoreCase( true );
    ui->kComboBox_composer->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->kComboBox_genre->completionObject()->setIgnoreCase( true );
    ui->kComboBox_genre->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->kComboBox_label->completionObject()->setIgnoreCase( true );
    ui->kComboBox_label->setCompletionMode( KGlobalSettings::CompletionPopup );

    ui->addButton->setEnabled( false );
    ui->removeButton->setEnabled( false );

    // set an icon for the open-in-konqui button
    ui->pushButton_open->setIcon( KIcon( "folder-amarok" ) );

    connect( ui->pushButton_guessTags, SIGNAL(clicked()), SLOT(guessFromFilename()) );

    // Connects for modification check
    // only set to overwrite-on-save if the text has changed
    connect( ui->kLineEdit_title,       SIGNAL(textChanged(QString)),     SLOT(checkChanged()) );
    connect( ui->kComboBox_composer,    SIGNAL(activated(int)),                  SLOT(checkChanged()) );
    connect( ui->kComboBox_composer,    SIGNAL(editTextChanged(QString)), SLOT(checkChanged()) );
    connect( ui->kComboBox_artist,      SIGNAL(activated(int)),                  SLOT(checkChanged()) );
    connect( ui->kComboBox_artist,      SIGNAL(editTextChanged(QString)), SLOT(checkChanged()) );
    connect( ui->kComboBox_album,       SIGNAL(activated(int)),                  SLOT(checkChanged()) );
    connect( ui->kComboBox_album,       SIGNAL(editTextChanged(QString)), SLOT(checkChanged()) );
    connect( ui->kComboBox_albumArtist, SIGNAL(activated(int)),                  SLOT(checkChanged()) );
    connect( ui->kComboBox_albumArtist, SIGNAL(editTextChanged(QString)), SLOT(checkChanged()) );
    connect( ui->kComboBox_genre,       SIGNAL(activated(int)),                  SLOT(checkChanged()) );
    connect( ui->kComboBox_genre,       SIGNAL(editTextChanged(QString)), SLOT(checkChanged()) );
    connect( ui->kLineEdit_Bpm,         SIGNAL(textChanged(QString))    , SLOT(checkChanged()) );
    connect( ui->ratingWidget,          SIGNAL(ratingChanged(int)),              SLOT(checkChanged()) );
    connect( ui->qSpinBox_track,        SIGNAL(valueChanged(int)),               SLOT(checkChanged()) );
    connect( ui->qSpinBox_year,         SIGNAL(valueChanged(int)),               SLOT(checkChanged()) );
    connect( ui->qSpinBox_score,        SIGNAL(valueChanged(int)),               SLOT(checkChanged()) );
    connect( ui->qPlainTextEdit_comment,SIGNAL(textChanged()),                     SLOT(checkChanged()) );
    connect( ui->kRichTextEdit_lyrics,  SIGNAL(textChanged()),                     SLOT(checkChanged()) );
    connect( ui->qSpinBox_discNumber,   SIGNAL(valueChanged(int)),               SLOT(checkChanged()) );

    connect( ui->pushButton_cancel,   SIGNAL(clicked()), SLOT(cancelPressed()) );
    connect( ui->pushButton_ok,       SIGNAL(clicked()), SLOT(accept()) );
    connect( ui->pushButton_open,     SIGNAL(clicked()), SLOT(openPressed()) );
    connect( ui->pushButton_previous, SIGNAL(clicked()), SLOT(previousTrack()) );
    connect( ui->pushButton_next,     SIGNAL(clicked()), SLOT(nextTrack()) );
    connect( ui->checkBox_perTrack,   SIGNAL(toggled(bool)), SLOT(perTrack(bool)) );

    connect( ui->addButton,           SIGNAL(clicked()),                          SLOT(addLabelPressed()) );
    connect( ui->removeButton,        SIGNAL(clicked()),                          SLOT(removeLabelPressed()) );
    connect( ui->kComboBox_label,     SIGNAL(activated(int)),                   SLOT(labelModified()) );
    connect( ui->kComboBox_label,     SIGNAL(editTextChanged(QString)),  SLOT(labelModified()) );
    connect( ui->kComboBox_label,     SIGNAL(returnPressed()),                    SLOT(addLabelPressed()) );
    connect( ui->kComboBox_label,     SIGNAL(returnPressed()),                    SLOT(checkChanged()) );
    connect( ui->labelsList,          SIGNAL(pressed(QModelIndex)),      SLOT(labelSelected()) );

    ui->pixmap_cover->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( ui->pixmap_cover, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showCoverMenu(QPoint)) );

    connect( ui->pushButton_musicbrainz, SIGNAL(clicked()), SLOT(musicbrainzTagger()) );

    if( m_tracks.count() > 1 )
        setPerTrack( false );
    else
        setPerTrack( true );

    ui->pushButton_ok->setEnabled( false );

    startDataQueries();
}

void
TagDialog::setCurrentTrack( int num )
{
    if( num < 0 || num >= m_tracks.count() )
        return;

    if( m_currentTrack ) // even in multiple tracks mode we don't want to write back
        setTagsToTrack();

    // there is a logical problem here.
    // if the track itself changes (e.g. because it get's a new album)
    // then we don't re-subscribe
    if( m_currentTrack && m_currentTrack->album() )
        unsubscribeFrom( m_currentTrack->album() );

    m_currentTrack = m_tracks.at( num );
    m_currentTrackNum = num;

    if( m_currentTrack && m_currentTrack->album() )
        subscribeTo( m_currentTrack->album() );

    setControlsAccessability();
    updateButtons();
    setTagsToUi();
}

void
TagDialog::startDataQuery( Collections::QueryMaker::QueryType type, const char *signal,
                           const char *slot )
{
    Collections::QueryMaker *qm = CollectionManager::instance()->queryMaker();
    qm->setQueryType( type );

    connect( qm, SIGNAL(queryDone()), SLOT(dataQueryDone()), Qt::QueuedConnection );
    connect( qm, signal, slot,  Qt::QueuedConnection );

    qm->setAutoDelete( true );
    qm->run();
}

void
TagDialog::startDataQueries()
{
    startDataQuery( Collections::QueryMaker::Artist,
                    SIGNAL(newResultReady(Meta::ArtistList)),
                    SLOT(resultReady(Meta::ArtistList)) );
    startDataQuery( Collections::QueryMaker::Album,
                    SIGNAL(newResultReady(Meta::AlbumList)),
                    SLOT(resultReady(Meta::AlbumList)) );
    startDataQuery( Collections::QueryMaker::Composer,
                    SIGNAL(newResultReady(Meta::ComposerList)),
                    SLOT(resultReady(Meta::ComposerList)) );
    startDataQuery( Collections::QueryMaker::Genre,
                    SIGNAL(newResultReady(Meta::GenreList)),
                    SLOT(resultReady(Meta::GenreList)) );
    startDataQuery( Collections::QueryMaker::Label,
                    SIGNAL(newResultReady(Meta::LabelList)),
                    SLOT(resultReady(Meta::LabelList)) );
}


inline const QString
TagDialog::unknownSafe( const QString &s ) const
{
    return ( s.isNull() || s.isEmpty() || s == "?" || s == "-" )
           ? i18nc( "The value for this tag is not known", "Unknown" )
           : s;
}

inline const QString
TagDialog::unknownSafe( int i ) const
{
    return ( i == 0 )
           ? i18nc( "The value for this tag is not known", "Unknown" )
           : QString::number( i );
}

void
TagDialog::showCoverMenu( const QPoint &pos )
{
    Meta::AlbumPtr album = m_currentTrack->album();
    if( !album )
        return; // TODO: warning or something?

    QAction *displayCoverAction = new DisplayCoverAction( this, album );
    QAction *unsetCoverAction   = new UnsetCoverAction( this, album );

    if( !album->hasImage() )
    {
        displayCoverAction->setEnabled( false );
        unsetCoverAction->setEnabled( false );
    }

    KMenu *menu = new KMenu( this );
    menu->addAction( displayCoverAction );
    menu->addAction( new FetchCoverAction( this, album ) );
    menu->addAction( new SetCustomCoverAction( this, album ) );
    menu->addAction( unsetCoverAction );

    menu->exec( ui->pixmap_cover->mapToGlobal(pos) );
}

void
TagDialog::setTagsToUi( const QVariantMap &tags )
{
    bool oldChanged = m_changed;

    // -- the windows title
    if( m_perTrack )
    {
        setWindowTitle( KDialog::makeStandardCaption( i18n("Track Details: %1 by %2",
                                                           m_currentTrack->name(),  m_currentTrack->artist() ? m_currentTrack->artist()->name() : QString() ) ) );

    }
    else
    {
        setWindowTitle( KDialog::makeStandardCaption( i18ncp( "The amount of tracks being edited", "1 Track", "Information for %1 Tracks", m_tracks.count() ) ) );

    }

    // -- the title in the summary tab

    if( m_perTrack )
    {
        QString niceTitle;

        const QFontMetrics fnt =  ui->trackArtistAlbumLabel->fontMetrics();
        const int len = ui->trackArtistAlbumLabel->width();
        QString curTrackAlbName;
        QString curArtistName;

        QString curTrackName = fnt.elidedText( Qt::escape( m_currentTrack->name() ), Qt::ElideRight, len );
        QString curTrackPretName = fnt.elidedText( Qt::escape( m_currentTrack->prettyName() ), Qt::ElideRight, len );

        if( m_currentTrack->album() )
            curTrackAlbName = fnt.elidedText( Qt::escape( m_currentTrack->album()->name() ), Qt::ElideRight, len );
        if( m_currentTrack->artist() )
            curArtistName = fnt.elidedText( Qt::escape( m_currentTrack->artist()->name() ), Qt::ElideRight, len );


        if( m_currentTrack->album() && m_currentTrack->album()->name().isEmpty() )
        {
            if( !m_currentTrack->name().isEmpty() )
            {
                if( !m_currentTrack->artist()->name().isEmpty() )
                    niceTitle = i18n( "<b>%1</b> by <b>%2</b>", curTrackName,  curArtistName );
                else
                    niceTitle = i18n( "<b>%1</b>", curTrackName );
            }
            else
                niceTitle = curTrackPretName;
        }
        else if( m_currentTrack->album() )
            niceTitle = i18n( "<b>%1</b> by <b>%2</b> on <b>%3</b>" , curTrackName, curArtistName, curTrackAlbName );
        else if( m_currentTrack->artist() )
            niceTitle = i18n( "<b>%1</b> by <b>%2</b>" , curTrackName, curArtistName );
        else
            niceTitle = i18n( "<b>%1</b>" , curTrackName );

        ui->trackArtistAlbumLabel->setText( niceTitle );
    }
    else
    {
        ui->trackArtistAlbumLabel->setText( i18np( "Editing 1 file", "Editing %1 files", m_tracks.count() ) );

    }

    // -- the rest

    ui->kLineEdit_title->setText( tags.value( Meta::Field::TITLE ).toString() );
    selectOrInsertText( tags.value( Meta::Field::ARTIST ).toString(), ui->kComboBox_artist );
    selectOrInsertText( tags.value( Meta::Field::ALBUM ).toString(), ui->kComboBox_album );
    selectOrInsertText( tags.value( Meta::Field::ALBUMARTIST ).toString(), ui->kComboBox_albumArtist );
    selectOrInsertText( tags.value( Meta::Field::COMPOSER ).toString(), ui->kComboBox_composer );
    ui->qPlainTextEdit_comment->setPlainText( tags.value( Meta::Field::COMMENT ).toString() );
    selectOrInsertText( tags.value( Meta::Field::GENRE ).toString(), ui->kComboBox_genre );
    ui->qSpinBox_track->setValue( tags.value( Meta::Field::TRACKNUMBER ).toInt() );
    ui->qSpinBox_discNumber->setValue( tags.value( Meta::Field::DISCNUMBER ).toInt() );
    ui->qSpinBox_year->setValue( tags.value( Meta::Field::YEAR ).toInt() );
    ui->kLineEdit_Bpm->setText( tags.value( Meta::Field::BPM ).toString() );

    ui->qLabel_length->setText( unknownSafe( Meta::msToPrettyTime( tags.value( Meta::Field::LENGTH ).toLongLong() ) ) );
    ui->qLabel_bitrate->setText( Meta::prettyBitrate( tags.value( Meta::Field::BITRATE ).toInt() ) );
    ui->qLabel_samplerate->setText( unknownSafe( tags.value( Meta::Field::SAMPLERATE ).toInt() ) );
    ui->qLabel_size->setText( Meta::prettyFilesize( tags.value( Meta::Field::FILESIZE ).toLongLong() ) );
    ui->qLabel_format->setText( unknownSafe( tags.value( Meta::Field::TYPE ).toString() ) );

    ui->qSpinBox_score->setValue( tags.value( Meta::Field::SCORE ).toInt() );
    ui->ratingWidget->setRating( tags.value( Meta::Field::RATING ).toInt() );
    ui->ratingWidget->setMaxRating( 10 );
    ui->qLabel_playcount->setText( unknownSafe( tags.value( Meta::Field::PLAYCOUNT ).toInt() ) );

    QDate firstPlayed = tags.value( Meta::Field::FIRST_PLAYED ).toDate();
    ui->qLabel_firstPlayed->setText( firstPlayed.isValid() ?
                                     KGlobal::locale()->formatDate( firstPlayed, KLocale::ShortDate ) :
                                     i18nc( "When this track first played", "Never") );

    QDate lastPlayed = tags.value( Meta::Field::LAST_PLAYED ).toDate();
    ui->qLabel_lastPlayed->setText( lastPlayed.isValid() ?
                                    KGlobal::locale()->formatDate( lastPlayed, KLocale::ShortDate ) :
                                    i18nc( "When this track was last played", "Never") );

    ui->qLabel_collection->setText( tags.contains( Meta::Field::COLLECTION ) ?
                                    tags.value( Meta::Field::COLLECTION ).toString() :
                                    i18nc( "The collection this track is part of", "None") );

    // special handling - we want to hide this if empty
    if( tags.contains( Meta::Field::NOTE ) )
    {
        ui->noteLabel->show();
        ui->qLabel_note->setText( tags.value( Meta::Field::NOTE ).toString() );
        ui->qLabel_note->show();
    }
    else
    {
        ui->noteLabel->hide();
        ui->qLabel_note->hide();
    }

    ui->kRichTextEdit_lyrics->setTextOrHtml( tags.value( Meta::Field::LYRICS ).toString() );

    m_labelModel->setLabels( tags.value( Meta::Field::LABELS ).toStringList() );
    ui->labelsList->update();

    updateCover();

    setControlsAccessability();

    // If it's a local file, write the directory to m_path, else disable the "open in konqui" button
    QString urlString = tags.value( Meta::Field::URL ).toString();
    KUrl url( urlString );
    //pathOrUrl will give localpath or proper url for remote.
    ui->kLineEdit_location->setText( url.pathOrUrl() );
    if( url.isLocalFile() )
    {
        ui->locationLabel->show();
        ui->kLineEdit_location->show();
        QFileInfo fi( urlString );
        m_path = fi.isDir() ? urlString : url.directory( KUrl::AppendTrailingSlash );
        ui->pushButton_open->setEnabled( true );
    }
    else
    {
        m_path.clear();
        ui->pushButton_open->setEnabled( false );
    }

    m_changed = oldChanged;
    ui->pushButton_ok->setEnabled( m_changed );
}

void
TagDialog::setTagsToUi()
{
    if( m_perTrack )
        setTagsToUi( m_storedTags.value( m_currentTrack ) );
    else
        setTagsToUi( getTagsFromMultipleTracks() );
}


QVariantMap
TagDialog::getTagsFromUi( const QVariantMap &tags ) const
{
    QVariantMap map;

    if( ui->kLineEdit_title->text() != tags.value( Meta::Field::TITLE ).toString() )
        map.insert( Meta::Field::TITLE, ui->kLineEdit_title->text() );
    if( ui->kComboBox_artist->currentText() != tags.value( Meta::Field::ARTIST ).toString() )
        map.insert( Meta::Field::ARTIST, ui->kComboBox_artist->currentText() );
    if( ui->kComboBox_album->currentText() != tags.value( Meta::Field::ALBUM ).toString() )
        map.insert( Meta::Field::ALBUM, ui->kComboBox_album->currentText() );
    if( ui->kComboBox_albumArtist->currentText() != tags.value( Meta::Field::ALBUMARTIST ).toString() )
        map.insert( Meta::Field::ALBUMARTIST, ui->kComboBox_albumArtist->currentText() );
    if( ui->kComboBox_composer->currentText() != tags.value( Meta::Field::COMPOSER ).toString() )
        map.insert( Meta::Field::COMPOSER, ui->kComboBox_composer->currentText() );
    if( ui->qPlainTextEdit_comment->toPlainText() != tags.value( Meta::Field::COMMENT ).toString() )
        map.insert( Meta::Field::COMMENT, ui->qPlainTextEdit_comment->toPlainText() );
    if( ui->kComboBox_genre->currentText() != tags.value( Meta::Field::GENRE ).toString() )
        map.insert( Meta::Field::GENRE, ui->kComboBox_genre->currentText() );
    if( ui->qSpinBox_track->value() != tags.value( Meta::Field::TRACKNUMBER ).toInt() )
        map.insert( Meta::Field::TRACKNUMBER, ui->qSpinBox_track->value() );
    if( ui->qSpinBox_discNumber->value() != tags.value( Meta::Field::DISCNUMBER ).toInt() )
        map.insert( Meta::Field::DISCNUMBER, ui->qSpinBox_discNumber->value() );
    if( ui->kLineEdit_Bpm->text().toDouble() != tags.value( Meta::Field::BPM ).toReal() )
        map.insert( Meta::Field::BPM, ui->kLineEdit_Bpm->text() );
    if( ui->qSpinBox_year->value() != tags.value( Meta::Field::YEAR ).toInt() )
        map.insert( Meta::Field::YEAR, ui->qSpinBox_year->value() );

    if( ui->qSpinBox_score->value() != tags.value( Meta::Field::SCORE ).toInt() )
        map.insert( Meta::Field::SCORE, ui->qSpinBox_score->value() );

    if( ui->ratingWidget->rating() != tags.value( Meta::Field::RATING ).toUInt() )
        map.insert( Meta::Field::RATING, ui->ratingWidget->rating() );

    if( !m_tracks.count() || m_perTrack )
    { //ignore these on MultipleTracksMode
        if ( ui->kRichTextEdit_lyrics->textOrHtml() != tags.value( Meta::Field::LYRICS ).toString() )
            map.insert( Meta::Field::LYRICS, ui->kRichTextEdit_lyrics->textOrHtml() );
    }

    QSet<QString> uiLabels = m_labelModel->labels().toSet();
    QSet<QString> oldLabels = tags.value( Meta::Field::LABELS ).toStringList().toSet();
    if( uiLabels != oldLabels )
        map.insert( Meta::Field::LABELS, QVariant( uiLabels.toList() ) );

    return map;
}

QVariantMap
TagDialog::getTagsFromTrack( const Meta::TrackPtr &track ) const
{
    QVariantMap map;
    if( !track )
        return map;

    // get the shared pointers now to ensure that they don't get freed
    Meta::AlbumPtr album = track->album();
    Meta::ArtistPtr artist = track->artist();
    Meta::GenrePtr genre = track->genre();
    Meta::ComposerPtr composer = track->composer();
    Meta::YearPtr year = track->year();

    if( !track->name().isEmpty() )
        map.insert( Meta::Field::TITLE, track->name() );
    if( artist && !artist->name().isEmpty() )
        map.insert( Meta::Field::ARTIST, artist->name() );
    if( album && !track->album()->name().isEmpty() )
    {
        map.insert( Meta::Field::ALBUM, album->name() );
        if( album->hasAlbumArtist() && !album->albumArtist()->name().isEmpty() )
            map.insert( Meta::Field::ALBUMARTIST, album->albumArtist()->name() );
    }
    if( composer && !composer->name().isEmpty() )
        map.insert( Meta::Field::COMPOSER, composer->name() );
    if( !track->comment().isEmpty() )
        map.insert( Meta::Field::COMMENT, track->comment() );
    if( genre && !genre->name().isEmpty() )
        map.insert( Meta::Field::GENRE, genre->name() );
    if( track->trackNumber() )
        map.insert( Meta::Field::TRACKNUMBER, track->trackNumber() );
    if( track->discNumber() )
        map.insert( Meta::Field::DISCNUMBER, track->discNumber() );
    if( year && year->year() )
        map.insert( Meta::Field::YEAR, year->year() );
    if( track->bpm() > 0.0)
        map.insert( Meta::Field::BPM, track->bpm() );
    if( track->length() )
        map.insert( Meta::Field::LENGTH, track->length() );
    if( track->bitrate() )
        map.insert( Meta::Field::BITRATE, track->bitrate() );
    if( track->sampleRate() )
        map.insert( Meta::Field::SAMPLERATE, track->sampleRate() );
    if( track->filesize() )
        map.insert( Meta::Field::FILESIZE, track->filesize() );

    Meta::ConstStatisticsPtr statistics = track->statistics();
    map.insert( Meta::Field::SCORE, statistics->score() );
    map.insert( Meta::Field::RATING, statistics->rating() );
    map.insert( Meta::Field::PLAYCOUNT, statistics->playCount() );
    map.insert( Meta::Field::FIRST_PLAYED, statistics->firstPlayed() );
    map.insert( Meta::Field::LAST_PLAYED, statistics->lastPlayed() );
    map.insert( Meta::Field::URL, track->prettyUrl() );

    map.insert( Meta::Field::TYPE, track->type() );

    if( track->inCollection() )
        map.insert( Meta::Field::COLLECTION, track->collection()->prettyName() );

    if( !track->notPlayableReason().isEmpty() )
        map.insert( Meta::Field::NOTE, i18n( "The track is not playable. %1",
                                             track->notPlayableReason() ) );

    QStringList labelNames;
    foreach( const Meta::LabelPtr &label, track->labels() )
    {
        labelNames << label->name();
    }
    map.insert( Meta::Field::LABELS, labelNames );

    map.insert( Meta::Field::LYRICS, track->cachedLyrics() );

    return map;
}

QVariantMap
TagDialog::getTagsFromMultipleTracks() const
{
    QVariantMap map;
    if( m_tracks.isEmpty() )
        return map;

    //Check which fields are the same for all selected tracks
    QSet<QString> mismatchingTags;

    Meta::TrackPtr first = m_tracks.first();
    map = getTagsFromTrack( first );

    QString directory = first->playableUrl().directory();
    int scoreCount = 0;
    double scoreSum = map.value( Meta::Field::SCORE ).toDouble();
    if( map.value( Meta::Field::SCORE ).toDouble() )
        scoreCount++;

    int ratingCount = 0;
    int ratingSum = map.value( Meta::Field::RATING ).toInt();
    if( map.value( Meta::Field::RATING ).toInt() )
        ratingCount++;

    QDateTime firstPlayed = first->statistics()->firstPlayed();
    QDateTime lastPlayed = first->statistics()->lastPlayed();

    qint64 length = first->length();
    qint64 size = first->filesize();
    QStringList validLabels = map.value( Meta::Field::LABELS ).toStringList();

    for( int i = 1; i < m_tracks.count(); i++ )
    {
        Meta::TrackPtr track = m_tracks[i];
        QVariantMap tags = m_storedTags.value( track );

        // -- figure out which tags do not match.

        // - occur not in every file
        mismatchingTags |= map.keys().toSet() - tags.keys().toSet();
        mismatchingTags |= tags.keys().toSet() - map.keys().toSet();

        // - not the same in every file
        foreach( const QString &key, (map.keys().toSet() & tags.keys().toSet()) )
        {
            if( map.value( key ) != tags.value( key ) )
                mismatchingTags.insert( key );
        }

        // -- special handling for values

        // go up in the directories until we find a common one
        QString newDirectory = track->playableUrl().directory();
        while( newDirectory != directory )
        {
            if( newDirectory.length() > directory.length() )
            {
                QDir up( newDirectory ); up.cdUp();
                QString d = up.path();
                if( d == newDirectory ) // nothing changed
                {
                    directory.clear();
                    break;
                }
                newDirectory = d;
            }
            else
            {
                QDir up( directory ); up.cdUp();
                QString d = up.path();
                if( d == directory ) // nothing changed
                {
                    directory.clear();
                    break;
                }
                directory = d;
            }
        }
        if( !track->playableUrl().isLocalFile() )
            directory.clear();

        // score and rating (unrated if rating == 0)
        scoreSum += tags.value( Meta::Field::SCORE ).toDouble();
        if( tags.value( Meta::Field::SCORE ).toDouble() )
            scoreCount++;

        ratingSum += tags.value( Meta::Field::RATING ).toInt();
        if( tags.value( Meta::Field::RATING ).toInt() )
            ratingCount++;

        Meta::StatisticsPtr statistics = track->statistics();
        if( statistics->firstPlayed().isValid() &&
            (!firstPlayed.isValid() || statistics->firstPlayed() < firstPlayed) )
            firstPlayed = statistics->firstPlayed();

        if( statistics->lastPlayed().isValid() &&
            (!lastPlayed.isValid() || statistics->lastPlayed() > lastPlayed) )
            lastPlayed = statistics->lastPlayed();

        length += track->length();
        size += track->filesize();

        // Only show labels present in all of the tracks
        QStringList labels = tags.value( Meta::Field::LABELS ).toStringList();
        for ( int x = 0; x < validLabels.count(); x++ )
        {
            if ( !labels.contains( validLabels.at( x ) ) )
                validLabels.removeAt( x );
        }

    }

    foreach( const QString &key, mismatchingTags )
        map.remove( key );

    map.insert( Meta::Field::URL, directory );
    if( scoreCount > 0 )
        map.insert( Meta::Field::SCORE, scoreSum / scoreCount );
    if( ratingCount > 0 )
        // the extra fuzz is for emulating rounding to nearest integer
        map.insert( Meta::Field::RATING, ( ratingSum + ratingCount / 2 ) / ratingCount );

    map.insert( Meta::Field::FIRST_PLAYED, firstPlayed );
    map.insert( Meta::Field::LAST_PLAYED, lastPlayed );

    map.insert( Meta::Field::LENGTH, length );
    map.insert( Meta::Field::FILESIZE, size );

    map.insert( Meta::Field::LABELS, validLabels );

    return map;
}

void
TagDialog::setTagsToTrack( const Meta::TrackPtr &track, const QVariantMap &tags )
{
    foreach( const QString &key, tags.keys() )
    {
        m_storedTags[ track ].insert( key, tags.value( key ) );
    }
}

void
TagDialog::setTagsToMultipleTracks( QVariantMap tags )
{
    tags.remove( Meta::Field::LABELS );

    foreach( const Meta::TrackPtr &track, m_tracks )
    {
        setTagsToTrack( track, tags );
    }
}

void
TagDialog::setTagsToTrack()
{
    QVariantMap oldTags;
    if( m_perTrack )
        oldTags = m_storedTags.value( m_currentTrack );
    else
        oldTags = getTagsFromMultipleTracks();
    QVariantMap newTags = getTagsFromUi( oldTags );

    if( !newTags.isEmpty() )
    {
        m_changed = true;
        if( m_perTrack )
            setTagsToTrack( m_currentTrack, newTags );
        else
        {
            setTagsToMultipleTracks( newTags );

            // -- special handling for labels
            if( newTags.contains( Meta::Field::LABELS ) )
            {
                // determine the differences
                QSet<QString> oldLabelsSet = oldTags.value( Meta::Field::LABELS ).toStringList().toSet();
                QSet<QString> newLabelsSet = newTags.value( Meta::Field::LABELS ).toStringList().toSet();

                QSet<QString> labelsToRemove = oldLabelsSet - newLabelsSet;
                QSet<QString> labelsToAdd = newLabelsSet - oldLabelsSet;

                // apply the differences for each track
                foreach( const Meta::TrackPtr &track, m_tracks )
                {
                    QSet<QString> labelsSet = m_storedTags[track].value( Meta::Field::LABELS ).toStringList().toSet();
                    labelsSet += labelsToAdd;
                    labelsSet -= labelsToRemove;

                    m_storedTags[ track ].insert( Meta::Field::LABELS, QVariant( labelsSet.toList() ) );
                }
            }
        }
    }
}


void
TagDialog::setPerTrack( bool isEnabled )
{
    debug() << "setPerTrack" << m_tracks.count() << isEnabled;
    if( m_tracks.count() < 2 )
        isEnabled = true;

    /* force an update so that we can use this function in the initialization
    if( m_perTrack == isEnabled )
        return;
    */

    m_perTrack = isEnabled;

    setControlsAccessability();
    updateButtons();
}


void
TagDialog::updateButtons()
{
    ui->pushButton_ok->setEnabled( m_changed );

    ui->checkBox_perTrack->setVisible( m_tracks.count() > 1 );
    ui->pushButton_previous->setVisible( m_tracks.count() > 1 );
    ui->pushButton_next->setVisible( m_tracks.count() > 1 );

    ui->checkBox_perTrack->setChecked( m_perTrack );
    ui->pushButton_previous->setEnabled( m_perTrack && m_currentTrackNum > 0 );
    ui->pushButton_next->setEnabled( m_perTrack && m_currentTrackNum < m_tracks.count()-1 );
}

void
TagDialog::updateCover()
{
    DEBUG_BLOCK

    if( !m_currentTrack )
        return;

    // -- get the album
    Meta::AlbumPtr album = m_currentTrack->album();
    if( !m_perTrack )
    {
        foreach( Meta::TrackPtr track, m_tracks )
        {
            if( track->album() != album )
                album = 0;
        }
    }

    // -- set the ui
    const int s = 100; // Image preview size
    ui->pixmap_cover->setMinimumSize( s, s );
    ui->pixmap_cover->setMaximumSize( s, s );

    if( !album )
    {
        ui->pixmap_cover->setVisible( false );
    }
    else
    {
        ui->pixmap_cover->setVisible( true );
        ui->pixmap_cover->setPixmap( The::svgHandler()->imageWithBorder( album, s ) );
        QString artist = m_currentTrack->artist() ? m_currentTrack->artist()->name() : QString();
        ui->pixmap_cover->setInformation( artist, album->name() );
    }
}


void
TagDialog::setControlsAccessability()
{
    bool editable = m_currentTrack ? bool( m_currentTrack->editor() ) : true;

    ui->kTabWidget->setTabEnabled( ui->kTabWidget->indexOf(ui->lyricsTab),
                                   m_perTrack );

    ui->kLineEdit_title->setEnabled( m_perTrack && editable );
    ui->kLineEdit_title->setClearButtonShown( m_perTrack && editable );

#define enableOrDisable( X ) \
    ui->X->setEnabled( editable ); \
    qobject_cast<KLineEdit*>(ui->X->lineEdit())->setClearButtonShown( editable )

    enableOrDisable( kComboBox_artist );
    enableOrDisable( kComboBox_albumArtist );
    enableOrDisable( kComboBox_composer );
    enableOrDisable( kComboBox_album );
    enableOrDisable( kComboBox_genre );

#undef enableOrDisable

    ui->qSpinBox_track->setEnabled( m_perTrack && editable );
    ui->qSpinBox_discNumber->setEnabled( editable );
    ui->qSpinBox_year->setEnabled( editable );
    ui->kLineEdit_Bpm->setEnabled( editable );
    ui->kLineEdit_Bpm->setClearButtonShown( editable );

    ui->qPlainTextEdit_comment->setEnabled( editable );
    ui->pushButton_guessTags->setEnabled( m_perTrack && editable );
    ui->pushButton_musicbrainz->setEnabled( editable );
}

void
TagDialog::saveLabels( Meta::TrackPtr track, const QStringList &labels )
{
    if( !track )
        return;

    QHash<QString, Meta::LabelPtr> labelMap;
    foreach( const Meta::LabelPtr &label, track->labels() )
    {
        labelMap.insert( label->name(), label );
    }

    // labels to remove
    foreach( const QString &label, labelMap.keys().toSet() - labels.toSet() )
    {
        track->removeLabel( labelMap.value( label ) );
    }

    // labels to add
    foreach( const QString &label, labels.toSet() - labelMap.keys().toSet() )
    {
        track->addLabel( label );
    }
}


void
TagDialog::saveTags()
{
    setTagsToTrack();

    foreach( Meta::TrackPtr track, m_tracks )
    {
        QVariantMap data = m_storedTags[ track ];
        //there is really no need to write to the file if only info m_stored in the db has changed
        if( !data.isEmpty() )
        {
            debug() << "File info changed....";

            if( data.contains( Meta::Field::SCORE ) )
                track->statistics()->setScore( data.value( Meta::Field::SCORE ).toInt() );
            if( data.contains( Meta::Field::RATING ) )
                track->statistics()->setRating( data.value( Meta::Field::RATING ).toInt() );
            if( data.contains( Meta::Field::LYRICS ) )
            {
                track->setCachedLyrics( data.value( Meta::Field::LYRICS ).toString() );
                emit lyricsChanged( track->uidUrl() );
            }

            saveLabels( track, data.value( Meta::Field::LABELS ).toStringList() );

            Meta::TrackEditorPtr ec = track->editor();
            if( !ec )
            {
                debug() << "Track" << track->prettyUrl() << "does not have Meta::TrackEditor. Skiping.";
                continue;
            }

            ec->beginUpdate();
            if( data.contains( Meta::Field::TITLE ) )
                ec->setTitle( data.value( Meta::Field::TITLE ).toString() );
            if( data.contains( Meta::Field::COMMENT ) )
                ec->setComment( data.value( Meta::Field::COMMENT ).toString() );
            if( data.contains( Meta::Field::ARTIST ) )
                ec->setArtist( data.value( Meta::Field::ARTIST ).toString() );
            if( data.contains( Meta::Field::ALBUM ) )
                ec->setAlbum( data.value( Meta::Field::ALBUM ).toString() );
            if( data.contains( Meta::Field::GENRE ) )
                ec->setGenre( data.value( Meta::Field::GENRE ).toString() );
            if( data.contains( Meta::Field::COMPOSER ) )
                ec->setComposer( data.value( Meta::Field::COMPOSER ).toString() );
            if( data.contains( Meta::Field::YEAR ) )
                ec->setYear( data.value( Meta::Field::YEAR ).toInt() );
            if( data.contains( Meta::Field::TRACKNUMBER ) )
                ec->setTrackNumber( data.value( Meta::Field::TRACKNUMBER ).toInt() );
            if( data.contains( Meta::Field::DISCNUMBER ) )
                ec->setDiscNumber( data.value( Meta::Field::DISCNUMBER ).toInt() );
            if( data.contains( Meta::Field::BPM ) )
                ec->setBpm( data.value( Meta::Field::BPM ).toDouble() );
            if( data.contains( Meta::Field::ALBUMARTIST ) )
                ec->setAlbumArtist( data.value( Meta::Field::ALBUMARTIST ).toString() );

            ec->endUpdate();
            // note: the track should by itself emit a collectionUpdated signal if needed
        }
    }
}

void
TagDialog::selectOrInsertText( const QString &text, QComboBox *comboBox )
{
    int index = comboBox->findText( text );
    if( index == -1 )
    {
        comboBox->insertItem( 0, text );    //insert at the beginning
        comboBox->setCurrentIndex( 0 );
    }
    else
    {
        comboBox->setCurrentIndex( index );
    }
}

void
TagDialog::musicbrainzTagger()
{
    DEBUG_BLOCK

    MusicBrainzTagger *dialog = new MusicBrainzTagger( m_tracks, this );
    dialog->setWindowTitle( i18n( "MusicBrainz Tagger" ) );
    connect( dialog, SIGNAL(sendResult(QMap<Meta::TrackPtr,QVariantMap>)),
             this, SLOT(musicbrainzTaggerResult(QMap<Meta::TrackPtr,QVariantMap>)) );
    dialog->show();
}

void
TagDialog::musicbrainzTaggerResult( const QMap<Meta::TrackPtr, QVariantMap> result )
{
    if( result.isEmpty() )
        return;

    foreach( Meta::TrackPtr track, result.keys() )
    {
        setTagsToTrack( track, result.value( track ) );
    }
    m_changed = true;

    if( m_perTrack )
        setTagsToUi( m_storedTags.value( m_currentTrack ) );
    else
        setTagsToUi( getTagsFromMultipleTracks() );
}

#include "TagDialog.moc"

