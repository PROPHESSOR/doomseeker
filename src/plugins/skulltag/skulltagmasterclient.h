//------------------------------------------------------------------------------
// skulltagmasterclient.h
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

#ifndef __SKULLTAGMASTERCLIENT_H__
#define __SKULLTAGMASTERCLIENT_H__

#include "masterserver/masterclient.h"

class EnginePlugin;

class SkulltagMasterClient : public MasterClient
{
	Q_OBJECT

	public:
		SkulltagMasterClient();

		const EnginePlugin* plugin() const;

		bool readMasterResponse(QByteArray &data);

	public slots:
		void refresh();

	protected:
		bool getServerListRequest(QByteArray &data);

		/// This is used as a marker to determine if we have an accurate reading for numPackets.
		bool readLastPacket;
		int numPackets;
		unsigned int packetsRead;
};

#endif /* __SKULLTAGMASTERCLIENT_H__ */
