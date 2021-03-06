//------------------------------------------------------------------------------
// flagspage.h
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
// Copyright (C) 2012 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#ifndef DOOMSEEKER_PLUGIN_ZANDRONUM_CREATESERVERDIALOGPAGES_FLAGSPAGE_H
#define DOOMSEEKER_PLUGIN_ZANDRONUM_CREATESERVERDIALOGPAGES_FLAGSPAGE_H

#include "ui_flagspage.h"
#include <gui/widgets/createserverdialogpage.h>

class FlagsPage : public CreateServerDialogPage, private Ui::FlagsPage
{
	friend class FlagsPageValueController;

	Q_OBJECT;

	public:
		enum FallingDamageType
		{
			FDT_None = 0,
			FDT_Old = 1,
			FDT_Hexen = 2,
			FDT_Strife = 3
		};

		enum JumpCrouchAbility
		{
			JCA_Default = 0,
			JCA_No = 1,
			JCA_Yes = 2
		};

		FlagsPage(CreateServerDialog* pParentDialog);
		~FlagsPage();

		virtual QStringList generateGameRunParameters();
		virtual bool loadConfig(Ini& ini);
		virtual bool saveConfig(Ini& ini);

	private:
		class PrivData;

		PrivData* d;

		void initJumpCrouchComboBoxes(QComboBox* pComboBox);
		void insertFlagsIfValid(QLineEdit* dst, QString flags, unsigned valIfInvalid = 0);

	private slots:
		/**
		 * @brief Extracts dmflags values from widgets and inserts their
		 *        numerical values into the text input widgets.
		 */
		void applyWidgetsChange();

		/**
		 * @brief Takes the numerical values of flags and applies them to
		 *        widgets.
		 */
		void propagateFlagsInputsChanges();
};

#endif
