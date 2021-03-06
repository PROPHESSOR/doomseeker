//------------------------------------------------------------------------------
// memorylineedit.h
//------------------------------------------------------------------------------
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//
//------------------------------------------------------------------------------
// Copyright (C) 2009 "Blzut3" <admin@maniacsvault.net>
//------------------------------------------------------------------------------
#ifndef __MEMORYLINEEDIT_H__
#define __MEMORYLINEEDIT_H__

#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QString>

/**
 *	@brief Stores the input into a history that can be accessed by pressing up 
 *	and down.
 */
class MemoryLineEdit : public QLineEdit
{
	Q_OBJECT

	public:
		MemoryLineEdit(QWidget *parent=0);

	protected:
		void keyPressEvent(QKeyEvent *event);

	protected slots:
		void storeCommand();

	private:
		QList<QString> history;
		QList<QString>::iterator position;
};

#endif
