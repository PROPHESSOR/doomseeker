//------------------------------------------------------------------------------
// wadseekershow.cpp
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
// Copyright (C) 2014 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#include "wadseekershow.h"

#include "configuration/doomseekerconfig.h"
#include "gui/mainwindow.h"
#include "gui/wadseekerinterface.h"
#include "serverapi/server.h"
#include "application.h"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>

class WadseekerShow::PrivData
{
public:
	QMap<WadseekerInterface*, ServerPtr> wadseekers;
};

WadseekerShow *WadseekerShow::staticInstance = NULL;

WadseekerShow::WadseekerShow()
{
	d = new PrivData();
}

bool WadseekerShow::checkWadseekerValidity(QWidget *parent)
{
	if (WadseekerInterface::isInstantiated())
	{
		QMessageBox::warning(parent, tr("Wadseeker cannot be launched"),
			tr("Another instance of Wadseeker is already running."), QMessageBox::Ok);
		return false;
	}

	QString targetDirPath = gConfig.wadseeker.targetDirectory;
	QDir targetDir(targetDirPath);
	QFileInfo targetDirFileInfo(targetDirPath);

	if (targetDirPath.isEmpty() || !targetDir.exists() || !targetDirFileInfo.isWritable())
	{
		QString error = tr("Wadseeker will not work correctly:\n\n"
				"Target directory is either not set, is invalid or cannot be written to.\n\n"
				"Please review your Configuration and/or refer to online help available from "
				"the Help menu.");
		QMessageBox::warning(parent, tr("Wadseeker cannot be launched"),
			error, QMessageBox::Ok);
		return false;
	}

	return true;
}

WadseekerShow *WadseekerShow::instance()
{
	if (staticInstance == NULL)
	{
		staticInstance = new WadseekerShow();
	}
	return staticInstance;
}

void WadseekerShow::onWadseekerWithServerDone(int result)
{
	WadseekerInterface *wadseeker = static_cast<WadseekerInterface*>(sender());
	ServerPtr server = d->wadseekers[wadseeker];
	d->wadseekers.remove(wadseeker);
	if (result == QDialog::Accepted)
	{
		gApp->mainWindow()->runGame(server);
	}
}

void WadseekerShow::registerWadseekerWithServer(ServerPtr server, WadseekerInterface *wadseeker)
{
	d->wadseekers[wadseeker] = server;
	this->connect(wadseeker, SIGNAL(finished(int)), SLOT(onWadseekerWithServerDone(int)));
}
