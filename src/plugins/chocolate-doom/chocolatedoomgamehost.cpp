//------------------------------------------------------------------------------
// chocolatedoomgamehost.cpp
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
// Copyright (C) 2013 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#include "chocolatedoomgamehost.h"

#include "chocolatedoomengineplugin.h"
#include "chocolatedoomgameinfo.h"
#include <QString>
#include <serverapi/gamecreateparams.h>

ChocolateDoomGameHost::ChocolateDoomGameHost()
: GameHost(ChocolateDoomEnginePlugin::staticInstance())
{
}

void ChocolateDoomGameHost::addExtra()
{
	args() << "-skill" << QString::number(params().skill() + 1); // from 1 to 5

	switch(params().gameMode().index())
	{
		default: break;
		case GameMode::SGM_Deathmatch:
			args() << "-deathmatch";
			break;
		case ChocolateDoomGameInfo::MODE_ALTDEATH:
			args() << "-altdeath";
			break;
	}

	// Convert map name to proper number for -warp
	QString mapname = params().map().trimmed().toUpper();
	if(mapname.length() == 5 && mapname.startsWith("MAP"))
		args() << "-warp" << mapname.right(2);
	else if(mapname.length() == 4 && mapname[0] == 'E' && mapname[2] == 'M')
		args() << "-warp" << QString("%1%2").arg(mapname[1]).arg(mapname[3]);
}
