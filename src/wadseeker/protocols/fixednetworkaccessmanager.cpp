/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the documentation of Qt. It was originally
** published as part of Qt Quarterly.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights.  These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "fixednetworkaccessmanager.h"

#include <QtNetwork>
#include "protocols/fixedftpreply.h"

FixedNetworkAccessManager::FixedNetworkAccessManager(QObject *parent)
	: QNetworkAccessManager(parent)
{
}

QNetworkReply *FixedNetworkAccessManager::createRequest(
	QNetworkAccessManager::Operation operation, const QNetworkRequest &request,
	QIODevice *device)
{
	if (request.url().scheme() != "ftp")
		return QNetworkAccessManager::createRequest(operation, request, device);

	if (operation == GetOperation)
		// Handle ftp:// URLs separately by creating custom QNetworkReply
		// objects.
		return new FixedFtpReply(request);
	else
		return QNetworkAccessManager::createRequest(operation, request, device);
}


QString FixedNetworkAccessManager::networkErrorToString(QNetworkReply::NetworkError error) 
{
	QMetaObject meta = QNetworkReply::staticMetaObject;
	for (int i=0; i < meta.enumeratorCount(); ++i) 
	{
		QMetaEnum m = meta.enumerator(i);
		if (m.name() == QLatin1String("NetworkError")) 
		{
			return QLatin1String(m.valueToKey(error));
		}
	}
	
	return tr("Unknown Error");
}
