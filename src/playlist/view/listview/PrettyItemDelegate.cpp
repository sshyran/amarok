/***************************************************************************
 * copyright            : (C) 2007 Ian Monroe <ian@monroe.nu>
 *                      : (C) 2008 Nikolaj Hald Nielsen <nhnFreespirit@gmail.com>
 *                      : (C) 2008 Soren Harward <stharward@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **************************************************************************/

#define DEBUG_PREFIX "Playlist::PrettyItemDelegate"

#include "PrettyItemDelegate.h"

#include "App.h"
#include "Debug.h"
#include "SvgHandler.h"
#include "SvgTinter.h"
#include "meta/Meta.h"
#include "meta/SourceInfoCapability.h"
#include "playlist/GroupingProxy.h"
#include "playlist/PlaylistModel.h"
#include "LayoutManager.h"

#include <QFontMetricsF>
#include <QPainter>

using namespace Playlist;

const qreal PrettyItemDelegate::ALBUM_WIDTH = 50.0;
const qreal PrettyItemDelegate::SINGLE_TRACK_ALBUM_WIDTH = 40.0;
const qreal PrettyItemDelegate::MARGIN = 2.0;
const qreal PrettyItemDelegate::MARGINH = 6.0;
const qreal PrettyItemDelegate::MARGINBODY = 1.0;
const qreal PrettyItemDelegate::PADDING = 1.0;

Playlist::PrettyItemDelegate::PrettyItemDelegate( QObject* parent )
        : QStyledItemDelegate( parent )
{
    DEBUG_BLOCK

    LayoutManager::instance();

}

PrettyItemDelegate::~PrettyItemDelegate() { }

QSize
PrettyItemDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    int height = 0;

    QFontMetricsF nfm( option.font );
    QFont boldfont( option.font );
    boldfont.setBold( true );
    QFontMetricsF bfm( boldfont );

    PlaylistLayout layout = LayoutManager::instance()->activeLayout();

    int groupMode = index.data( GroupRole ).toInt();
    int rowCount = 1;
    switch ( groupMode )
    {
    case Head:
        rowCount = layout.head().rows() + layout.body().rows();
        break;
    case Body:
        rowCount = layout.body().rows();
        break;
    case Tail:
        rowCount = layout.body().rows();
        break;
    case None:
    default:
        rowCount = layout.single().rows();
        break;
    }

    height = MARGIN * 2 + rowCount * bfm.height() + ( rowCount - 1 ) * PADDING;
    return QSize( 120, height );

    



}

void
PrettyItemDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    PlaylistLayout layout = LayoutManager::instance()->activeLayout();
    
    painter->save();
    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option, painter );
    painter->translate( option.rect.topLeft() );

    painter->drawPixmap( 0, 0, The::svgHandler()->renderSvgWithDividers( "track", ( int )option.rect.width(), ( int )option.rect.height(), "track" ) );

    if ( option.state & QStyle::State_Selected )
        painter->setPen( App::instance()->palette().highlightedText().color() );
    else
        painter->setPen( App::instance()->palette().text().color() );

    // call paint method based on type
    int groupMode = index.data( GroupRole ).toInt();
    if ( groupMode == None )
        paintItem( layout.single(), painter, option, index );
    else if ( groupMode == Head ) {

        //we need to split up the options for the actual header and the included first track

        QFont boldfont( option.font );
        boldfont.setBold( true );
        QFontMetricsF bfm( boldfont );

        QStyleOptionViewItem headOption( option );
        QStyleOptionViewItem trackOption( option );

        int headRows = layout.head().rows();
        int headHeight = MARGIN * 2 + headRows * bfm.height() + ( headRows - 1 ) * PADDING;
        headOption.rect = QRect( 0, 0, option.rect.width(), headHeight );

        int trackRows = layout.body().rows();
        int trackHeight = MARGIN * 2 + trackRows * bfm.height() + ( trackRows - 1 ) * PADDING;
        trackOption.rect = QRect( 0, headHeight, option.rect.width(), trackHeight );

        paintItem( layout.head(), painter, headOption, index );
        painter->translate( 0, headHeight - 1 );
        paintItem( layout.body(), painter, trackOption, index );
        
    } else if ( groupMode == Body )
        paintItem( layout.body(), painter, option, index );
    else if ( groupMode == Tail )
        paintItem( layout.body(), painter, option, index );
    else
        QStyledItemDelegate::paint( painter, option, index );

    painter->restore();
}

bool
PrettyItemDelegate::insideItemHeader( const QPoint& pt, const QRect& rect )
{
    QRect headerBounds = rect.adjusted(( int )MARGINH,
                                       ( int )MARGIN,
                                       ( int )( -MARGINH ),
                                       0 );
    headerBounds.setHeight( static_cast<int>( 2 * MARGIN + SINGLE_TRACK_ALBUM_WIDTH ) );
    return headerBounds.contains( pt );
}

void
PrettyItemDelegate::paintSingleTrack( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QRectF trackRect( option.rect );
    QFontMetricsF nfm( option.font );

    Meta::TrackPtr track = index.data( TrackRole ).value<Meta::TrackPtr>();

    //paint cover
    QPixmap albumPixmap;
    if ( track->album() )
        albumPixmap = track->album()->imageWithBorder( int( SINGLE_TRACK_ALBUM_WIDTH ), 3 );

    //offset cover if non square
    const QPointF offset = centerImage( albumPixmap, imageLocation() );
    const QRectF imageRect = imageRectify( offset );

    painter->drawPixmap( imageRect, albumPixmap, QRectF( albumPixmap.rect() ) );

    //check if there is a emblem to display
    //does this track have the SourceInfoCapability?
    Meta::SourceInfoCapability *sic = track->as<Meta::SourceInfoCapability>();
    if ( sic )
    {
        //is the source defined
        QString source = sic->sourceName();
        if ( !source.isEmpty() )
            painter->drawPixmap( QRectF( imageLocation().x(), imageLocation().y() , 16, 16 ), sic->emblem(), QRectF( 0, 0 , 16, 16 ) );

        delete sic;
    }

    // Draw queue display indicator
    if( index.data( StateRole ).toInt() & Item::Queued )
    {
        const int queuePosition = index.data( QueuePositionRole ).toInt();
        const int w = 16, h = 16;
        const int x = imageRect.x() + imageRect.width() - w;
        const int y = imageRect.y();
        const QRect rect( x, y, w, h );
        painter->drawPixmap( x, y, The::svgHandler()->renderSvg( "active_overlay", w, h, "active_overlay" ) ); // TODO: actual queue overlay
        painter->drawText( rect, Qt::AlignCenter, QString::number( queuePosition ) );
    }

    // Set up the text areas
    qreal leftside = MARGINH + SINGLE_TRACK_ALBUM_WIDTH + 3 * PADDING;
    qreal boxheight = ( trackRect.height() - 3 * MARGIN ) / 2.0;
    qreal textwidth = trackRect.width() - leftside - MARGINH;

    QPointF topLeft( leftside, MARGIN );
    QRectF topLine( leftside, MARGIN, textwidth, boxheight ); // box that surrounds the entire top line of text
    QRectF bottomLine( leftside, MARGIN + boxheight + MARGIN, textwidth, boxheight ); // box that surrounds the entire bottom line of text

    // top right: track time
    QString timeString;
    if ( track->length() > 3600 )
        timeString = QTime().addSecs( track->length() ).toString( "h:mm:ss" );
    else
        timeString = QTime().addSecs( track->length() ).toString( "m:ss" );
    QSizeF timeStringSize( nfm.size( Qt::TextSingleLine, timeString ) );
    timeStringSize.setHeight( boxheight );
    QPointF textLoc( trackRect.width() - MARGINH - timeStringSize.width() - PADDING, MARGIN );
    QRectF timeTextBox( textLoc, timeStringSize );
    timeTextBox = timeTextBox.adjusted( 0, PADDING, 0, -PADDING );

    // top left: track name
    QRectF titleTextBox = topLine.adjusted( PADDING, PADDING, -2 * PADDING - timeStringSize.width(), -PADDING );
    QString titleString = nfm.elidedText( track->prettyName(), Qt::ElideRight, ( int )titleTextBox.width() );

    QString rawArtistString = ( track->album() == Meta::ArtistPtr() ) ? QString() : track->artist()->prettyName();
    QString rawAlbumString = ( track->album() == Meta::AlbumPtr() ) ? QString() : track->album()->prettyName();

    // figure out widths for artist and album boxes
    textwidth -= static_cast<qreal>( nfm.averageCharWidth() * 2.0 ); // make room for a space between artist & album
    qreal albumNameWidth;
    qreal artistNameWidth = nfm.size( Qt::TextSingleLine, rawArtistString ).width();
    if ( artistNameWidth <= textwidth / 2.0 )
        albumNameWidth = textwidth - artistNameWidth;
    else
    {
        albumNameWidth = nfm.size( Qt::TextSingleLine, rawAlbumString ).width();
        if ( albumNameWidth <= textwidth / 2.0 )
            artistNameWidth = textwidth - albumNameWidth;
        else
        {
            artistNameWidth = textwidth / 2.0;
            albumNameWidth = textwidth / 2.0;
        }
    }

    // bottom left: artist name
    QPointF artistLoc( bottomLine.topLeft() );
    artistLoc.rx() += PADDING;
    QRectF artistTextBox( artistLoc, QSizeF( artistNameWidth, boxheight ) );
    artistTextBox.adjust( 0, PADDING, 0, -PADDING );
    QString artistString = nfm.elidedText( rawArtistString, Qt::ElideRight, artistNameWidth );

    // bottom right: album name
    QPointF albumLoc( bottomLine.topRight() );
    albumLoc.rx() -= ( albumNameWidth + PADDING );
    QRectF albumTextBox( albumLoc, QSizeF( albumNameWidth, boxheight ) );
    albumTextBox.adjust( 0, PADDING, 0, -PADDING );
    QString albumString = nfm.elidedText( rawAlbumString, Qt::ElideRight, albumNameWidth );

    // draw the "current track" highlight underneath the text
    //if( index.data( ActiveTrackRole ).toBool() )
    //    paintActiveOverlay( painter, topLine.x(), topLine.y(), bottomLine.width(), bottomLine.height() );

    // render text in here
    painter->drawText( titleTextBox, Qt::AlignLeft | Qt::AlignVCenter, titleString );
    painter->drawText( timeTextBox, Qt::AlignRight | Qt::AlignVCenter, timeString );
    painter->drawText( artistTextBox, Qt::AlignLeft | Qt::AlignVCenter, artistString );
    painter->drawText( albumTextBox, Qt::AlignRight | Qt::AlignVCenter, albumString );

    //set selection marker if needed
    /*if (option.state & QStyle::State_Selected) {
        painter->drawPixmap( (int)topLine.x(),(int)topLine.y(),
                             The::svgHandler()->renderSvg(
                                        "selection",
                                        (int)bottomLine.width(), (int)bottomLine.height(),
                                        "selection"
                                      )
                           );
    }*/
}

void
PrettyItemDelegate::paintHead( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QRectF trackRect( option.rect );
    QFontMetricsF nfm( option.font );
    QFont boldfont( option.font );
    boldfont.setBold( true );
    QFontMetricsF bfm( boldfont );

    Meta::TrackPtr track = index.data( TrackRole ).value<Meta::TrackPtr>();

    //paint cover
    QPixmap albumPixmap;
    if ( track->album() )
        albumPixmap = track->album()->imageWithBorder( int( SINGLE_TRACK_ALBUM_WIDTH ), 3 );

    //offset cover if non square
    const QPointF offset = centerImage( albumPixmap, imageLocation() );
    const QRectF imageRect = imageRectify( offset );

    painter->drawPixmap( imageRect, albumPixmap, QRectF( albumPixmap.rect() ) );

    //check if there is a emblem to display
    //does this track have the SourceInfoCapability?
    Meta::SourceInfoCapability *sic = track->as<Meta::SourceInfoCapability>();
    if ( sic )
    {
        //is the source defined
        QString source = sic->sourceName();
        if ( !source.isEmpty() )
            painter->drawPixmap( QRectF( imageLocation().x(), imageLocation().y() , 16, 16 ), sic->emblem(), QRectF( 0, 0 , 16, 16 ) );

        delete sic;
    }

    qreal headheight = MARGIN + qMax( SINGLE_TRACK_ALBUM_WIDTH, bfm.height() * 2 + PADDING ) + PADDING;

    // Set up the text areas
    qreal leftside = MARGINH + SINGLE_TRACK_ALBUM_WIDTH + 3 * PADDING;
    qreal boxheight = ( headheight - PADDING ) / 2.0;
    qreal textwidth = trackRect.width() - leftside - MARGINH;

    QPointF topLeft( leftside, MARGIN );
    QRectF topLine( leftside, MARGIN, textwidth, boxheight ); // box that surrounds the entire top line of text
    QRectF bottomLine( leftside, MARGIN + boxheight + MARGIN, textwidth, boxheight ); // box that surrounds the entire bottom line of text

    painter->save(); // before changing to bold text

    QFont font;
    font.setBold( true );
    painter->setFont( font );

    // top line: album
    QRectF albumTextBox = topLine.adjusted( PADDING, PADDING, -PADDING, -PADDING );
    QString rawAlbumString = ( track->album() == Meta::AlbumPtr() ) ? QString() : track->album()->prettyName();
    QString albumString = bfm.elidedText( rawAlbumString, Qt::ElideRight, ( int )albumTextBox.width() );
    painter->drawText( albumTextBox, Qt::AlignCenter, albumString );

    // bottom line: artist
    QRectF artistTextBox = bottomLine.adjusted( PADDING, PADDING, -PADDING, -PADDING );
    QString rawArtistString = ( track->album() == Meta::ArtistPtr() ) ? QString() : track->artist()->prettyName();
    QString artistString = bfm.elidedText( rawArtistString, Qt::ElideRight, ( int )artistTextBox.width() );
    painter->drawText( artistTextBox, Qt::AlignCenter, artistString );

    painter->restore(); // change back from bold text

    // for track number, name, and time
    QRectF line( QPointF( MARGINH, headheight + MARGIN ), QPointF( trackRect.width() - MARGINH, trackRect.height() - MARGINBODY ) );

    // draw the "current track" highlight underneath the text
    //if( index.data( ActiveTrackRole ).toBool() )
    //    paintActiveOverlay( painter, line.x(), line.y(), line.width(), line.height() );

    // right: track time
    QString timeString;
    if ( track->length() > 3600 )
        timeString = QTime().addSecs( track->length() ).toString( "h:mm:ss" );
    else
        timeString = QTime().addSecs( track->length() ).toString( "m:ss" );
    QSizeF timeStringSize( nfm.size( Qt::TextSingleLine, timeString ) );
    timeStringSize.setHeight( line.height() );
    QPointF textLoc( trackRect.width() - MARGINH - timeStringSize.width() - PADDING, line.y() );
    QRectF timeTextBox( textLoc, timeStringSize );
    timeTextBox = timeTextBox.adjusted( 0, PADDING, 0, -PADDING );

    // left: track number and name
    QRectF textBox = line.adjusted( PADDING, PADDING, -2 * PADDING - timeStringSize.width(), -PADDING );

    QString trackString;
    QString trackName = track->prettyName();
    if ( track->trackNumber() > 0 )
    {
        QString trackNumber = QString::number( track->trackNumber() );
        trackString = nfm.elidedText( QString( trackNumber + " - " + trackName ), Qt::ElideRight, ( int )textBox.width() );
    }
    else
        trackString = nfm.elidedText( QString( trackName ), Qt::ElideRight, ( int )textBox.width() );

    // render text in here
    //setTextColor(index.data(ActiveTrackRole).toBool());
    painter->drawText( textBox, Qt::AlignLeft | Qt::AlignVCenter, trackString );
    painter->drawText( timeTextBox, Qt::AlignRight | Qt::AlignVCenter, timeString );
}

void
PrettyItemDelegate::paintBody( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QRectF trackRect( option.rect );
    QFontMetricsF nfm( option.font );

    QRectF line( MARGINH, MARGINBODY, trackRect.width() - ( 2*MARGINH ), trackRect.height() - ( 2*MARGINBODY ) );

    // draw the "current track" highlight underneath the text
    //if ( index.data( ActiveTrackRole ).toBool() )
    //    paintActiveOverlay( painter, line.x(), line.y(), line.width(), line.height() );

    // Draw queue display indicator
    if( index.data( StateRole ).toInt() & Item::Queued )
    {
        const int queuePosition = index.data( QueuePositionRole ).toInt();
        const int w = 16, h = 16;
        const int x = 1;
        const int y = 1;
        const QRect rect( x, y, w, h );
        painter->drawPixmap( x, y, The::svgHandler()->renderSvg( "active_overlay", w, h, "active_overlay" ) ); // TODO: actual queue overlay
        painter->drawText( rect, Qt::AlignCenter, QString::number( queuePosition ) );

        line.setLeft( line.left() + w ); // adjust the left edge of the text to account for the queue indicator
    }

    Meta::TrackPtr track = index.data( TrackRole ).value<Meta::TrackPtr>();

    // right: track time
    QString timeString;
    if ( track->length() > 3600 )
        timeString = QTime().addSecs( track->length() ).toString( "h:mm:ss" );
    else
        timeString = QTime().addSecs( track->length() ).toString( "m:ss" );
    QSizeF timeStringSize( nfm.size( Qt::TextSingleLine, timeString ) );
    timeStringSize.setHeight( line.height() );
    QPointF textLoc( trackRect.width() - MARGINH - timeStringSize.width() - PADDING, MARGIN );
    QRectF timeTextBox( textLoc, timeStringSize );
    timeTextBox = timeTextBox.adjusted( 0, PADDING, 0, -PADDING );

    // left: track number and name
    QRectF textBox = line.adjusted( PADDING, PADDING, -2 * PADDING - timeStringSize.width(), -PADDING );

    QString trackString;
    QString trackName = track->prettyName();
    if ( track->trackNumber() > 0 )
    {
        QString trackNumber = QString::number( track->trackNumber() );
        trackString = nfm.elidedText( QString( trackNumber + " - " + trackName ), Qt::ElideRight, ( int )textBox.width() );
    }
    else
        trackString = nfm.elidedText( QString( trackName ), Qt::ElideRight, ( int )textBox.width() );

    // render text in here
    //setTextColor(index.data(ActiveTrackRole).toBool());
    painter->drawText( textBox, Qt::AlignLeft | Qt::AlignVCenter, trackString );
    painter->drawText( timeTextBox, Qt::AlignRight | Qt::AlignVCenter, timeString );
}

void
PrettyItemDelegate::paintTail( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QRectF trackRect( option.rect );
    QFontMetricsF nfm( option.font );

    QRectF line( MARGINH, MARGINBODY, trackRect.width() - ( 2*MARGINH ), trackRect.height() - MARGINBODY - MARGIN );

    // draw the "current track" highlight underneath the text
    //if( index.data( ActiveTrackRole ).toBool() )
    //    paintActiveOverlay( painter, line.x(), line.y(), line.width(), line.height() );
    

    Meta::TrackPtr track = index.data( TrackRole ).value<Meta::TrackPtr>();

    // right: track time
    QString timeString;
    if ( track->length() > 3600 )
        timeString = QTime().addSecs( track->length() ).toString( "h:mm:ss" );
    else
        timeString = QTime().addSecs( track->length() ).toString( "m:ss" );
    QSizeF timeStringSize( nfm.size( Qt::TextSingleLine, timeString ) );
    timeStringSize.setHeight( line.height() );
    QPointF textLoc( trackRect.width() - MARGINH - timeStringSize.width() - PADDING, MARGIN );
    QRectF timeTextBox( textLoc, timeStringSize );
    timeTextBox = timeTextBox.adjusted( 0, PADDING, 0, -PADDING );

    // left: track number and name
    QRectF textBox = line.adjusted( PADDING, PADDING, -2 * PADDING - timeStringSize.width(), -PADDING );

    QString trackString;
    QString trackName = track->prettyName();
    if ( track->trackNumber() > 0 )
    {
        QString trackNumber = QString::number( track->trackNumber() );
        trackString = nfm.elidedText( QString( trackNumber + " - " + trackName ), Qt::ElideRight, ( int )textBox.width() );
    }
    else
        trackString = nfm.elidedText( QString( trackName ), Qt::ElideRight, ( int )textBox.width() );

    // render text in here
    //setTextColor(index.data(ActiveTrackRole).toBool());
    painter->drawText( textBox, Qt::AlignLeft | Qt::AlignVCenter, trackString );
    painter->drawText( timeTextBox, Qt::AlignRight | Qt::AlignVCenter, timeString );
}

QPointF
PrettyItemDelegate::centerImage( const QPixmap& pixmap, const QRectF& rect ) const
{
    qreal pixmapRatio = ( qreal )pixmap.width() / ( qreal )pixmap.height();

    qreal moveByX = 0.0;
    qreal moveByY = 0.0;

    if ( pixmapRatio >= 1 )
        moveByY = ( rect.height() - ( rect.width() / pixmapRatio ) ) / 2.0;
    else
        moveByX = ( rect.width() - ( rect.height() * pixmapRatio ) ) / 2.0;

    return QPointF( moveByX, moveByY );
}

const QRectF
PrettyItemDelegate::imageRectify( const QPointF offset ) const
{
    const QRectF imageRect( imageLocationSingleTrack().x() + offset.x(),
                      imageLocationSingleTrack().y() + offset.y(),
                      imageLocationSingleTrack().width() - offset.x() * 2,
                      imageLocationSingleTrack().height() - offset.y() * 2 );
    return imageRect;
}

void Playlist::PrettyItemDelegate::paintItem( PrettyItemConfig config, QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    int rowCount = config.rows();

    if ( rowCount == 0 )
        return;
    
    int rowHeight = option.rect.height() / rowCount;

    int rowOffsetX = MARGINH;
    int rowOffsetY = 0;

    if ( config.showCover() ) {
        int imageSize = option.rect.height() - MARGIN * 2;
        
        QModelIndex coverIndex = index.model()->index( index.row(), CoverImage );
        QPixmap albumPixmap = coverIndex.data( Qt::DisplayRole ).value<QPixmap>();

        QRectF nominalImageRect( MARGINH, MARGIN, imageSize, imageSize );

        //offset cover if non square
        QPointF offset = centerImage( albumPixmap, nominalImageRect );
        QRectF imageRect( nominalImageRect.x() + offset.x(),
                          nominalImageRect.y() + offset.y(),
                          nominalImageRect.width() - offset.x() * 2,
                          nominalImageRect.height() - offset.y() * 2 );

        painter->drawPixmap( imageRect, albumPixmap, QRectF( albumPixmap.rect() ) );


        QModelIndex emblemIndex = index.model()->index( index.row(), SourceEmblem );
        QPixmap emblemPixmap = emblemIndex.data( Qt::DisplayRole ).value<QPixmap>();

        if ( !albumPixmap.isNull() )
            painter->drawPixmap( QRectF( nominalImageRect.x(), nominalImageRect.y() , 16, 16 ), emblemPixmap, QRectF( 0, 0 , 16, 16 ) );

        rowOffsetX = imageSize + MARGINH + PADDING * 2;
    }

    for ( int i = 0; i < rowCount; i++ ) {
        
        PrettyItemConfigRow row = config.row( i );
        qreal itemOffsetX = rowOffsetX;

        int elementCount = row.count();

        qreal rowWidth = option.rect.width() - ( rowOffsetX + MARGINH );


        if ( i == config.activeIndicatorRow() && index.data( ActiveTrackRole ).toBool() )
        {
            painter->drawPixmap( rowOffsetX - 1, rowOffsetY + 1,
                                  The::svgHandler()->renderSvg(
                                  "active_overlay",
                                  rowWidth + 2, rowHeight - 2,
                                  "active_overlay" ) );
        }

        for ( int j = 0; j < elementCount; j++ ) {

            PrettyItemConfigRowElement element = row.element( j );
            qreal itemWidth = rowWidth * element.size();

            //if not the last item, add a PADDING
            if ( j < elementCount - 1 )
                itemWidth += PADDING;

            int value = element.value();
            bool bold = element.bold();
            int alignment = element.alignment();

            QFontMetricsF nfm( option.font );
            QFont boldfont( option.font );
            boldfont.setBold( true );
            QFontMetricsF bfm( boldfont );

            
            QModelIndex textIndex = index.model()->index( index.row(), value );
            QString orgText = textIndex.data( Qt::DisplayRole ).toString();
            QString text;

            QRectF textBox( itemOffsetX, rowOffsetY, itemWidth, rowHeight );

            if ( bold ) 
                text = bfm.elidedText( orgText, Qt::ElideRight, itemWidth );
            else
                text = nfm.elidedText( orgText, Qt::ElideRight, itemWidth );

            QFont font;
            font.setBold( bold );
            painter->setFont( font );

            painter->drawText( textBox, alignment, text );

            itemOffsetX += itemWidth;

        }


        rowOffsetY += rowHeight;


    }
}


