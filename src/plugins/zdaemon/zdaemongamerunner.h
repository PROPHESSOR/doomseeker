//------------------------------------------------------------------------------
// zdaemongamerunner.h
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
// Copyright (C) 2010 "Blzut3" <admin@maniacsvault.net>
//------------------------------------------------------------------------------
#ifndef __ZDAEMON_GAME_RUNNER_H_
#define __ZDAEMON_GAME_RUNNER_H_

#include "serverapi/gamerunner.h"

class EnginePlugin;
class ZDaemonServer;

class ZDaemonGameRunner : public GameRunner
{
	public:
		ZDaemonGameRunner(const ZDaemonServer* server);

		const EnginePlugin* plugin() const;

	protected:
		virtual QString argForConnect() const { return "+connect"; }

		void hostDMFlags(QStringList& args, const DMFlags& dmFlags) const;
};

#endif
