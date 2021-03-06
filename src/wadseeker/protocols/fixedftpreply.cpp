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
#include "fixedftpreply.h"

#include <QDebug>
#include <QtNetwork>

FixedFtpReply::FixedFtpReply(const QNetworkRequest& request)
	: QNetworkReply()
{
	bIsAborting = false;
	offset = 0;
	fileSize = 0;
	
	QUrl url = request.url();

	ftp = new QFtp(this);
	connect(ftp, SIGNAL(done(bool)), this, SLOT(processDone(bool)));
	connect(ftp, SIGNAL(listInfo(QUrlInfo)), this, SLOT(processListInfo(QUrlInfo)));
	connect(ftp, SIGNAL(readyRead()), this, SLOT(processData()));
	connect(ftp, SIGNAL(commandFinished(int, bool)), this, SLOT(processCommand(int, bool)));

	this->connect(ftp, SIGNAL(dataTransferProgress(qint64, qint64)),
			SLOT( dataProgressSlot(qint64, qint64) ));

	setRequest(request);
	setUrl(url);
	ftp->connectToHost(url.host());
}

void FixedFtpReply::dataProgressSlot(qint64 current, qint64 total)
{
	if (total <= 0)
	{
		// Replace total with file size stored in the object's memory, if
		// current total is invalid.
		total = fileSize;
	}

	emit downloadProgress(current, total);
}

void FixedFtpReply::fetchSize()
{
	QString cmd = QString("SIZE %1").arg(url().path());
	ftp->rawCommand(cmd);
}

void FixedFtpReply::processCommand(int, bool err)
{
#ifndef NDEBUG
	qDebug() << "FTP - process command:" << ftp->currentCommand() << "err:" << err;
#endif

	if (err) {
		setError(ContentNotFoundError, "Unknown command");
		emit error(ContentNotFoundError);
		abort();
		return;
	}

	switch (ftp->currentCommand()) {
	case QFtp::ConnectToHost:
		if (!bIsAborting)
		{
			ftp->login();
		}
		break;

	case QFtp::Login:
		ftp->list(url().path());
		break;

	case QFtp::List:
		ftp->get(url().path());
		break;

	case QFtp::Get:
		setContent();
		break;

	default:
		;
	}
}

void FixedFtpReply::processDone(bool bError)
{
	emit finished();
}

void FixedFtpReply::processListInfo(const QUrlInfo &urlInfo)
{
	this->fileSize = urlInfo.size() > 0 ? urlInfo.size() : 0;
}

void FixedFtpReply::processData()
{
	content += ftp->readAll();
}

void FixedFtpReply::setContent()
{
	open(ReadOnly | Unbuffered);
	setHeader(QNetworkRequest::ContentLengthHeader, QVariant(content.size()));
	emit readyRead();
	emit finished();
	ftp->close();
}

// QIODevice methods

void FixedFtpReply::abort()
{
	bIsAborting = true;
	ftp->abort();
	ftp->disconnect(this);
	emit finished();
}

void FixedFtpReply::close()
{
	ftp->close();
}

qint64 FixedFtpReply::bytesAvailable() const
{
	return content.size() - offset;
}

bool FixedFtpReply::isSequential() const
{
	return true;
}

qint64 FixedFtpReply::readData(char *data, qint64 maxSize)
{
	if (offset < content.size()) {
		qint64 number = qMin(maxSize, content.size() - offset);
		memcpy(data, content.constData() + offset, number);
		offset += number;
		return number;
	} else
		return -1;
}
