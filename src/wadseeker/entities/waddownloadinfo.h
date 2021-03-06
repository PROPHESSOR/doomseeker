//------------------------------------------------------------------------------
// waddownloadinfo.h
//------------------------------------------------------------------------------
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301  USA
//
//------------------------------------------------------------------------------
// Copyright (C) 2011 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#ifndef __WADDOWNLOADINFO_H__
#define __WADDOWNLOADINFO_H__

#include <QByteArray>
#include <QString>

/**
 * @brief Holds information about unique WAD download.
 */
class WadDownloadInfo
{
	public:
		WadDownloadInfo();
		WadDownloadInfo(const QString& name);

		/**
		 * @brief Gets name for the archive that may contain the mentioned file.
		 *
		 * @b NOTE: If isArchive() returns true this will return the same value
		 * as name().
		 *
		 * @param suffix
		 *      Archive's extensions without the '.' character. For example
		 *      "zip" or "7z".
		 */
		QString archiveName(const QString& suffix) const;

		/**
		 * @brief Gets the name() minus the extension.
		 */
		QString basename() const;

		/**
 		 * @brief Recognizes if download is an archive basing on the extension
		 * in the name specified in the constructor.
		 */
		bool isArchive() const;

		/**
		 * @brief Checks if filename is the same WAD as this object.
		 *
		 * The check is done by comparing the basenames parts of the filename
		 * with suffixes stripped.
		 */
		bool isFilenameIndicatingSameWad(const QString& filename) const;

		const QString& name() const
		{
			return d.name;
		}

		/**
		 * @brief Check if name() values match.
		 *
		 * Check is case-insensitive.
		 */
		bool operator==(const WadDownloadInfo& other) const;

		/**
		 * @brief Check if name() values mismatch.
		 *
		 * Check is case-insensitive.
		 */
		bool operator!=(const WadDownloadInfo& other) const;

		/**
		 * @brief Return possible archive names.
		 *
		 * @return If isArchive() returns true, list will contain only the
		 *         name() value. Otherwise, the returned list will contain
		 *         filenames for all supported archives, where the archive
		 *         extension will replace the extension from name().
		 */
		QStringList possibleArchiveNames() const;
		
		/**
		 * @brief Size of the file.
		 *
		 * @param size 
		 *      Size of the file in bytes if known. If unknown
		 *      then a negative value should be set.
		 */
		void setSize(qint64 size)
		{
			d.size = size;
		}
		
		/**
		 * @brief Size of the file if known.
		 *
		 * @return Size of the file in bytes if known. If unknown
		 *         then a negative value is returned.
		 */
		qint64 size() const
		{
			return d.size;
		}

	private:
		class PrivData
		{
			public:
				QString name;
				qint64 size;
		};

		PrivData d;
};

#endif
