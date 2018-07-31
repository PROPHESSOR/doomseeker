//------------------------------------------------------------------------------
// serverlistview.cpp
//------------------------------------------------------------------------------
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301  USA
//
//------------------------------------------------------------------------------
// Copyright (C) 2009 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------

#include "gui/widgets/serverlistview.h"

#include "configuration/doomseekerconfig.h"
#include "gui/models/serverlistcolumn.h"
#include "refresher/refresher.h"
#include <QDebug>
#include <QHeaderView>
#include <QItemDelegate>
#include <QMouseEvent>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QTextDocument>

/**
 * Custom delegate class.  This allows us to right align the decoration images.
 * To do this the UserRole needs to be set to USERROLE_RIGHTALIGNDECORATION.
 */
class CustomItemDelegate : public QItemDelegate
{
	public:
		void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
		{
			// First we're going to check for our new right aligned image
			// option.
			bool rightAligned = false;
			QStyleOptionViewItem opt = option;

			QVariant userRole = index.data(Qt::UserRole);
			if(userRole.isValid() && userRole.type() == QVariant::Int && userRole.toInt() == USERROLE_RIGHTALIGNDECORATION)
			{
				opt.decorationAlignment = Qt::AlignRight|Qt::AlignVCenter;
				rightAligned = true;
			}

			// Now we draw the table as usual.
			QItemDelegate::paint(painter, opt, index);

			// If the row is selected and we are using the right aligned feature
			// we must now redraw the decoration.  The rectangle that was used
			// in the previous function will cause the image to clip.
			//
			// The only other way I can think of for fixing that problem would
			// be to completely rewrite this class, which I really don't want
			// to do.
			if(rightAligned && (opt.state & QStyle::State_Selected))
			{
				QVariant decorationRole = index.data(Qt::DecorationRole);
				if(decorationRole.isValid())
				{
					painter->save();
					painter->setClipRect(opt.rect);

					QPixmap pixmap = decoration(opt, decorationRole);
					drawDecoration(painter, opt, opt.rect, pixmap);

					painter->restore();
				}
			}
		}

	protected:
		void drawDecoration(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QPixmap &pixmap) const
		{
			if(pixmap.isNull() || !rect.isValid())
				return;

			if(option.decorationAlignment == (Qt::AlignRight|Qt::AlignVCenter)) // Special handling.
			{
				QPoint p = QStyle::alignedRect(option.direction, option.decorationAlignment, pixmap.size(), option.rect).topLeft();
				painter->drawPixmap(p, pixmap);
			}
			else
				QItemDelegate::drawDecoration(painter, option, rect, pixmap);
		}
};

////////////////////////////////////////////////////////////////////////////////

ServerListView::ServerListView(QWidget* parent) : QTableView(parent)
{
	// Prevent the fat rows problem.
	verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
#if QT_VERSION >= 0x050000
	verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
#else
	verticalHeader()->setResizeMode(QHeaderView::Fixed);
#endif
	setShowGrid(gConfig.doomseeker.bDrawGridInServerTable);

	setItemDelegate(new CustomItemDelegate());
}

void ServerListView::mouseReleaseEvent(QMouseEvent* event)
{
	QModelIndex index = indexAt(event->pos());
	switch (event->button())
	{
		case Qt::MidButton:
			if (index.isValid())
			{
				emit middleMouseClicked(index, event->pos());
			}
			break;

		case Qt::RightButton:
			if (index.isValid())
			{
				emit rightMouseClicked(index, event->pos());
			}
			break;

		default:
			QTableView::mouseReleaseEvent(event);
			break;
	}
}

void ServerListView::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		QTableView::mouseDoubleClickEvent(event);
	}
	else
	{
		QModelIndex index = indexAt(event->pos());
		if (index.isValid())
		{
			emit leftMouseDoubleClicked(index, event->pos());
		}
	}
}

void ServerListView::setupTableProperties()
{
	setIconSize(QSize(26, 15));
	// Some flags that can't be set from the Designer.
	horizontalHeader()->setSortIndicatorShown(true);
	horizontalHeader()->setHighlightSections(false);

	setMouseTracking(true);

	setupTableColumnWidths();
}

void ServerListView::setupTableColumnWidths()
{
	QString &headerState = gConfig.doomseeker.serverListColumnState;
	if(headerState.isEmpty())
	{
		for (int i = 0; i < ServerListColumnId::NUM_SERVERLIST_COLUMNS; ++i)
		{
			ServerListColumn* columns = ServerListColumns::columns;
			setColumnWidth(i, columns[i].width);
			setColumnHidden(i, columns[i].bHidden);
			if(!columns[i].bResizable)
			{
#if QT_VERSION >= 0x050000
				horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
#else
				horizontalHeader()->setResizeMode(i, QHeaderView::Fixed);
#endif
			}
		}
	}
	else
		horizontalHeader()->restoreState(QByteArray::fromBase64(headerState.toUtf8()));

#if QT_VERSION >= 0x050000
	horizontalHeader()->setSectionsMovable(true);
#else
	horizontalHeader()->setMovable(true);
#endif
}
