//------------------------------------------------------------------------------
// createserverdialog.cpp
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
// Copyright (C) 2009-2012 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#include "createserverdialog.h"

#include "configuration/doomseekerconfig.h"
#include "copytextdlg.h"
#include "gui/widgets/createserverdialogpage.h"
#include "datapaths.h"
#include "commongui.h"
#include "ini/ini.h"
#include "ini/settingsproviderqt.h"
#include "pathfinder/pathfinder.h"
#include "plugins/engineplugin.h"
#include "plugins/pluginloader.h"
#include "scanner.h"
#include "serverapi/exefile.h"
#include "serverapi/gameexeretriever.h"
#include "serverapi/gamecreateparams.h"
#include "serverapi/gamehost.h"
#include "serverapi/message.h"
#include "serverapi/server.h"
#include "apprunner.h"
#include "commandline.h"
#include "strings.h"

#include <QCheckBox>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QScopedPointer>
#include <QStandardItemModel>
#include <QTimer>

class CreateServerDialog::PrivData
{
	public:
		bool bSuppressMissingExeErrors;
		bool bIsServerSetup;
		bool iwadSetExplicitly;
		QList<CreateServerDialogPage*> currentCustomPages;
		EnginePlugin *currentEngine;
		QList<DMFlagsTabWidget*> dmFlagsTabs;
		QList<GameLimitWidget*> limitWidgets;
		QList<GameCVar> gameModifiers;
};

const QString CreateServerDialog::TEMP_SERVER_CONFIG_FILENAME = "/tmpserver.ini";

CreateServerDialog::CreateServerDialog(QWidget* parent)
: QDialog(parent)
{
	d = new PrivData();
	// Have the console delete itself
	setAttribute(Qt::WA_DeleteOnClose);

	d->bSuppressMissingExeErrors = true;
	d->bIsServerSetup = false;
	d->currentEngine = NULL;
	d->iwadSetExplicitly = false;

	setupUi(this);
	connect(btnAddMapToMaplist, SIGNAL( clicked() ), this, SLOT ( btnAddMapToMaplistClicked() ) );
	connect(btnAddPwad, SIGNAL( clicked() ), this, SLOT ( btnAddPwadClicked() ) );
	connect(btnBrowseExecutable, SIGNAL( clicked() ), this, SLOT ( btnBrowseExecutableClicked() ) );
	connect(btnCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
	connect(btnCommandLine, SIGNAL( clicked() ), this, SLOT( btnCommandLineClicked() ) );
	connect(btnClearPwadList, SIGNAL( clicked() ), this, SLOT( btnClearPwadListClicked() ) );
	connect(btnDefaultExecutable, SIGNAL( clicked() ), this, SLOT( btnDefaultExecutableClicked() ) );
	connect(btnIwadBrowse, SIGNAL( clicked() ), this, SLOT ( btnIwadBrowseClicked() ) );
	connect(btnLoad, SIGNAL( clicked() ), this, SLOT ( btnLoadClicked() ) );
	connect(btnPlayOffline, SIGNAL( clicked() ), this, SLOT ( btnPlayOfflineClicked() ) );
	connect(btnRemoveMapFromMaplist, SIGNAL( clicked() ), this, SLOT ( btnRemoveMapFromMaplistClicked() ) );
	connect(btnRemovePwad, SIGNAL( clicked() ), this, SLOT ( btnRemovePwadClicked() ) );
	connect(btnSave, SIGNAL( clicked() ), this, SLOT ( btnSaveClicked() ) );
	connect(btnStartServer, SIGNAL( clicked() ), this, SLOT( btnStartServerClicked() ) );

	connect(cboEngine, SIGNAL( currentIndexChanged(int) ), this, SLOT( cboEngineSelected(int) ) );
	connect(cboGamemode, SIGNAL( currentIndexChanged(int) ), this, SLOT( cboGamemodeSelected(int) ) );

	connect(lstAdditionalFiles, SIGNAL( fileSystemPathDropped(const QString& ) ),
		this, SLOT( lstAdditionalFilesPathDnd(const QString&) ) );

	connect(QApplication::instance(), SIGNAL( focusChanged(QWidget*, QWidget*) ), this, SLOT( focusChanged(QWidget*, QWidget*) ));

	cboIwad->setEditable(true);
	lstAdditionalFiles->setModel(new QStandardItemModel(this));
	lstMaplist->setModel(new QStandardItemModel(this));

	initPrimary();
	d->bSuppressMissingExeErrors = false;

	// This is a crude solution to the problem where message boxes appear
	// before the actual Create Server dialog. We need to give some time
	// for the Dialog to appear. Unfortunately reimplementing
	// QDialog::showEvent() didn't work very well.
	QTimer::singleShot(1, this, SLOT(firstLoadConfigTimer()) );
}

CreateServerDialog::~CreateServerDialog()
{
	delete d;
}

void CreateServerDialog::addIwad(const QString& path)
{
	if (path.isEmpty())
	{
		return;
	}

	for (int i = 0; i < cboIwad->count(); ++i)
	{
		if (cboIwad->itemText(i).compare(path) == 0)
		{
			cboIwad->setCurrentIndex(i);
			return;
		}
	}

	QString cleanPath = Strings::normalizePath(path);

	cboIwad->addItem(cleanPath);
	cboIwad->setCurrentIndex(cboIwad->count() - 1);
}

void CreateServerDialog::addMapToMaplist(const QString& map)
{
	if (map.isEmpty())
	{
		return;
	}

	QStandardItemModel* model = static_cast<QStandardItemModel*>(lstMaplist->model());

	QStandardItem* it = new QStandardItem(map);

	it->setDragEnabled(true);
	it->setDropEnabled(false);

	model->appendRow(it);
}

void CreateServerDialog::addWadPath(const QString& strPath)
{
	if (strPath.isEmpty())
	{
		return;
	}

	QStandardItemModel* model = static_cast<QStandardItemModel*>(lstAdditionalFiles->model());

	// Check if this path exists already, if so - do nothing.
	for(int i = 0; i < model->rowCount(); ++i)
	{
		QStandardItem* item = model->item(i);
		QString dir = item->text();
		Qt::CaseSensitivity cs;

		#ifdef Q_OS_WIN32
		cs = Qt::CaseInsensitive;
		#else
		cs = Qt::CaseSensitive;
		#endif

		if (dir.compare(strPath, cs) == 0)
		{
			return;
		}
	}

	QStandardItem* it = new QStandardItem(strPath);

	it->setDragEnabled(true);
	it->setDropEnabled(false);
	it->setToolTip(strPath);

	model->appendRow(it);
}

void CreateServerDialog::btnAddMapToMaplistClicked()
{
	addMapToMaplist(leMapname->text());
}

void CreateServerDialog::btnAddPwadClicked()
{
	QString dialogDir = gConfig.doomseeker.previousCreateServerWadDir;
	QStringList filesNames = QFileDialog::getOpenFileNames(this, tr("Doomseeker - Add file(s)"), dialogDir);

	if (!filesNames.isEmpty())
	{
		// Remember the directory of the first file. This directory will be
		// restored the next time this dialog is opened.
		QFileInfo fi(filesNames[0]);
		gConfig.doomseeker.previousCreateServerWadDir = fi.absolutePath();

		foreach (const QString& strFile, filesNames)
		{
			addWadPath(strFile);
		}
	}
}

void CreateServerDialog::btnBrowseExecutableClicked()
{
	QString dialogDir = gConfig.doomseeker.previousCreateServerExecDir;
	QString strFile = QFileDialog::getOpenFileName(this, tr("Doomseeker - Add file"), dialogDir);

	if (!strFile.isEmpty())
	{
		QFileInfo fi(strFile);
		gConfig.doomseeker.previousCreateServerExecDir = fi.absolutePath();

		leExecutable->setText(fi.absoluteFilePath());
	}
}

void CreateServerDialog::btnClearPwadListClicked()
{
	QStandardItemModel* pModel = (QStandardItemModel*)lstAdditionalFiles->model();
	pModel->clear();
}

void CreateServerDialog::btnCommandLineClicked()
{
	QString executable;
	QStringList args;
	if(commandLineArguments(executable, args))
	{
		// Lines below directly modify the passed values.
		CommandLine::escapeExecutable(executable);
		CommandLine::escapeArgs(args);

		CopyTextDlg ctd(executable + " " + args.join(" "), "Host server command line:", this);
		ctd.exec();
	}
}

void CreateServerDialog::btnDefaultExecutableClicked()
{
	Message message;
	leExecutable->setText(pathToServerExe(message));

	if (!message.isIgnore())
	{
		QMessageBox::critical(NULL, tr("Obtaining default server binary path."),
			message.contents(),QMessageBox::Ok, QMessageBox::Ok);
	}
}

void CreateServerDialog::btnIwadBrowseClicked()
{
	QString dialogDir = gConfig.doomseeker.previousCreateServerWadDir;
	QString strFile = QFileDialog::getOpenFileName(this, tr("Doomseeker - select IWAD"), dialogDir);

	if (!strFile.isEmpty())
	{
		QFileInfo fi(strFile);
		gConfig.doomseeker.previousCreateServerWadDir = fi.absolutePath();

		addIwad(strFile);
	}
}

void CreateServerDialog::btnLoadClicked()
{
	QString dialogDir = gConfig.doomseeker.previousCreateServerConfigDir;
	QString strFile = QFileDialog::getOpenFileName(this, tr("Doomseeker - load server config"), dialogDir, tr("Config files (*.ini)"));

	if (!strFile.isEmpty())
	{
		QFileInfo fi(strFile);
		gConfig.doomseeker.previousCreateServerConfigDir = fi.absolutePath();

		loadConfig(strFile, false);
	}
}

void CreateServerDialog::btnPlayOfflineClicked()
{
	runGame(true);
}

void CreateServerDialog::btnRemoveMapFromMaplistClicked()
{
	const bool bSelectNextLowest = true;
	CommonGUI::removeSelectedRowsFromStandardItemView(lstMaplist, bSelectNextLowest);
}

void CreateServerDialog::btnRemovePwadClicked()
{
	const bool bSelectNextLowest = true;
	CommonGUI::removeSelectedRowsFromStandardItemView(lstAdditionalFiles, bSelectNextLowest);
}

void CreateServerDialog::btnSaveClicked()
{
	QString dialogDir = gConfig.doomseeker.previousCreateServerConfigDir;
	QString strFile = QFileDialog::getSaveFileName(this, tr("Doomseeker - save server config"), dialogDir, tr("Config files (*.ini)"));
	if (!strFile.isEmpty())
	{
		QFileInfo fi(strFile);
		gConfig.doomseeker.previousCreateServerConfigDir = fi.absolutePath();

		if (fi.suffix().isEmpty())
		{
			strFile += ".ini";
		}

		if (!saveConfig(strFile))
		{
			QMessageBox::critical(NULL, tr("Doomseeker - save server config"), tr("Unable to save server configuration!"));
		}
	}

}

void CreateServerDialog::btnStartServerClicked()
{
	if(!d->bIsServerSetup)
		runGame(false);
	else
		accept();
}

void CreateServerDialog::cboEngineSelected(int index)
{
	if (index >= 0)
	{
		unsigned enginePluginIndex = cboEngine->itemData(index).toUInt();
		if (enginePluginIndex < gPlugins->numPlugins())
		{
			initEngineSpecific(gPlugins->info(enginePluginIndex));
		}
	}
}

void CreateServerDialog::cboGamemodeSelected(int index)
{
	if (index >= 0)
	{
		const QList<GameMode>* gameModes = d->currentEngine->data()->gameModes;
		if (gameModes != NULL)
		{
			initGamemodeSpecific((*gameModes)[index]);
		}
	}
}

bool CreateServerDialog::commandLineArguments(QString &executable, QStringList &args)
{
	const QString errorCapt = tr("Doomseeker - create server");
	if (d->currentEngine == NULL)
	{
		QMessageBox::critical(NULL, errorCapt, tr("No engine selected"));
		return false;
	}

	GameCreateParams gameParams;
	if (createHostInfo(gameParams, false))
	{
		CommandLineInfo cli;
		QString error;

		GameHost* gameRunner = d->currentEngine->gameHost();
		Message message = gameRunner->createHostCommandLine(gameParams, cli);

		delete gameRunner;

		if (message.isError())
		{
			QMessageBox::critical(NULL, tr("Doomseeker - error"), message.contents());
			return false;
		}
		else
		{
			executable = cli.executable.absoluteFilePath();
			args = cli.args;
			return true;
		}
	}
	return false;
}

bool CreateServerDialog::createHostInfo(GameCreateParams& params, bool offline)
{
	// Since some operating systems have different offline and server binaries
	// We will see if they are playing offline and switch to the client
	// binary if the specified executable is the same as what is provided
	// as the server.
	Message message;
	QString offlineExePath = pathToOfflineExe(message);
	QString serverExePath = pathToServerExe(message);
	bool bIsLineEditPotiningToServerBinary = (leExecutable->text() == serverExePath);
	bool bShouldUseClientBinary = (offline || d->bIsServerSetup) && message.isIgnore() && bIsLineEditPotiningToServerBinary;

	params.setHostMode(offline ? GameCreateParams::Offline : GameCreateParams::Host);
	if (bShouldUseClientBinary)
	{
		params.setExecutablePath(offlineExePath);
	}
	else
	{
		params.setExecutablePath(leExecutable->text());
	}

	params.setIwadPath(cboIwad->currentText());
	params.setPwadsPaths(CommonGUI::listViewStandardItemsToStringList(lstAdditionalFiles));

	// DMFlags
	foreach(const DMFlagsTabWidget* p, d->dmFlagsTabs)
	{
		DMFlagsSection sec(p->section.name());
		for (int i = 0; i < p->section.count(); ++i)
		{
			if (p->checkBoxes[i]->isChecked())
			{
				sec.add(p->section[i]);
			}
		}
		params.dmFlags() << sec;
	}

	// Custom pages.
	QStringList customPagesParams;
	foreach (CreateServerDialogPage* page, d->currentCustomPages)
	{
		if (page->validate())
		{
			customPagesParams << page->generateGameRunParameters();
		}
		else
		{
			// Pages must take care of displaying their own error messages.
			tabWidget->setCurrentIndex(tabWidget->indexOf(page));
			return false;
		}
	}
	params.setCustomParameters(customPagesParams);

	// limits
	foreach(GameLimitWidget* p, d->limitWidgets)
	{
		p->limit.setValue(p->spinBox->value());
		params.cvars() << p->limit;
	}

	// modifier
	int modIndex = cboModifier->currentIndex();
	if (modIndex > 0) // Index zero is always "< NONE >"
	{
		--modIndex;
		d->gameModifiers[modIndex].setValue(1);
		params.cvars() << d->gameModifiers[modIndex];
	}

	// Custom parameters
	{
		QString cp = pteCustomParameters->toPlainText();
		Scanner sc(cp.toAscii().constData(), cp.length());
		while (sc.nextString())
		{
			QString param = sc->str();
			params.customParameters() << param;
		}
	}

	// Other
	params.setBroadcastToLan(cbBroadcastToLAN->isChecked());
	params.setBroadcastToMaster(cbBroadcastToMaster->isChecked());
	params.setEmail(leEmail->text());
	params.setMap(leMap->text());
	params.setMapList(CommonGUI::listViewStandardItemsToStringList(lstMaplist));
	params.setRandomMapRotation(cbRandomMapRotation->isChecked());
	params.setMaxClients(spinMaxClients->value());
	params.setMaxPlayers(spinMaxPlayers->value());
	params.setMotd(pteMOTD->toPlainText());
	params.setName(leServername->text());
	params.setConnectPassword(leConnectPassword->text());
	params.setIngamePassword(leJoinPassword->text());
	params.setRconPassword(leRConPassword->text());
	params.setPort(spinPort->isEnabled() ? spinPort->value() : 0);
	params.setSkill(cboDifficulty->currentIndex());
	params.setUrl(leURL->text());

	const QList<GameMode>* gameModes = d->currentEngine->data()->gameModes;
	if (gameModes != NULL)
	{
		foreach (const GameMode& mode, (*gameModes))
		{
			if (mode.name().compare(cboGamemode->currentText()) == 0)
			{
				params.setGameMode(mode);
				break;
			}
		}
	}

	return true;
}

void CreateServerDialog::firstLoadConfigTimer()
{
	QString tmpServerCfgPath = gDefaultDataPaths->programsDataDirectoryPath() + TEMP_SERVER_CONFIG_FILENAME;

	QFileInfo fi(tmpServerCfgPath);
	if (fi.exists())
	{
		loadConfig(tmpServerCfgPath, true);
	}
}


void CreateServerDialog::focusChanged(QWidget* oldW, QWidget* newW)
{
	if (newW == leMapname)
	{
		btnAddMapToMaplist->setDefault(true);
	}
	else if (oldW == leMapname)
	{
		btnAddMapToMaplist->setDefault(false);
	}
}

void CreateServerDialog::initDMFlagsTabs()
{
	removeDMFlagsTabs();

	if (d->currentEngine->data()->createDMFlagsPagesAutomatic)
	{
		int paramsIndex = tabWidget->indexOf(tabCustomParameters);
		const QList<DMFlagsSection>* dmFlagsSec = d->currentEngine->data()->allDMFlags;
		if(dmFlagsSec == NULL || dmFlagsSec->empty())
		{
			return; // Nothing to do
		}

		const QList<DMFlagsSection>& dmFlagsSections = *dmFlagsSec;

		for (int i = 0; i < dmFlagsSections.count(); ++i)
		{
			DMFlagsTabWidget* dmftw = new DMFlagsTabWidget();

			QWidget* flagsTab = new QWidget(this);
			dmftw->widget = flagsTab;
			dmftw->section = dmFlagsSections[i];

			QHBoxLayout* hLayout = new QHBoxLayout(flagsTab);

			QVBoxLayout* layout = NULL;
			for (int j = 0; j < dmFlagsSections[i].count(); ++j)
			{
				if ((j % 16) == 0)
				{
					if (layout != NULL)
					{
						layout->addStretch();
					}

					layout = new QVBoxLayout();
					hLayout->addLayout(layout);
				}

				QCheckBox* checkBox = new QCheckBox();
				checkBox->setText(dmFlagsSections[i].name());
				dmftw->checkBoxes << checkBox;
				layout->addWidget(checkBox);
			}

			if (layout != NULL)
			{
				layout->addStretch();
			}

			d->dmFlagsTabs << dmftw;
			tabWidget->insertTab(paramsIndex++, flagsTab, dmFlagsSections[i].name());
		}
	}
}

void CreateServerDialog::initEngineSpecific(EnginePlugin* engineInfo)
{
	if (engineInfo == d->currentEngine || engineInfo == NULL)
	{
		return;
	}

	d->currentEngine = engineInfo;

	// Executable path
	Message message;

	if (d->bIsServerSetup)
	{
		// TODO: something weird, perhaps just refer to offline executable here?
		ServerPtr server = d->currentEngine->server(QHostAddress("127.0.0.1"), 1);
		leExecutable->setText(pathToClientExe(server.data(), message));
	}
	else
	{
		leExecutable->setText(pathToServerExe(message));
	}

	if (message.isError() && !d->bSuppressMissingExeErrors)
	{
		QString caption = tr("Doomseeker - error obtaining server binary");
		QString error = tr("Server binary for engine \"%1\" cannot be obtained.\nFollowing error has occured:\n%2").arg(engineInfo->data()->name, message.contents());

		QMessageBox::warning(NULL, caption, error);
	}

	spinPort->setValue(d->currentEngine->data()->defaultServerPort);

	cboGamemode->clear();

	const QList<GameMode>* gameModes = d->currentEngine->data()->gameModes;
	if (gameModes != NULL)
	{
		for (int i = 0; i < gameModes->count(); ++i)
		{
			cboGamemode->addItem((*gameModes)[i].name(), i);
		}
	}
	else
	{
		// No game modes defined for plugin so lets clear the rules page.
		removeLimitWidgets();
	}

	initDMFlagsTabs();
	initEngineSpecificPages(engineInfo);
	initInfoAndPassword();
	initRules();
}

void CreateServerDialog::initEngineSpecificPages(EnginePlugin* engineInfo)
{
	// First, get rid of the original pages.
	foreach (CreateServerDialogPage* page, d->currentCustomPages)
	{
		delete page;
	}
	d->currentCustomPages.clear();

	// Add new custom pages to the dialog.
	d->currentCustomPages = engineInfo->createServerDialogPages(this);
	foreach (CreateServerDialogPage* page, d->currentCustomPages)
	{
		int idxInsertAt = tabWidget->indexOf(tabCustomParameters);
		tabWidget->insertTab(idxInsertAt, page, page->name());
	}
}

void CreateServerDialog::initGamemodeSpecific(const GameMode& gameMode)
{
	// Rules tab
	removeLimitWidgets();
	QList<GameCVar> limits = d->currentEngine->limits(gameMode);
	QList<GameCVar>::iterator it;

	int number = 0;
	for (it = limits.begin(); it != limits.end(); ++it, ++number)
	{
		QLabel* label = new QLabel(this);
		label->setText(it->name());
		QSpinBox* spinBox = new QSpinBox(this);
		spinBox->setMaximum(999999);

		limitsLayout->addRow(label, spinBox);

		GameLimitWidget* glw = new GameLimitWidget();
		glw->label = label;
		glw->spinBox = spinBox;
		glw->limit = (*it);
		d->limitWidgets << glw;
	}
}

void CreateServerDialog::initInfoAndPassword()
{
	const static int MISC_TAB_INDEX = 2;

	bool bAtLeastOneVisible = false;
	bool bIsVisible = false;

	bIsVisible = d->currentEngine->data()->allowsConnectPassword;
	labelConnectPassword->setVisible(bIsVisible);
	leConnectPassword->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	bIsVisible = d->currentEngine->data()->allowsEmail;
	labelEmail->setVisible(bIsVisible);
	leEmail->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	bIsVisible = d->currentEngine->data()->allowsJoinPassword;
	labelJoinPassword->setVisible(bIsVisible);
	leJoinPassword->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	bIsVisible = d->currentEngine->data()->allowsMOTD;
	labelMOTD->setVisible(bIsVisible);
	pteMOTD->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	bIsVisible = d->currentEngine->data()->allowsRConPassword;
	labelRConPassword->setVisible(bIsVisible);
	leRConPassword->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	bIsVisible = d->currentEngine->data()->allowsURL;
	labelURL->setVisible(bIsVisible);
	leURL->setVisible(bIsVisible);
	bAtLeastOneVisible = bAtLeastOneVisible || bIsVisible;

	tabWidget->setTabEnabled(MISC_TAB_INDEX, bAtLeastOneVisible);
}

void CreateServerDialog::initPrimary()
{
	cboEngine->clear();

	for (unsigned i = 0; i < gPlugins->numPlugins(); ++i)
	{
		const EnginePlugin* plugin = gPlugins->info(i);
		cboEngine->addItem(plugin->icon(), plugin->data()->name, i);
	}

	if (cboEngine->count() > 0)
	{
		cboEngine->setCurrentIndex(0);
	}

	cboDifficulty->clear();

	cboDifficulty->addItem("1 - I'm too young to die", 0);
	cboDifficulty->addItem("2 - Hey, not too rough", 1);
	cboDifficulty->addItem("3 - Hurt me plenty", 2);
	cboDifficulty->addItem("4 - Ultra-violence", 3);
	cboDifficulty->addItem("5 - NIGHTMARE!", 4);

	const QString iwads[] = { "doom.wad", "doom1.wad", "doom2.wad", "tnt.wad", "plutonia.wad", "heretic.wad", "hexen.wad", "hexdd.wad", "freedoom.wad", "strife1.wad", "" };

	cboIwad->clear();

	for (int i = 0; !iwads[i].isEmpty(); ++i)
	{
		PathFinder pathFinder;
		QString path = pathFinder.findFile(iwads[i]);
		if (!path.isEmpty())
		{
			cboIwad->addItem(path);
		}
	}
}

void CreateServerDialog::initRules()
{
	cbRandomMapRotation->setVisible(d->currentEngine->data()->supportsRandomMapRotation);

	cboModifier->clear();
	d->gameModifiers.clear();

	const QList<GameCVar>* pEngineGameModifiers = d->currentEngine->data()->gameModifiers;

	if (pEngineGameModifiers != NULL && !pEngineGameModifiers->isEmpty())
	{
		const QList<GameCVar>& engineGameModifiers = *pEngineGameModifiers;

		cboModifier->show();
		labelModifiers->show();

		cboModifier->addItem(tr("< NONE >"));

		for (int i = 0; i < engineGameModifiers.count(); ++i)
		{
			cboModifier->addItem(engineGameModifiers[i].name());
			d->gameModifiers << engineGameModifiers[i];
		}
	}
	else
	{
		cboModifier->hide();
		labelModifiers->hide();
	}
}

bool CreateServerDialog::loadConfig(const QString& filename, bool loadingPrevious)
{
	QAbstractItemModel* model;
	QStringList stringList;
	QSettings settingsFile(filename, QSettings::IniFormat);
	SettingsProviderQt settingsProvider(&settingsFile);
	Ini ini(&settingsProvider);
	IniSection general = ini.section("General");
	IniSection rules = ini.section("Rules");
	IniSection misc = ini.section("Misc");
	IniSection dmflags = ini.section("DMFlags");

	// General
	if (!d->bIsServerSetup)
	{
		QString engineName = general["engine"];
		const EnginePlugin* prevEngine = d->currentEngine;
		if(!setEngine(engineName))
			return false;

		bool bChangeExecutable = (prevEngine != d->currentEngine || !cbLockExecutable->isChecked());

		// First let's check if we can use executable stored in the server's config.
		// We will save the path to this executable in a local variable.
		QString executablePath = "";
		if (bChangeExecutable)
		{
			executablePath = *general["executable"];
			QFileInfo fileInfo(executablePath);
			if (!fileInfo.exists())
			{
				// Executable cannot be found, display error message and reset
				// the local variable.
				QMessageBox::warning(NULL, tr("Doomseeker - load server config"), tr("Game executable saved in config cannot be found.\nDefault executable will be used."));
				executablePath = "";
			}
		}

		// If we successfuly retrieved path from the config we shall
		// set this path in the line edit control.
		if (!executablePath.isEmpty())
		{
			leExecutable->setText(executablePath);
		}
	}

	leServername->setText(general["name"]);
	spinPort->setValue(general["port"]);
	cboGamemode->setCurrentIndex(general["gamemode"]);
	leMap->setText(general["map"]);
	if (!(loadingPrevious && d->iwadSetExplicitly))
	{
		addIwad(general["iwad"]);
	}

	stringList = general["pwads"].valueString().split(";");
	model = lstAdditionalFiles->model();
	model->removeRows(0, model->rowCount());
	foreach (QString s, stringList)
	{
		addWadPath(s);
	}

	cbBroadcastToLAN->setChecked(general["broadcastToLAN"]);
	cbBroadcastToMaster->setChecked(general["broadcastToMaster"]);

	// Rules
	cboDifficulty->setCurrentIndex(rules["difficulty"]);
	cboModifier->setCurrentIndex(rules["modifier"]);
	spinMaxClients->setValue(rules["maxClients"]);
	spinMaxPlayers->setValue(rules["maxPlayers"]);

	foreach (GameLimitWidget* widget, d->limitWidgets)
	{
		widget->spinBox->setValue(rules[widget->limit.command()]);
	}

	stringList = rules["maplist"].valueString().split(";");
	model = lstMaplist->model();
	model->removeRows(0, model->rowCount());
	foreach(QString s, stringList)
	{
		addMapToMaplist(s);
	}
	cbRandomMapRotation->setChecked(rules["randomMapRotation"]);

	// Misc.
	leURL->setText(misc["URL"]);
	leEmail->setText(misc["eMail"]);
	leConnectPassword->setText(misc["connectPassword"]);
	leJoinPassword->setText(misc["joinPassword"]);
	leRConPassword->setText(misc["RConPassword"]);
	pteMOTD->document()->setPlainText(misc["MOTD"]);

	// DMFlags
	foreach(DMFlagsTabWidget* p, d->dmFlagsTabs)
	{
		for (int i = 0; i < p->section.count(); ++i)
		{
			QRegExp re("[^a-zA-Z]");
			QString name1 = p->section.name();
			QString name2 = p->section[i].name();
			name1 = name1.remove(re);
			name2 = name2.remove(re);
			p->checkBoxes[i]->setChecked(dmflags[name1 + name2]);
		}
	}

	// Custom pages.
	foreach (CreateServerDialogPage* page, d->currentCustomPages)
	{
		page->loadConfig(ini);
	}

	// Custom parameters
	pteCustomParameters->document()->setPlainText(misc["CustomParams"]);
	return true;
}

void CreateServerDialog::lstAdditionalFilesPathDnd(const QString& path)
{
	QFileInfo fileInfo(path);
	if (fileInfo.isFile())
	{
		addWadPath(path);
	}
}

void CreateServerDialog::makeSetupServerDialog(const EnginePlugin *plugin)
{
	d->bSuppressMissingExeErrors = true;
	d->bIsServerSetup = true;
	setEngine(plugin->data()->name);

	cbAllowTheGameToChoosePort->hide();
	btnCommandLine->hide();

	// Disable some stuff
	QWidget *disableControls[] =
	{
		cboEngine, leExecutable, btnBrowseExecutable, btnDefaultExecutable,
		cbLockExecutable, leServername, spinPort, cbBroadcastToLAN,
		cbBroadcastToMaster, btnPlayOffline, spinMaxClients, spinMaxPlayers,

		NULL
	};
	for(int i = 0;disableControls[i] != NULL;++i)
		disableControls[i]->setDisabled(true);
}

QString CreateServerDialog::pathToClientExe(Server* server, Message& message)
{
	QScopedPointer<ExeFile> f(server->clientExe());
	return f->pathToExe(message);
}

QString CreateServerDialog::pathToOfflineExe(Message& message)
{
	return GameExeRetriever(*d->currentEngine->gameExe()).pathToOfflineExe(message);
}

QString CreateServerDialog::pathToServerExe(Message& message)
{
	return GameExeRetriever(*d->currentEngine->gameExe()).pathToServerExe(message);
}

void CreateServerDialog::removeDMFlagsTabs()
{
	foreach (DMFlagsTabWidget* flags, d->dmFlagsTabs)
	{
		int index = tabWidget->indexOf(flags->widget);
		tabWidget->removeTab(index);
		delete flags->widget;
		delete flags;
	}

	d->dmFlagsTabs.clear();
}

void CreateServerDialog::removeLimitWidgets()
{
	QList<GameLimitWidget*>::iterator it;
	foreach (GameLimitWidget *widget, d->limitWidgets)
	{
		delete widget->label;
		delete widget->spinBox;
		delete widget;
	}

	d->limitWidgets.clear();
}

void CreateServerDialog::runGame(bool offline)
{
	const QString errorCapt = tr("Doomseeker - create server");
	if (d->currentEngine == NULL)
	{
		QMessageBox::critical(NULL, errorCapt, tr("No engine selected"));
		return;
	}

	GameCreateParams gameParams;
	if (createHostInfo(gameParams, offline))
	{
		QString error;

		GameHost* gameRunner = d->currentEngine->gameHost();
		Message message = gameRunner->host(gameParams);

		delete gameRunner;

		if (message.isError())
		{
			QMessageBox::critical(NULL, tr("Doomseeker - error"), message.contents());
		}
		else
		{
			QString tmpServerConfigPath = gDefaultDataPaths->programsDataDirectoryPath() + TEMP_SERVER_CONFIG_FILENAME;
			saveConfig(tmpServerConfigPath);
		}
	}
}

bool CreateServerDialog::saveConfig(const QString& filename)
{
	QStringList stringList;
	QSettings settingsFile(filename, QSettings::IniFormat);
	SettingsProviderQt settingsProvider(&settingsFile);
	Ini ini(&settingsProvider);
	IniSection general = ini.createSection("General");
	IniSection rules = ini.createSection("Rules");
	IniSection misc = ini.createSection("Misc");
	IniSection dmflags = ini.createSection("DMFlags");

	// General
	general["engine"] = cboEngine->currentText();
	general["executable"] = leExecutable->text();
	general["name"] = leServername->text();
	general["port"] = spinPort->value();
	general["gamemode"] = cboGamemode->currentIndex();
	general["map"] = leMap->text();
	general["iwad"] = cboIwad->currentText();

	stringList = CommonGUI::listViewStandardItemsToStringList(lstAdditionalFiles);
	general["pwads"] = stringList.join(";");

	general["broadcastToLAN"] = cbBroadcastToLAN->isChecked();
	general["broadcastToMaster"] = cbBroadcastToMaster->isChecked();

	// Rules
	rules["difficulty"] = cboDifficulty->currentIndex();
	rules["modifier"] = cboModifier->currentIndex();
	rules["maxClients"] = spinMaxClients->value();
	rules["maxPlayers"] = spinMaxPlayers->value();

	foreach (GameLimitWidget *widget, d->limitWidgets)
	{
		rules[widget->limit.command()] = widget->spinBox->value();
	}

	stringList = CommonGUI::listViewStandardItemsToStringList(lstMaplist);
	rules["maplist"] = stringList.join(";");
	rules["randomMapRotation"] = cbRandomMapRotation->isChecked();

	// Misc.
	misc["URL"] = leURL->text();
	misc["eMail"] = leEmail->text();
	misc["connectPassword"] = leConnectPassword->text();
	misc["joinPassword"] = leJoinPassword->text();
	misc["RConPassword"] = leRConPassword->text();
	misc["MOTD"] = pteMOTD->toPlainText();

	// DMFlags
	foreach(DMFlagsTabWidget* p, d->dmFlagsTabs)
	{
		for (int i = 0; i < p->section.count(); ++i)
		{
			QRegExp re("[^a-zA-Z]");
			QString name1 = p->section.name();
			QString name2 = p->section[i].name();
			name1 = name1.remove(re);
			name2 = name2.remove(re);
			dmflags[name1 + name2] = p->checkBoxes[i]->isChecked();
		}
	}

	// Custom pages.
	foreach (CreateServerDialogPage* page, d->currentCustomPages)
	{
		page->saveConfig(ini);
	}

	// Custom parameters
	misc["CustomParams"] = pteCustomParameters->toPlainText();

	if (settingsFile.isWritable())
	{
		settingsFile.sync();
		return true;
	}
	return false;
}

bool CreateServerDialog::setEngine(const QString &engineName)
{
	int engIndex = gPlugins->pluginIndexFromName(engineName);
	if (engIndex < 0)
	{
		QMessageBox::critical(NULL, tr("Doomseeker - load server config"), tr("Plugin for engine \"%1\" is not present!").arg(engineName));
		return false;
	}

	// Select engine. This will also select the default executable path.
	cboEngine->setCurrentIndex(engIndex);
	return true;
}

void CreateServerDialog::setIwadByName(const QString &iwad)
{
	d->iwadSetExplicitly = true;
	for (int i = 0; i < cboIwad->count(); ++i)
	{
		QFileInfo fi(cboIwad->itemText(i));
		if (fi.fileName().compare(iwad, Qt::CaseInsensitive) == 0)
		{
			cboIwad->setCurrentIndex(i);
			return;
		}
	}
	// If IWAD with given name isn't present on the list try to find it anyway.
	PathFinder pathFinder;
	QString path = pathFinder.findFile(iwad);
	if (!path.isEmpty())
	{
		cboIwad->addItem(path);
		cboIwad->setCurrentIndex(cboIwad->count() - 1);
	}
}
