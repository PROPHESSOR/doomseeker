//------------------------------------------------------------------------------
// mainwindow.cpp
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
// Copyright (C) 2009 "Zalewa" <zalewapl@gmail.com>
// Copyright (C) 2018 PROPHESSOR
//------------------------------------------------------------------------------
#include "configuration/doomseekerconfig.h"
#include "gui/configuration/irc/ircconfigurationdialog.h"
#include "gui/configuration/doomseekerconfigurationdialog.h"
#include "gui/configuration/gameconfig.h"
#include "gui/helpers/playersdiagram.h"
#include "gui/irc/ircsounds.h"
#include "gui/widgets/serversstatuswidget.h"
#include "gui/aboutdialog.h"
#include "gui/copytextdlg.h"
#include "gui/createserverdialog.h"
#include "gui/demomanager.h"
#include "gui/dockBuddiesList.h"
#include "gui/ip2cupdatebox.h"
#include "gui/mainwindow.h"
#include "gui/passwordDlg.h"
#include "gui/playofflinedialog.h"
#include "gui/serverdetailsdock.h"
#include "gui/serverfilterdock.h"
#include "gui/wadseekerinterface.h"
#include "gui/wadseekershow.h"
#include "ip2c/ip2cloader.h"
#include "irc/configuration/ircconfig.h"
#include "pathfinder/pathfinder.h"
#include "plugins/engineplugin.h"
#include "refresher/refresher.h"
#include "serverapi/gameclientrunner.h"
#include "serverapi/message.h"
#include "serverapi/server.h"
#include "updater/autoupdater.h"
#include "updater/updatechannel.h"
#include "updater/updateinstaller.h"
#include "updater/updatepackage.h"
#include "commandline.h"
#include "connectionhandler.h"
#include "customservers.h"
#include "doomseekerfilepaths.h"
#include "fileutils.h"
#include "joincommandlinebuilder.h"
#include "log.h"
#include "main.h"
#include "strings.h"
#include <cassert>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QFileInfo>
#include <QIcon>
#include <QHeaderView>
#include <QMessageBox>
#include <QToolBar>
#include <QSizePolicy>

const QString MainWindow::HELP_SITE_URL = "http://doomseeker.drdteam.org/help";

MainWindow::MainWindow(QApplication* application, int argc, char** argv)
    : bTotalRefreshInProcess(false), buddiesList(NULL), bWasMaximized(false),
      bWantToQuit(false), logDock(NULL), masterManager(NULL),
      trayIcon(NULL), trayIconMenu(NULL) {
    autoUpdater = NULL;
    mainDock = NULL;
    connectionHandler = NULL;
    updateChannelOnUpdateStart = new UpdateChannel();
    updaterInstallerErrorCode = 0;

    this->application = application;

    this->setAttribute(Qt::WA_DeleteOnClose, true);
    setupUi(this);
    setupIcons();

    initAutoUpdaterWidgets();

    updatesConfirmationWidget->hide();
    updatesDownloadedWidget->hide();

    // Hide menu options which aren't supported on target platform.
#ifndef WITH_AUTOUPDATES
    menuActionCheckForUpdates->setVisible(false);
#endif

    if (gPlugins->numPlugins() == 0) {
        QString message = tr(
                              "Warning: \n"
                              "Doomseeker failed to detect any plugins.\n"
                              "While the core application will still work its functionality is going to be limited.\n"
                              "One of the proper locations for plugin modules is the \"engines/\" directory.\n"
                          );
        gLog << message;
        QMessageBox::critical(NULL, tr("Doomseeker - plugin load failure"), message);
    }

    initIP2CUpdater();

    // The buddies list must always be available so we can perform certain operations on it
    buddiesList = new DockBuddiesList(this);
    menuView->addAction(buddiesList->toggleViewAction());
    buddiesList->toggleViewAction()->setText(tr("&Buddies"));
    buddiesList->toggleViewAction()->setShortcut(tr("CTRL+B"));

    connect(buddiesList, SIGNAL( joinServer(ServerPtr) ), this, SLOT( runGame(ServerPtr) ));
    buddiesList->hide();
    this->addDockWidget(Qt::LeftDockWidgetArea, buddiesList);
    initLogDock();
    initIRCDock();
    initServerFilterDock();
    initMainDock();
    splitDockWidget(mainDock, serverFilterDock, Qt::Horizontal);

    // Spawn Server Table Handler.
    serverTableHandler = new ServerListHandler(tableServers, this);
    connectEntities();

    initServerDetailsDock();
    tabifyDockWidget(ircDock, detailsDock);

    // Calculate screen center.
    int screenWidth = QApplication::desktop()->width();
    int screenHeight = QApplication::desktop()->height();

    // Window geometry settings
    if (!gConfig.doomseeker.areMainWindowSizeSettingsValid(screenWidth, screenHeight)) {
        gConfig.doomseeker.mainWindowX = (screenWidth - width()) / 2;
        gConfig.doomseeker.mainWindowY = (screenHeight - height()) / 2;
        gConfig.doomseeker.mainWindowWidth = width();
        gConfig.doomseeker.mainWindowHeight = height();
    } else if (gConfig.doomseeker.mainWindowY < 0) {
        // Do not allow y values less than zero anyway.
        // We do not want to loose the titlebar.
        gConfig.doomseeker.mainWindowY = 0;
    }

    move(gConfig.doomseeker.mainWindowX, gConfig.doomseeker.mainWindowY);
    resize(gConfig.doomseeker.mainWindowWidth, gConfig.doomseeker.mainWindowHeight);

    restoreState(QByteArray::fromBase64(gConfig.doomseeker.mainWindowState.toAscii()));

    // Restore checked states.
    menuActionRecordDemo->setChecked(gConfig.doomseeker.bRecordDemo);

    // Get the master
    masterManager = new MasterManager();
    buddiesList->scan(masterManager);
    connect(masterManager, SIGNAL( masterMessage(MasterClient*, const QString&, const QString&, bool) ),
            this, SLOT( masterManagerMessages(MasterClient*, const QString&, const QString&, bool) ) );
    connect(masterManager, SIGNAL( masterMessageImportant(MasterClient*, const Message&) ),
            this, SLOT( masterManagerMessagesImportant(MasterClient*, const Message&) ));

    // Allow us to enable and disable ports.
    fillQueryMenu(masterManager);

    // Init custom servers
    masterManager->customServs()->readConfig(serverTableHandler,
            SLOT(serverUpdated(ServerPtr, int)),
            SLOT(serverBegunRefreshing(ServerPtr)) );

    setWindowIcon(QIcon(":/icon.png"));

    // Auto refresh timer
    initAutoRefreshTimer();
    connect(&autoRefreshTimer, SIGNAL( timeout() ), this, SLOT( autoRefreshTimer_timeout() ));

    // Tray icon
    initTrayIcon();

    setupToolBar();

    // Player diagram styles
    int slotStyle = gConfig.doomseeker.slotStyle;
    PlayersDiagram::loadImages(slotStyle);

    // IP2C
    ip2cLoader = new IP2CLoader();
    connectIP2CLoader(ip2cLoader);
    ip2cLoader->load();

    // Start first refresh from a timer. We want the main window fully
    // set up before refresh.
    QTimer::singleShot(1, this, SLOT(postInitAppStartup()));
}

MainWindow::~MainWindow() {
    if (updateChannelOnUpdateStart != NULL) {
        delete updateChannelOnUpdateStart;
    }
    if (autoUpdater != NULL) {
        autoUpdater->disconnect();
        delete autoUpdater;
    }
    if(connectionHandler)
        delete connectionHandler;

    // Window geometry settings
    gConfig.doomseeker.bMainWindowMaximized = isMaximized();
    if (!isMaximized() && !isMinimized()) {
        gConfig.doomseeker.mainWindowX = x();
        gConfig.doomseeker.mainWindowY = y();
        gConfig.doomseeker.mainWindowWidth = width();
        gConfig.doomseeker.mainWindowHeight = height();
    }

    gConfig.doomseeker.mainWindowState = saveState().toBase64();

    QList<QAction*> menuQueryActions = menuQuery->actions();
    QList<QAction*>::iterator it;
    for (it = menuQueryActions.begin(); it != menuQueryActions.end(); ++it) {
        QAction* action = *it;
        QString pluginName = action->text();

        if (!pluginName.isEmpty()) {
            IniSection pluginConfig = gConfig.iniSectionForPlugin(pluginName);
            pluginConfig["Query"] = action->isChecked();
        }
    }

    if (trayIcon != NULL) {
        trayIcon->setVisible(false);
        delete trayIcon;
        trayIcon = NULL;
    }

    if (trayIconMenu != NULL) {
        delete trayIconMenu;
        trayIconMenu = NULL;
    }

    delete serverTableHandler;

    if(masterManager != NULL) {
        delete masterManager;
    }

    if (ip2cLoader != NULL) {
        delete ip2cLoader;
    }
}

void MainWindow::abortAutoUpdater() {
    if (autoUpdater != NULL) {
        autoUpdater->abort();
    }
}

void MainWindow::autoRefreshTimer_timeout() {
    if (gConfig.doomseeker.bQueryAutoRefreshDontIfActive && !isMinimized()) {
        if (QApplication::activeWindow() != 0) {
            return;
        }
    }

    getServers();
}

void MainWindow::blockRefreshButtons() {
    toolBarGetServers->setEnabled(false);
    toolBarRefreshAll->setEnabled(false);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ActivationChange && isActiveWindow() && !isMinimized() && !isHidden()) {
        serverTableHandler->cleanUp();
        event->accept();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::checkForUpdates(bool bUserTriggered) {
    if (autoUpdater != NULL) {
        if (autoUpdater->isRunning()) {
            QMessageBox::warning(this, tr("Doomseeker - Auto Update"),
                                 tr("Update is already in progress."));
            return;
        } else {
            delete autoUpdater;
            autoUpdater = NULL;
        }
    }
    gLog << tr("Removing old update packages from local temporary space.");
    QStringList removeFilter(QString("%1*").arg(DataPaths::UPDATE_PACKAGE_FILENAME_PREFIX));
    FileUtils::rmAllFiles(DoomseekerFilePaths::updatePackagesStorageDir(),
                          removeFilter);

    gLog << tr("Checking for updates...");
    autoUpdater = new AutoUpdater();
    this->connect(autoUpdater, SIGNAL(finished()),
                  SLOT(onAutoUpdaterFinish()));
    this->connect(autoUpdater, SIGNAL(downloadAndInstallConfirmationRequested()),
                  SLOT(onAutoUpdaterDownloadAndInstallConfirmationRequest()));
    this->connect(autoUpdater, SIGNAL(overallProgress(int, int, const QString&)),
                  SLOT(onAutoUpdaterOverallProgress(int, int, const QString&)));
    this->connect(autoUpdater, SIGNAL(packageDownloadProgress(qint64, qint64)),
                  SLOT(onAutoUpdaterFileProgress(qint64, qint64)));

    QMap<QString, QList<unsigned long long> > ignoredPackagesRevisions;
    if (!bUserTriggered) {
        foreach (const QString& package, gConfig.autoUpdates.lastKnownUpdateRevisions.keys()) {
            unsigned long long revision = gConfig.autoUpdates.lastKnownUpdateRevisions[package];
            QList<unsigned long long> list;
            list << revision;
            ignoredPackagesRevisions.insert(package, list);
        }
    }
    autoUpdater->setIgnoreRevisions(ignoredPackagesRevisions);

    UpdateChannel channel = UpdateChannel::fromName(gConfig.autoUpdates.updateChannelName);
    autoUpdater->setChannel(channel);
    *updateChannelOnUpdateStart = channel;

    bool bRequireConfirmation = true;
    if (!bUserTriggered) {
        bRequireConfirmation = (gConfig.autoUpdates.updateMode
                                != DoomseekerConfig::AutoUpdates::UM_FullAuto);
    }
    autoUpdater->setRequireDownloadAndInstallConfirmation(bRequireConfirmation);
    autoUpdaterStatusBarWidget->show();
    autoUpdater->start();
}


void MainWindow::checkForUpdatesAuto() {
    const bool bUserTriggered = true;
    checkForUpdates(!bUserTriggered);
}

void MainWindow::checkForUpdatesUserTriggered() {
    const bool bUserTriggered = true;
    checkForUpdates(bUserTriggered);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Check if tray icon is available and if we want to minimize to tray icon
    // when 'X' button is pressed. Real quit requests are handled by
    // quitProgram() method. This method sets bWantToQuit to true.
    if (trayIcon != NULL && gConfig.doomseeker.bCloseToTrayIcon && !bWantToQuit) {
        bWasMaximized = isMaximized();
        event->ignore();
        hide();
    } else {
        event->accept();
    }
}

void MainWindow::confirmUpdateInstallation() {
    assert(autoUpdater != NULL && "MainWindow::confirmUpdateInstallation()");
    updatesConfirmationWidget->hide();
    autoUpdater->confirmDownloadAndInstall();
}

void MainWindow::connectIP2CLoader(IP2CLoader* loader) {
    this->connect(ip2cLoader, SIGNAL( finished() ), SLOT( ip2cJobsFinished()));
    this->connect(ip2cLoader, SIGNAL( downloadProgress(qint64, qint64) ),
                  SLOT( ip2cDownloadProgress(qint64, qint64) ) );
}

void MainWindow::discardUpdates() {
    assert(autoUpdater != NULL && "MainWindow::confirmUpdateInstallation()");
    updatesConfirmationWidget->hide();
    // User rejected this update so let's add the packages
    // to the ignore list so user won't be nagged again.
    const QList<UpdatePackage>& pkgList = autoUpdater->newUpdatePackages();
    foreach (const UpdatePackage& pkg, pkgList) {
        gConfig.autoUpdates.lastKnownUpdateRevisions.insert(pkg.name, pkg.revision);
    }
    autoUpdater->abort();
}

void MainWindow::connectEntities() {
    // Connect refreshing thread.
    connect(gRefresher, SIGNAL( block() ), this, SLOT( blockRefreshButtons() ) );
    connect(gRefresher, SIGNAL( finishedQueryingMaster(MasterClient*) ), this, SLOT( finishedQueryingMaster(MasterClient*) ) );
    connect(gRefresher, SIGNAL( sleepingModeEnter() ), this, SLOT( refreshThreadEndsWork() ) );
    connect(gRefresher, SIGNAL( sleepingModeEnter() ), buddiesList, SLOT( scan() ) );
    connect(gRefresher, SIGNAL( sleepingModeExit() ), this, SLOT( refreshThreadBeginsWork() ) );

    // Controls
    connect(menuActionAbout, SIGNAL( triggered() ), this, SLOT( menuHelpAbout() ));
    connect(menuActionAboutQt, SIGNAL( triggered() ), application, SLOT( aboutQt() ));
    connect(menuActionBuddies, SIGNAL( triggered() ), this, SLOT( menuBuddies() ));
    connect(menuActionConfigure, SIGNAL( triggered() ), this, SLOT( menuOptionsConfigure() ));
    connect(menuActionCreateServer, SIGNAL( triggered() ), this, SLOT( menuCreateServer() ));
    connect(menuActionHelp, SIGNAL( triggered() ), this, SLOT ( menuHelpHelp() ) );
    connect(menuActionIRCOptions, SIGNAL( triggered() ), this, SLOT( menuIRCOptions() ) );
    connect(menuActionGameOptions, SIGNAL( triggered() ), this, SLOT( menuGameOptions() ) );
    connect(menuActionLog, SIGNAL( triggered() ), this, SLOT( menuLog() ));
    connect(menuActionManageDemos, SIGNAL( triggered() ), this, SLOT( menuManageDemos() ) );
    connect(menuActionPlayOffline, SIGNAL( triggered() ), this, SLOT( menuPlayOffline() ));
    connect(menuActionRecordDemo, SIGNAL( triggered() ), this, SLOT( menuRecordDemo() ) );
    connect(menuActionUpdateIP2C, SIGNAL( triggered() ), this, SLOT( menuUpdateIP2C() ) );
    connect(menuActionQuit, SIGNAL( triggered() ), this, SLOT( quitProgram() ));
    connect(menuActionViewIRC, SIGNAL( triggered() ) , this, SLOT( menuViewIRC() ));
    connect(menuActionWadseeker, SIGNAL( triggered() ), this, SLOT( menuWadSeeker() ));
    connect(serverFilterDock, SIGNAL(filterUpdated(const ServerListFilterInfo&)),
            this, SLOT(updateServerFilter(const ServerListFilterInfo&)) );
    connect(serverFilterDock, SIGNAL(nonEmptyServerGroupingAtTopToggled(bool)),
            serverTableHandler, SLOT(setGroupServersWithPlayersAtTop(bool)) );
    connect(serverTableHandler, SIGNAL(serverFilterModified(ServerListFilterInfo)),
            serverFilterDock, SLOT(setFilterInfo(ServerListFilterInfo)));
    connect(serverTableHandler, SIGNAL( serverDoubleClicked(ServerPtr) ), this, SLOT( runGame(ServerPtr) ) );
    connect(serverTableHandler, SIGNAL( displayServerJoinCommandLine(ServerPtr) ), this, SLOT( showServerJoinCommandLine(ServerPtr) ) );
    connect(serverTableHandler, SIGNAL( serverInfoUpdated(ServerPtr) ), this, SLOT( serverAddedToList(ServerPtr) ) );
}

void MainWindow::fillQueryMenu(MasterManager* masterManager) {
    // This is called only once from the constructor. No clears to
    // queryMenuPorts are ever performed. Not even in the destructor.
    for(unsigned i = 0; i < gPlugins->numPlugins(); ++i) {
        const EnginePlugin* plugin = gPlugins->info(i);
        if(!plugin->data()->hasMasterServer) {
            continue;
        }

        MasterClient* pMasterClient = plugin->masterClient();
        pMasterClient->updateAddress();
        masterManager->addMaster(pMasterClient);

        // Now is a good time to also populate the status bar widgets
        ServersStatusWidget *statusWidget = new ServersStatusWidget(plugin->icon(), pMasterClient);
        serversStatusesWidgets.insert(pMasterClient, statusWidget);

        this->connect(statusWidget, SIGNAL( clicked(MasterClient*) ) ,
                      SLOT( toggleMasterClientEnabled(MasterClient*) ) );

        statusBar()->addPermanentWidget(statusWidget);

        QString name = gPlugins->info(i)->data()->name;
        QQueryMenuAction* query = new QQueryMenuAction(pMasterClient, statusWidget, menuQuery);
        queryMenuPorts.insert(pMasterClient, query);

        menuQuery->addAction(query);

        query->setCheckable(true);
        query->setIcon(plugin->icon());
        query->setText(name);

        IniSection pluginConfig = gConfig.iniSectionForPlugin(name);

        if (!pluginConfig.retrieveSetting("Query").value().isNull()) {
            bool enabled = pluginConfig["Query"];
            setQueryMasterServerEnabled(pMasterClient, enabled);
        } else {
            // if no setting is found for this engine
            // set default to true:
            setQueryMasterServerEnabled(pMasterClient, true);
        }
    }
}

void MainWindow::finishConfiguration(DoomseekerConfigurationDialog &configDialog, bool lookupHostsChanged) {
    // In case the master server addresses changed, notify the master clients.
    updateMasterAddresses();

    // If appearance changed - update the widgets.
    if (configDialog.appearanceChanged()) {
        tableServers->setShowGrid(gConfig.doomseeker.bDrawGridInServerTable);
        serverTableHandler->redraw();
        initTrayIcon();
    }

    // Do the following only if setting changed from false to true.
    if (lookupHostsChanged) {
        serverTableHandler->lookupHosts();
    }

    // Refresh custom servers list:
    if (configDialog.customServersChanged()) {
        serverTableHandler->serverModel()->removeCustomServers();
        masterManager->customServs()->readConfig(serverTableHandler, SLOT(serverUpdated(ServerPtr, int)), SLOT(serverBegunRefreshing(ServerPtr)) );
        refreshCustomServers();
    }
}

void MainWindow::finishedQueryingMaster(MasterClient* master) {
    if (master == NULL) {
        return;
    }

    for(int i = 0; i < master->numServers(); i++) {
        connect((*master)[i].data(), SIGNAL(updated(ServerPtr, int)),
                serverTableHandler, SLOT(serverUpdated(ServerPtr, int)) );

        connect((*master)[i].data(), SIGNAL(begunRefreshing(ServerPtr)),
                serverTableHandler, SLOT(serverBegunRefreshing(ServerPtr)) );
    }
}

void MainWindow::getServers() {
    // Check if this operation has any sense.
    if (!isAnythingToRefresh()) {
        gLog << tr("Senseless refresh operation attempted.");

        QString message = tr("Doomseeker is unable to proceed with the refresh"
                             " operation because the following problem has occured:\n\n");

        if (gPlugins->numPlugins() == 0) {
            message += tr("Plugins are missing from the \"engines/\" directory.");
        } else if (!isAnyMasterEnabled()) {
            message += tr("No master servers are enabled in the \"Query\" menu.");
        } else {
            message += tr("Unknown error occured.");
        }

        gLog << message;
        QMessageBox::warning(this, tr("Doomseeker - senseless operation"), message);
        return;
    }

    bTotalRefreshInProcess = true;
    autoRefreshTimer.stop();
    gLog << tr("Total refresh process initialized!");
    serverTableHandler->clearTable();
    refreshCustomServers();

    bool bAtLeastOneEnabled = false;

    if (!isAnyMasterEnabled()) {
        gLog << tr("Warning: No master servers were enabled for this refresh. "
                   "Check your Query menu or \"engines/\" directory. Custom servers will still refresh.");
    }

    masterManager->clearServers();
    for (int i = 0; i < masterManager->numMasters(); ++i) {
        MasterClient* pMaster = (*masterManager)[i];

        if (pMaster->isEnabled()) {
            gRefresher->registerMaster(pMaster);
        }
    }
}

bool MainWindow::hasCustomServers() const {
    CustomServers* customServers = masterManager->customServs();
    return customServers->numServers() > 0;
}

void MainWindow::initAutoRefreshTimer() {
    const unsigned MIN_DELAY = 30;
    const unsigned MAX_DELAY = 3600;

    bool bEnabled = gConfig.doomseeker.bQueryAutoRefreshEnabled;

    if (!bEnabled) {
        autoRefreshTimer.stop();
    } else {
        // If delay value is out of bounds we should adjust
        // config value as well.
        unsigned& delay = gConfig.doomseeker.queryAutoRefreshEverySeconds;

        // Make sure delay is in given limit.
        if (delay < MIN_DELAY) {
            delay = MIN_DELAY;
        } else if (delay > MAX_DELAY) {
            delay = MAX_DELAY;
        }

        unsigned delayMs = delay * 1000;

        autoRefreshTimer.setSingleShot(false);
        autoRefreshTimer.start(delayMs);
    }
}

void MainWindow::initAutoUpdaterWidgets() {
    static const int FILE_BAR_WIDTH = 50;
    static const int OVERALL_BAR_WIDTH = 180;

    autoUpdaterStatusBarWidget = new QWidget(statusBar());
    autoUpdaterStatusBarWidget->setLayout(new QHBoxLayout(autoUpdaterStatusBarWidget));
    autoUpdaterStatusBarWidget->layout()->setContentsMargins(QMargins(0, 0, 0, 0));
    statusBar()->addPermanentWidget(autoUpdaterStatusBarWidget);
    autoUpdaterStatusBarWidget->hide();

    autoUpdaterLabel = new QLabel(autoUpdaterStatusBarWidget);
    autoUpdaterLabel->setText(tr("Auto Updater:"));
    autoUpdaterStatusBarWidget->layout()->addWidget(autoUpdaterLabel);

    autoUpdaterFileProgressBar = mkStdProgressBarForStatusBar();
    autoUpdaterFileProgressBar->setFormat("%p%");
    autoUpdaterFileProgressBar->setMaximumWidth(FILE_BAR_WIDTH);
    autoUpdaterFileProgressBar->setMinimumWidth(FILE_BAR_WIDTH);
    autoUpdaterStatusBarWidget->layout()->addWidget(autoUpdaterFileProgressBar);

    autoUpdaterOverallProgressBar = mkStdProgressBarForStatusBar();
    autoUpdaterOverallProgressBar->setMaximumWidth(OVERALL_BAR_WIDTH);
    autoUpdaterOverallProgressBar->setMinimumWidth(OVERALL_BAR_WIDTH);
    autoUpdaterStatusBarWidget->layout()->addWidget(autoUpdaterOverallProgressBar);

    autoUpdaterAbortButton = new QPushButton(statusBar());
    autoUpdaterAbortButton->setToolTip(tr("Abort update."));
    autoUpdaterAbortButton->setIcon(QIcon(":/icons/x.png"));
    this->connect(autoUpdaterAbortButton, SIGNAL(clicked()),
                  SLOT(abortAutoUpdater()));
    autoUpdaterStatusBarWidget->layout()->addWidget(autoUpdaterAbortButton);
}

void MainWindow::initIP2CUpdater() {
    static const int PROGRESSBAR_WIDTH = 220;

    ip2cUpdateProgressBar = mkStdProgressBarForStatusBar();
    ip2cUpdateProgressBar->setFormat(tr("IP2C Update"));
    ip2cUpdateProgressBar->hide();
    ip2cUpdateProgressBar->setMaximumWidth(PROGRESSBAR_WIDTH);
    ip2cUpdateProgressBar->setMinimumWidth(PROGRESSBAR_WIDTH);
    statusBar()->addPermanentWidget(ip2cUpdateProgressBar);
}

void MainWindow::initIRCDock() {
    ircDock = new IRCDock(this);
    menuView->addAction(ircDock->toggleViewAction());
    ircDock->toggleViewAction()->setText(tr("&IRC"));
    ircDock->toggleViewAction()->setShortcut(tr("CTRL+I"));
    ircDock->hide();
    this->addDockWidget(Qt::BottomDockWidgetArea, ircDock);

    if (gIRCConfig.isAutojoinNetworksEnabled()) {
        this->ircDock->setVisible(true);
        this->ircDock->performNetworkAutojoins();
    }
}

void MainWindow::initLogDock() {
    logDock = new LogDock(this);
    menuView->addAction(logDock->toggleViewAction());
    logDock->toggleViewAction()->setText(tr("&Log"));
    logDock->toggleViewAction()->setShortcut(tr("CTRL+L"));
    logDock->hide();
    this->addDockWidget(Qt::BottomDockWidgetArea, logDock);

    connect(&gLog, SIGNAL( newEntry(const QString&) ), logDock, SLOT( appendLogEntry(const QString&) ) );

    // Also add anything that already might be in the log to the box.
    logDock->appendLogEntry(gLog.content());
}

void MainWindow::initMainDock() {
    setDockNestingEnabled(true); // This line allows us to essentually treat a dock as a central widget.

    // Make a dock out of the central MainWindow widget and drop that widget
    // from the MainWindow itself.
    mainDock = new QDockWidget(tr("Servers"));
    mainDock->setTitleBarWidget(new QWidget(this));
    mainDock->setObjectName("ServerList");
    mainDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    mainDock->setWidget(centralWidget());
    setCentralWidget(0);
    addDockWidget(Qt::RightDockWidgetArea, mainDock);
}

void MainWindow::initServerDetailsDock() {
    detailsDock = new ServerDetailsDock(this);
    menuView->addAction(detailsDock->toggleViewAction());
    detailsDock->toggleViewAction()->setText(tr("Server &Details"));
    detailsDock->toggleViewAction()->setShortcut(tr("CTRL+D"));
    detailsDock->hide();
    addDockWidget(Qt::BottomDockWidgetArea, detailsDock);

    detailsDock->connect(serverTableHandler, SIGNAL( serversSelected(QList<ServerPtr>&) ), SLOT( displaySelection(QList<ServerPtr> &) ));
}

void MainWindow::initServerFilterDock() {
    serverFilterDock = new ServerFilterDock(this);

    menuView->addAction(serverFilterDock->toggleViewAction());
    serverFilterDock->hide();
    this->addDockWidget(Qt::RightDockWidgetArea, serverFilterDock);
}

void MainWindow::initTrayIcon() {
    bool isEnabled = gConfig.doomseeker.bUseTrayIcon;
    if (!isEnabled || !QSystemTrayIcon::isSystemTrayAvailable()) {
        if (trayIcon != NULL) {
            delete trayIcon;
            trayIcon = NULL;
        }

        if (trayIconMenu != NULL) {
            delete trayIconMenu;
            trayIconMenu = NULL;
        }
    } else if (trayIcon == NULL) {
        QAction* trayAction;
        trayIconMenu = new QMenu(this);
        trayAction = trayIconMenu->addAction("Exit");
        connect(trayAction, SIGNAL( triggered() ), this, SLOT( quitProgram() ) );

        // This should be automatically deleted when main window closes
        trayIcon = new QSystemTrayIcon(this);
        connect(trayIcon, SIGNAL( activated(QSystemTrayIcon::ActivationReason) ), this, SLOT( trayIcon_activated(QSystemTrayIcon::ActivationReason) ) );

        updateTrayIconTooltipAndLogTotalRefresh();

        trayIcon->setContextMenu(trayIconMenu);
        trayIcon->setIcon(QIcon(":/icon.png"));
        trayIcon->setVisible(true);
    }
}

void MainWindow::ip2cDownloadProgress(qint64 current, qint64 max) {
    ip2cUpdateProgressBar->setMaximum(max);
    ip2cUpdateProgressBar->setValue(current);
}

void MainWindow::ip2cJobsFinished() {
    menuActionUpdateIP2C->setEnabled(true);
    serverTableHandler->updateCountryFlags();
    ip2cUpdateProgressBar->hide();

    if (ip2cLoader != NULL) {
        delete ip2cLoader;
        ip2cLoader = NULL;
    }
}

void MainWindow::ip2cStartUpdate() {
    if (ip2cLoader != NULL) {
        // If update is currently in progress then prevent re-starting.
        return;
    }

    menuActionUpdateIP2C->setEnabled(false);
    ip2cUpdateProgressBar->show();

    ip2cLoader = new IP2CLoader();
    connectIP2CLoader(ip2cLoader);
    ip2cLoader->update();
}

bool MainWindow::isAnythingToRefresh() const {
    return hasCustomServers() || isAnyMasterEnabled();
}

bool MainWindow::isAnyMasterEnabled() const {
    for (int i = 0; i < masterManager->numMasters(); ++i) {
        MasterClient* pMaster = (*masterManager)[i];

        if (pMaster->isEnabled()) {
            return true;
        }
    }

    return false;
}

void MainWindow::masterManagerMessages(MasterClient* pSender, const QString& title, const QString& content, bool isError) {
    QString message = tr("Master server for %1: %2").arg(pSender->plugin()->data()->name).arg(content);

    if (isError) {
        message = tr("Error: %1").arg(message);
        statusBar()->showMessage(message);
    }

    gLog << message;
}

void MainWindow::masterManagerMessagesImportant(MasterClient* pSender, const Message& objMessage) {
    QString strFullMessage = tr("Master server for %1: %2")
                             .arg(pSender->plugin()->data()->name)
                             .arg(objMessage.contents());

    if (objMessage.isError()) {
        strFullMessage = "<font color=\"#ff0000\">" + strFullMessage + "</font>";
    }

    importantMessagesWidget->addMessage(strFullMessage, objMessage.timestamp());
}

void MainWindow::menuBuddies() {
    buddiesList->setVisible(!buddiesList->isVisible());
}

void MainWindow::menuCreateServer() {
    // This object will auto-delete on close.
    CreateServerDialog* dialog = new CreateServerDialog(NULL);
    dialog->setWindowIcon(this->windowIcon());
    dialog->show();
}

void MainWindow::menuPlayOffline() {
    // This object will auto-delete on close.
    PlayOfflineDialog* dialog = new PlayOfflineDialog(NULL);
    dialog->setWindowIcon(this->windowIcon());
    dialog->show();
}

void MainWindow::menuHelpAbout() {
    AboutDialog dlg(this);
    autoRefreshTimer.stop();
    dlg.exec();
    initAutoRefreshTimer();
}

void MainWindow::menuHelpHelp() {
    if (HELP_SITE_URL.isEmpty() || !Strings::isUrlSafe(HELP_SITE_URL)) {
        QMessageBox::critical(this, tr("Help error"), tr("No help found."), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    bool bSuccess = QDesktopServices::openUrl(HELP_SITE_URL);

    if (!bSuccess) {
        QMessageBox::critical(this, tr("Help error"), tr("Failed to open URL:\n%1").arg(HELP_SITE_URL), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
}

void MainWindow::menuIRCOptions() {
    IRCConfigurationDialog dialog(this);
    dialog.initOptionsList();
    dialog.exec();

    if (ircDock != NULL) {
        ircDock->applyAppearanceSettings();

        // This could probably be optimized to not re-read files from drive
        // if audio options didn't change but currently there are only two
        // files, so no harm should be done.
        ircDock->sounds().loadFromConfig();
    }
}

void MainWindow::menuGameOptions() {
    GameConfig dialog(NULL);
    dialog.exec();
}

void MainWindow::menuLog() {
    logDock->setVisible(!logDock->isVisible());
}

void MainWindow::menuManageDemos() {
    DemoManagerDlg dm;
    dm.exec();
}

void MainWindow::menuOptionsConfigure() {
    DoomseekerConfigurationDialog::openConfiguration();
}

void MainWindow::menuRecordDemo() {
    gConfig.doomseeker.bRecordDemo = menuActionRecordDemo->isChecked();
}

void MainWindow::menuUpdateIP2C() {
    IP2CUpdateBox updateBox(this);

    connect(&updateBox, SIGNAL( accepted() ), this, SLOT( ip2cStartUpdate() ) );

    updateBox.exec();
}

void MainWindow::menuViewIRC() {
    ircDock->setVisible(!ircDock->isVisible());
}

void MainWindow::menuWadSeeker() {
    if (gWadseekerShow->checkWadseekerValidity(this)) {
        WadseekerInterface *wadseeker = WadseekerInterface::create(NULL);
        wadseeker->setAttribute(Qt::WA_DeleteOnClose);
        wadseeker->show();
    }
}

QProgressBar* MainWindow::mkStdProgressBarForStatusBar() {
    QProgressBar* pBar = new QProgressBar(statusBar());
    pBar->setAlignment(Qt::AlignCenter);
    pBar->setTextVisible(true);
    pBar->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    return pBar;
}

void MainWindow::notifyFirstRun() {
    // On first run prompt configuration box.
    QMessageBox::information(NULL, tr("Welcome to Doomseeker"),
                             tr("Before you start browsing for servers, please ensure that Doomseeker is properly configured."));
    menuActionConfigure->trigger();
}

void MainWindow::onAutoUpdaterDownloadAndInstallConfirmationRequest() {
    updatesConfirmationWidget->show();
}

void MainWindow::onAutoUpdaterFileProgress(qint64 bytesReceived, qint64 bytesTotal) {
    autoUpdaterFileProgressBar->setValue(bytesReceived);
    autoUpdaterFileProgressBar->setMaximum(bytesTotal);
}

void MainWindow::onAutoUpdaterFinish() {
    gLog << tr("Program update detection & download finished with status: [%1] %2")
         .arg((int)autoUpdater->errorCode()).arg(autoUpdater->errorString());
    gConfig.autoUpdates.bPerformUpdateOnNextRun = false;
    if (autoUpdater->errorCode() == AutoUpdater::EC_Ok) {
        UpdateChannel channel = UpdateChannel::fromName(gConfig.autoUpdates.updateChannelName);
        if (channel == *updateChannelOnUpdateStart) {
            if (!autoUpdater->downloadedPackagesFilenames().isEmpty()) {
                gLog << tr("Updates will be installed on next program start.");
                updatesDownloadedWidget->show();
                gConfig.autoUpdates.bPerformUpdateOnNextRun = true;
            }
        } else {
            gLog << tr("Update channel was changed during update process. Discarding update.");
        }
    }
    gConfig.saveToFile();
    autoUpdaterStatusBarWidget->hide();
    updatesConfirmationWidget->hide();
    autoUpdater->deleteLater();
    autoUpdater = NULL;
}

void MainWindow::onAutoUpdaterOverallProgress(int current, int total,
        const QString& msg) {
    autoUpdaterOverallProgressBar->setValue(current);
    autoUpdaterOverallProgressBar->setMaximum(total);
    autoUpdaterOverallProgressBar->setFormat(msg);
}

void MainWindow::postInitAppStartup() {
    // Load server filter from config.
    serverFilterDock->setFilterInfo(gConfig.serverFilter.info);

    // Check query on statup
    // Let's see if we have any plugins first. If not, display error.
    if (gPlugins->numPlugins() > 0) {
        bool bGettingServers = false;
        bool queryOnStartup = gConfig.doomseeker.bQueryOnStartup;
        if (queryOnStartup) {
            // If "Query on startup" option is enabled we should
            // attempt to refresh any masters that are enabled
            // in the Query menu.

            if (isAnyMasterEnabled()) {
                bGettingServers = true;
                getServers();
            } else {
                gLog << tr("Query on startup warning: No master servers are enabled in the Query menu.");
            }
        }

        // If we already successfully called the getServers() method
        // there is no need to call refreshCustomsServers().
        if (!bGettingServers && hasCustomServers()) {
            // Custom servers should be refreshed no matter what.
            // They will not block the app in any way, there is no reason
            // not to refresh them.
            refreshCustomServers();
        }
    } else {
        // There are no plugins so something is really bad.
        // Display error message.
        QMessageBox::critical(NULL, tr("Doomseeker critical error"),
                              tr("Doomseeker was unable to find any plugin libraries.\n"
                                 "Although the application will still work it will not be possible "
                                 "to fetch any server info or launch any game.\n\n"
                                 "Please check if there are any files in \"engines/\" directory.\n"
                                 "To fix this problem you may try downloading Doomseeker "
                                 "again from the site specified in the Help|About box and reinstalling "
                                 "Doomseeker."));
    }
}

QQueryMenuAction* MainWindow::queryMenuActionForMasterClient(MasterClient* pClient) {
    if (pClient == NULL) {
        return NULL;
    }

    if (queryMenuPorts.contains(pClient)) {
        return queryMenuPorts[pClient];
    }

    return NULL;
}

void MainWindow::quitProgram() {
    bWantToQuit = true;
    close();
}

void dupa(ServerPtr a) {
}

void MainWindow::refreshCustomServers() {
    CustomServers* customServers = masterManager->customServs();

    for(int i = 0; i < customServers->numServers(); i++) {
        ServerPtr server = (*customServers)[i];
        serverTableHandler->serverUpdated(server, Server::RESPONSE_NO_RESPONSE_YET);
        gRefresher->registerServer(server.data());
    }
}

void MainWindow::refreshThreadBeginsWork() {
    // disable refresh.
    serverTableHandler->serverTable()->setAllowAllRowsRefresh(false);
    statusBar()->showMessage(tr("Querying..."));
}

void MainWindow::refreshThreadEndsWork() {
    toolBarGetServers->setEnabled(true);
    toolBarRefreshAll->setEnabled(true);

    serverTableHandler->serverTable()->setAllowAllRowsRefresh(true);
    serverTableHandler->cleanUpForce();
    statusBar()->showMessage(tr("Done"));
    updateTrayIconTooltipAndLogTotalRefresh();

    if (bTotalRefreshInProcess) {
        initAutoRefreshTimer();
    }

    bTotalRefreshInProcess = false;
    QList<ServerPtr> selectedServers = serverTableHandler->selectedServers();
    detailsDock->displaySelection(selectedServers);
}

void MainWindow::restartAndInstallUpdatesNow() {
    Main::bInstallUpdatesAndRestart = true;
    quitProgram();
}

void MainWindow::runGame(const ServerPtr &server) {
    if(connectionHandler)
        delete connectionHandler;

    connectionHandler = new ConnectionHandler(server, this);
    connectionHandler->run();
}

void MainWindow::setQueryMasterServerEnabled(MasterClient* pClient, bool bEnabled) {
    assert(pClient != NULL);

    QQueryMenuAction* pAction = queryMenuActionForMasterClient(pClient);
    if (pAction != NULL) {
        pAction->setChecked(bEnabled);
        pClient->setEnabled(bEnabled);
        serversStatusesWidgets[pClient]->setMasterEnabledStatus(bEnabled);
    }
}

void MainWindow::serverAddedToList(const ServerPtr &pServer) {
    if (pServer->isKnown()) {
        const QString& gameMode = pServer->gameMode().name();
        serverFilterDock->addGameModeToComboBox(gameMode);
    }
}

void MainWindow::setDisplayUpdaterProcessFailure(int errorCode) {
    assert(this->updaterInstallerErrorCode == 0 &&
           "MainWindow::setDisplayUpdaterProcessFailure()");
    this->updaterInstallerErrorCode = errorCode;
    QTimer::singleShot(0, this, SLOT(showUpdaterProcessErrorDialog()));
}

void MainWindow::setDisplayUpdateInstallerError(int errorCode) {
    assert(this->updaterInstallerErrorCode == 0 &&
           "MainWindow::setDisplayUpdateInstallerError()");
    this->updaterInstallerErrorCode = errorCode;
    QTimer::singleShot(0, this, SLOT(showUpdateInstallErrorDialog()));
}

void MainWindow::setupIcons() {
    QStyle& style = *QApplication::style();

    // File menu.
    menuActionQuit->setIcon(style.standardIcon(QStyle::SP_TitleBarCloseButton));

    // Help menu.
    menuActionHelp->setIcon(style.standardIcon(QStyle::SP_MessageBoxQuestion));
    menuActionAbout->setIcon(style.standardIcon(QStyle::SP_MessageBoxInformation));
}

void MainWindow::setupToolBar() {
    QToolBar* pToolBar = new QToolBar(tr("Main Toolbar"), this);
    pToolBar->setMovable(false);
    pToolBar->setObjectName("Toolbar");

    // Refresh buttons
    toolBarGetServers = new QAction(QIcon(":/icons/view-refresh-2.png"), tr("Get Servers"), pToolBar);
    toolBarRefreshAll = new QAction(QIcon(":/icons/arrow-down-double.png"), tr("Refresh All"), pToolBar);

    // Setup menu
    // Refresh buttons
    pToolBar->addAction(toolBarGetServers);
    // pToolBar->addAction(toolBarRefreshAll);

    // File menu buttons.
    pToolBar->addSeparator();
    pToolBar->addAction(menuActionCreateServer);
    pToolBar->addAction(menuActionPlayOffline);
    pToolBar->addAction(menuActionWadseeker);

    // Demo buttons
    pToolBar->addSeparator();
    pToolBar->addAction(menuActionManageDemos);
    pToolBar->addAction(menuActionRecordDemo);

    pToolBar->addSeparator();

    // Dockable windows buttons.
    pToolBar->addAction(buddiesList->toggleViewAction());
    pToolBar->addAction(logDock->toggleViewAction());
    pToolBar->addAction(ircDock->toggleViewAction());
    pToolBar->addAction(serverFilterDock->toggleViewAction());
    pToolBar->addAction(detailsDock->toggleViewAction());

    // Quick Search
    QLineEdit *qs = serverFilterDock->createQuickSearch();
    qs->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    qs->setMinimumWidth(175);
    qs->setMaximumWidth(175);

    QWidget* searchSeparator = new QWidget();
    searchSeparator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    pToolBar->addWidget(searchSeparator);
    pToolBar->addWidget(new QLabel(tr("Search:"), pToolBar));
    pToolBar->addWidget(qs);

    this->addToolBar(Qt::TopToolBarArea, pToolBar);
    setUnifiedTitleAndToolBarOnMac(true);
    connect(pToolBar, SIGNAL( actionTriggered(QAction*) ), this, SLOT( toolBarAction(QAction*) ) );
}

void MainWindow::showServerJoinCommandLine(const ServerPtr &server) {
    CommandLineInfo cli;
    ConnectionHandler connectionHandler(server, this);
    if (connectionHandler.obtainJoinCommandLine(cli, tr("Doomseeker - join command line"), false)) {
        QString execPath = cli.executable.absoluteFilePath();
        QStringList args = cli.args;

        CommandLine::escapeExecutable(execPath);
        CommandLine::escapeArgs(args);

        CopyTextDlg ctd(execPath + " " + args.join(" "), server->name(), this);
        ctd.exec();
    }
}

void MainWindow::showUpdaterProcessErrorDialog() {
    QString explanation;
    if (this->updaterInstallerErrorCode != UpdateInstaller::PEC_GeneralFailure) {
        QString errorCodeExplanation = UpdateInstaller::processErrorCodeToStr(
                                           (UpdateInstaller::ProcessErrorCode) this->updaterInstallerErrorCode);
        explanation = tr("Update installation problem:\n%1").arg(errorCodeExplanation);
    } else {
        explanation = tr("Update installation failed.");
    }
    QMessageBox::critical(this, tr("Doomseeker - Auto Update problem"),
                          tr("%1\n\nRemaining updates have been discarded.").arg(explanation));
}

void MainWindow::showUpdateInstallErrorDialog() {
    QString msg = tr("Update install problem:\n%1\n\nRemaining updates have been discarded.")
                  .arg(UpdateInstaller::errorCodeToStr(
                           (UpdateInstaller::ErrorCode)this->updaterInstallerErrorCode));
    QMessageBox::critical(this, tr("Doomseeker - Auto Update problem"), msg);
}

void MainWindow::toggleMasterClientEnabled(MasterClient* pClient) {
    QQueryMenuAction* pAction = queryMenuActionForMasterClient(pClient);
    assert(pAction != NULL);

    setQueryMasterServerEnabled(pClient, !pAction->isChecked());
}

void MainWindow::toolBarAction(QAction* pAction) {
    if (pAction == toolBarGetServers) {
        getServers();
    } else if (pAction == toolBarRefreshAll) {
        if (serverTableHandler->hasAtLeastOneServer()) {
            serverTableHandler->refreshAll();
        } else {
            gLog << "Attempted a refresh on an empty table. Getting servers instead.";
            getServers();
        }
    }
}

void MainWindow::trayIcon_activated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isMinimized() || !isVisible()) {
            bWasMaximized == true ? showMaximized() : showNormal();
            activateWindow();
        } else if (gConfig.doomseeker.bCloseToTrayIcon) {
            close();
        } else {
            showMinimized();
        }
    }
}

// NOTE: Probably would be better if the master manager wasn't tied to the
//       MainWindow class?
void MainWindow::updateMasterAddresses() {
    for(int i = 0; i < masterManager->numMasters(); i++) {
        (*masterManager)[i]->updateAddress();
    }

}

void MainWindow::updateServerFilter(const ServerListFilterInfo& filterInfo) {
    serverTableHandler->applyFilter(filterInfo);
    lblServerFilterApplied->setVisible(filterInfo.isFilteringAnything());
}

void MainWindow::updateTrayIconTooltipAndLogTotalRefresh() {
    int numServers = masterManager->numServers();
    int numCustoms = masterManager->customServs()->numServers();
    int numPlayers = masterManager->numPlayers() + masterManager->customServs()->numPlayers();

    if (trayIcon != NULL) {
        QString tip;
        tip += "Servers: " + QString::number(numServers) + " + " + QString::number(numCustoms) + " custom\n";
        tip += "Players: " + QString::number(numPlayers);
        trayIcon->setToolTip(tip);
    }

    if (bTotalRefreshInProcess) {
        gLog << tr("Finished refreshing. Servers on the list: %1 (+ %2 custom). Players: %3.")
             .arg(numServers).arg(numCustoms).arg(numPlayers);
    }
}
