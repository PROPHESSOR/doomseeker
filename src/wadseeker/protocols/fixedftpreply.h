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

#ifndef FTPREPLY_H
#define FTPREPLY_H

#include <QNetworkReply>
#include <QStringList>
#include <QUrlInfo>

class QFtp;

/**
 * @brief This is here because the QNetworkAccessManager treats FTP servers
 * incorrectly.
 *
 * Actually QNetworkAccessManager performs SIZE command while in ASCII mode,
 * which dies on some servers and generates errors... seriously, WTF Trolltech?
 *
 * Original class comes from:
 * http://doc.qt.nokia.com/qq/32/qq32-webkit-protocols.html
 */
class FixedFtpReply : public QNetworkReply
{
	Q_OBJECT

public:
	FixedFtpReply(const QNetworkRequest& request);
	void abort();
	void close();
	qint64 bytesAvailable() const;
	bool isSequential() const;

protected:
	qint64 readData(char *data, qint64 maxSize);

private slots:
	void dataProgressSlot(qint64 current, qint64 total);
	void processCommand(int command, bool error);
	void processListInfo(const QUrlInfo &urlInfo);
	void processData();
	void processDone(bool bError);


private:
	bool bIsAborting;
	QFtp *ftp;
	QByteArray content;
	qint64 offset;

	/**
 	* @brief File size retrieved from listInfo signal. Might not be valid.
 	*
 	* If invalid then zero.
 	*/
	qint64 fileSize;

	void fetchSize();
	void setContent();
};

#endif
