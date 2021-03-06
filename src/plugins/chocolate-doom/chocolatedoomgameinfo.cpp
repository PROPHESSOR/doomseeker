//------------------------------------------------------------------------------
// chocolatedoomgameinfo.h
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
// Copyright (C) 2013 Braden "Blzut3" Obrzut <admin@maniacsvault.net>
//------------------------------------------------------------------------------ 

#include "chocolatedoomgameinfo.h"
#include <QObject>

QList<GameMode> ChocolateDoomGameInfo::gameModesList;
ChocolateDoomGameInfo* ChocolateDoomGameInfo::static_constructor = new ChocolateDoomGameInfo();

ChocolateDoomGameInfo::ChocolateDoomGameInfo()
{
	initGameModes();

	delete static_constructor;
}

void ChocolateDoomGameInfo::initGameModes()
{
	gameModesList << GameMode::mkCooperative();
	gameModesList << GameMode::mkDeathmatch();
	gameModesList << GameMode::ffaGame(MODE_ALTDEATH, QObject::tr("Altdeath (Deathmatch 2.0)"));
}
