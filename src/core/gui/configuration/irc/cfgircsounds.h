//------------------------------------------------------------------------------
// cfgircsounds.h
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
// Copyright (C) 2011 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#ifndef __CFGIRCSOUNDS_H__
#define __CFGIRCSOUNDS_H__

#include "ui_cfgircsounds.h"
#include "gui/configuration/configurationbasebox.h"

class CFGIRCSounds : public ConfigurationBaseBox, private Ui::CFGIRCSounds
{
	Q_OBJECT

	public:
		CFGIRCSounds(QWidget* parent = NULL);
		~CFGIRCSounds();
		
		QIcon icon() const { return QIcon(":/icons/audio-speaker.png"); }
		QString name() const { return tr("Sounds"); }
		void readSettings();
		QString title() const { return tr("IRC - Sounds"); }
		
	protected:
		void saveSettings();
		
	private:
		QString getPathToWav();
		void setPath(QLineEdit* pLineEdit, const QString& path);
		
	private slots:
		void btnBrowseNicknameUsedClicked();
		void btnBrowsePrivateMessageClicked();
};


#endif
