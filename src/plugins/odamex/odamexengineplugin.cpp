//------------------------------------------------------------------------------
// odamexengineplugin.cpp
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

#include <QDateTime>

#include "irc/entities/ircnetworkentity.h"
#include "plugins/engineplugin.h"

#include "odamexgamehost.h"
#include "odamexgameinfo.h"
#include "odamexmasterclient.h"
#include "odamexengineplugin.h"
#include "odamexserver.h"

INSTALL_PLUGIN(OdamexEnginePlugin)

OdamexEnginePlugin::OdamexEnginePlugin()
{
	const // clear warnings
	#include "odamex.xpm"

	init("Odamex", odamex_xpm,
		EP_Author, "The Doomseeker Team",
		EP_Version, 19,

		EP_AllowsURL,
		EP_AllowsEmail,
		EP_AllowsJoinPassword,
		EP_AllowsRConPassword,
		EP_AllowsMOTD,
		EP_DefaultMaster, "master1.odamex.net:15000",
		EP_DefaultServerPort, 10666,
		EP_GameModes, OdamexGameInfo::gameModes(),
		EP_HasMasterServer,
		EP_IRCChannel, "Odamex", "irc.quakenet.org", "#odamex",
		EP_SupportsRandomMapRotation,
		EP_InGameFileDownloads,
		EP_DemoExtension, true, "odd",
		EP_Done
	);
}

GameHost* OdamexEnginePlugin::gameHost()
{
	return new OdamexGameHost();
}

MasterClient *OdamexEnginePlugin::masterClient() const
{
	return new OdamexMasterClient();
}

ServerPtr OdamexEnginePlugin::mkServer(const QHostAddress &address, unsigned short port) const
{
	return ServerPtr(new OdamexServer(address, port));
}
