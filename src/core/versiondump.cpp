//------------------------------------------------------------------------------
// versiondump.cpp
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
#include "versiondump.h"

#include "plugins/engineplugin.h"
#include "plugins/pluginloader.h"
#include "json.h"
#include "version.h"
#include <QIODevice>
#include <QString>
#include <wadseeker/wadseekerversioninfo.h>

class Module
{
	public:
		Module(const QString& displayName, unsigned long long revision,
			const QString& displayVersion = QString())
		{
			_displayName = displayName;
			_revision = revision;
			_displayVersion = displayVersion;
		}

		QVariantMap toVariantMap()
		{
			QVariantMap result;
			result["display-name"] = _displayName;
			result["revision"] = _revision;
			if (!_displayVersion.isNull())
			{
				result["display-version"] = _displayVersion;
			}
			return result;
		}

	private:
		QString _displayName;
		unsigned long long _revision;
		QString _displayVersion;
};

void VersionDump::dumpJsonToIO(QIODevice& io)
{
	QVariantMap root;
	root["doomseeker"] = Module(Version::name(), Version::revisionNumber(),
		Version::versionRevision()).toVariantMap();
	root["wadseeker"] = Module("Wadseeker", Version::revisionNumber(),
		WadseekerVersionInfo::version()).toVariantMap();
	for (unsigned int i = 0; i < gPlugins->numPlugins(); ++i)
	{
		const PluginLoader::Plugin* plugin = gPlugins->plugin(i);
		QString name = plugin->info()->data()->name;
		QString keyword = "p-" + name.toLower().replace(" ", "");
		root[keyword] = Module(name, plugin->info()->data()->version).toVariantMap();
	}

	io.write(QtJson::Json::serialize(root));
}
