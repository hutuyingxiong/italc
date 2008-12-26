/*
 * main_window.cpp - main-file for iTALC-Application
 *
 * Copyright (c) 2004-2008 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
 *
 * This file is part of iTALC - http://italc.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#include <italcconfig.h>

#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopWidget>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include <QtGui/QScrollArea>
#include <QtGui/QSplashScreen>
#include <QtGui/QSplitter>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>
#include <QtGui/QWorkspace>
#include <QtNetwork/QHostAddress>

#include "main_window.h"
#include "classroom_manager.h"
#include "dialogs.h"
#include "italc_side_bar.h"
#include "overview_widget.h"
#include "snapshot_list.h"
#include "config_widget.h"
#include "messagebox.h"
#include "tool_button.h"
#include "tool_bar.h"
#include "isd_connection.h"
#include "local_system.h"
#include "remote_control_widget.h"


QSystemTrayIcon * __systray_icon = NULL;


extern int __isd_port;
extern QString __isd_host;


bool mainWindow::ensureConfigPathExists( void )
{
	return( localSystem::ensurePathExists(
					localSystem::personalConfigDir() ) );
}


bool mainWindow::s_atExit = FALSE;


mainWindow::mainWindow( int _rctrl_screen ) :
	QMainWindow(/* 0, Qt::FramelessWindowHint*/ ),
	m_openedTabInSideBar( 1 ),
	m_localISD( NULL ),
	m_rctrlLock(),
	m_remoteControlWidget( NULL ),
	m_stopDemo( FALSE ),
	m_remoteControlScreen( _rctrl_screen > -1 ?
				qMin( _rctrl_screen,
					QApplication::desktop()->numScreens() )
				:
				QApplication::desktop()->screenNumber( this ) )
{
	setWindowTitle( tr( "iTALC" ) + " " + ITALC_VERSION );
	setWindowIcon( QPixmap( ":/resources/logo.png" ) );

	if( mainWindow::ensureConfigPathExists() == FALSE )
	{
		if( splashScreen != NULL )
		{
			splashScreen->hide();
		}
		messageBox::information( tr( "No write-access" ),
			tr( "Could not read/write or create directory %1! "
			"For running iTALC, make sure you're permitted to "
			"create or write this directory." ).arg(
					localSystem::personalConfigDir() ) );
		return;
	}

	QWidget * hbox = new QWidget( this );
	QHBoxLayout * hbox_layout = new QHBoxLayout( hbox );
	hbox_layout->setMargin( 0 );
	hbox_layout->setSpacing( 0 );

	// create splitter, which is used for splitting sidebar-workspaces
	// from main-workspace
	m_splitter = new QSplitter( Qt::Horizontal, hbox );
#if QT_VERSION >= 0x030200
	m_splitter->setChildrenCollapsible( FALSE );
#endif

	// create sidebar
	m_sideBar = new italcSideBar( italcSideBar::VSNET, hbox, m_splitter );



	QScrollArea * sa = new QScrollArea( m_splitter );
	sa->setBackgroundRole( QPalette::Dark );
	sa->setFrameStyle( QFrame::NoFrame );
	m_splitter->setStretchFactor( m_splitter->indexOf( sa ), 10 );
	m_workspace = new clientWorkspace( sa );


	QWidget * twp = m_sideBar->tabWidgetParent();
	// now create all sidebar-workspaces
	m_overviewWidget = new overviewWidget( this, twp );
	m_classroomManager = new classroomManager( this, twp );
	m_snapshotList = new snapshotList( this, twp );
	m_configWidget = new configWidget( this, twp );

	m_workspace->m_contextMenu = m_classroomManager->quickSwitchMenu();

	// append sidebar-workspaces to sidebar
	int id = 0;
	m_sideBar->appendTab( m_overviewWidget, ++id );
	m_sideBar->appendTab( m_classroomManager, ++id );
	m_sideBar->appendTab( m_snapshotList, ++id );
	m_sideBar->appendTab( m_configWidget, ++id );
	m_sideBar->setPosition( italcSideBar::Left );
	m_sideBar->setTab( m_openedTabInSideBar, TRUE );

	setCentralWidget( hbox );
	hbox_layout->addWidget( m_sideBar );
	hbox_layout->addWidget( m_splitter );




	// create the action-toolbar
	m_toolBar = new toolBar( tr( "Actions" ), this );
	m_toolBar->layout()->setSpacing( 4 );
	m_toolBar->setMovable( FALSE );
	m_toolBar->setObjectName( "maintoolbar" );
	m_toolBar->toggleViewAction()->setEnabled( FALSE );

	addToolBar( Qt::TopToolBarArea, m_toolBar );

	toolButton * scr = new toolButton(
			QPixmap( ":/resources/classroom.png" ),
			tr( "Classroom" ), QString::null,
			tr( "Switch classroom" ),
			tr( "Click this button to open a menu where you can "
				"choose the active classroom." ),
			NULL, NULL, m_toolBar );
	scr->setMenu( m_classroomManager->quickSwitchMenu() );
	scr->setPopupMode( toolButton::InstantPopup );
	scr->setWhatsThis( tr( "Click on this button, to switch between "
							"classrooms." ) );

	m_modeGroup = new QButtonGroup( this );

	QAction * a;

	a = new QAction( QIcon( ":/resources/overview_mode.png" ),
						tr( "Overview mode" ), this );
	m_sysTrayActions << a;
	toolButton * overview_mode = new toolButton(
			a, tr( "Overview" ), QString::null,
			tr( "This is the default mode in iTALC and allows you "
				"to have an overview over all visible "
				"computers. Also click on this button for "
				"unlocking locked workstations or for leaving "
				"demo-mode." ),
			this, SLOT( mapOverview() ), m_toolBar );


	a = new QAction( QIcon( ":/resources/fullscreen_demo.png" ),
						tr( "Fullscreen demo" ), this );
	m_sysTrayActions << a;
	toolButton * fsdemo_mode = new toolButton(
			a, tr( "Fullscreen Demo" ), tr( "Stop Demo" ),
			tr( "In this mode your screen is being displayed on "
				"all shown computers. Furthermore the users "
				"aren't able to do something else as all input "
				"devices are locked in this mode." ),
			this, SLOT( mapFullscreenDemo() ), m_toolBar );

	a = new QAction( QIcon( ":/resources/window_demo.png" ),
						tr( "Window demo" ), this );
	m_sysTrayActions << a;
	toolButton * windemo_mode = new toolButton(
			a, tr( "Window Demo" ), tr( "Stop Demo" ),
			tr( "In this mode your screen being displayed in a "
				"window on all shown computers. The users are "
				"able to switch to other windows and thus "
				"can continue to work." ),
			this, SLOT( mapWindowDemo() ), m_toolBar );

	a = new QAction( QIcon( ":/resources/locked.png" ),
					tr( "Lock/unlock desktops" ), this );
	m_sysTrayActions << a;
	toolButton * lock_mode = new toolButton(
			a, tr( "Lock all" ), tr( "Unlock all" ),
			tr( "To have all user's full attention you can lock "
				"their desktops using this button. "
				"In this mode all input devices are locked and "
				"the screen is black." ),
			this, SLOT( mapScreenLock() ), m_toolBar );

	overview_mode->setCheckable( TRUE );
	fsdemo_mode->setCheckable( TRUE );
	windemo_mode->setCheckable( TRUE );
	lock_mode->setCheckable( TRUE );

	m_modeGroup->addButton( overview_mode, client::Mode_Overview );
	m_modeGroup->addButton( fsdemo_mode, client::Mode_FullscreenDemo );
	m_modeGroup->addButton( windemo_mode, client::Mode_WindowDemo );
	m_modeGroup->addButton( lock_mode, client::Mode_Locked );

	overview_mode->setChecked( TRUE );



	a = new QAction( QIcon( ":/resources/text_message.png" ),
					tr( "Send text message" ), this );
//	m_sysTrayActions << a;
	toolButton * text_msg = new toolButton(
			a, tr( "Text message" ), QString::null,
			tr( "Use this button to send a text message to all "
				"users e.g. to tell them new tasks etc." ),
			m_classroomManager, SLOT( sendMessage() ), m_toolBar );


	a = new QAction( QIcon( ":/resources/power_on.png" ),
					tr( "Power on computers" ), this );
	m_sysTrayActions << a;
	toolButton * power_on = new toolButton(
			a, tr( "Power on" ), QString::null,
			tr( "Click this button to power on all visible "
				"computers. This way you do not have to turn "
				"on each computer by hand." ),
			m_classroomManager, SLOT( powerOnClients() ),
								m_toolBar );

	a = new QAction( QIcon( ":/resources/power_off.png" ),
					tr( "Power down computers" ), this );
	m_sysTrayActions << a;
	toolButton * power_off = new toolButton(
			a, tr( "Power down" ), QString::null,
			tr( "To power down all shown computers (e.g. after "
				"the lesson has finished) you can click this "
				"button." ),
			m_classroomManager,
					SLOT( powerDownClients() ), m_toolBar );

	toolButton * remotelogon = new toolButton(
			QPixmap( ":/resources/remotelogon.png" ),
			tr( "Logon" ), QString::null,
			tr( "Remote logon" ),
			tr( "After clicking this button you can enter a "
				"username and password to log on the "
				"according user on all visible computers." ),
			m_classroomManager, SLOT( remoteLogon() ), m_toolBar );

	toolButton * directsupport = new toolButton(
			QPixmap( ":/resources/remote_control.png" ),
			tr( "Support" ), QString::null,
			tr( "Direct support" ),
			tr( "If you need to support someone at a certain "
				"computer you can click this button and enter "
				"the according hostname or IP afterwards." ),
			m_classroomManager, SLOT( directSupport() ), m_toolBar );

	toolButton * adjust_size = new toolButton(
			QPixmap( ":/resources/adjust_size.png" ),
			tr( "Adjust/align" ), QString::null,
			tr( "Adjust windows and their size" ),
			tr( "When clicking this button the biggest possible "
				"size for the client-windows is adjusted. "
				"Furthermore all windows are aligned." ),
			m_classroomManager, SLOT( adjustWindows() ), m_toolBar );

	toolButton * auto_arrange = new toolButton(
			QPixmap( ":/resources/auto_arrange.png" ),
			tr( "Auto view" ), QString::null,
			tr( "Auto re-arrange windows and their size" ),
			tr( "When clicking this button all visible windows "
					"are re-arranged and adjusted." ),
			NULL, NULL, m_toolBar );
	auto_arrange->setCheckable( true );
	auto_arrange->setChecked( m_classroomManager->isAutoArranged() );
	connect( auto_arrange, SIGNAL( toggled( bool ) ), m_classroomManager,
						 SLOT( arrangeWindowsToggle ( bool ) ) );

	scr->addTo( m_toolBar );
	overview_mode->addTo( m_toolBar );
	fsdemo_mode->addTo( m_toolBar );
	windemo_mode->addTo( m_toolBar );
	lock_mode->addTo( m_toolBar );
	text_msg->addTo( m_toolBar );
	power_on->addTo( m_toolBar );
	power_off->addTo( m_toolBar );
	remotelogon->addTo( m_toolBar );
	directsupport->addTo( m_toolBar );
	adjust_size->addTo( m_toolBar );
	auto_arrange->addTo( m_toolBar );

	restoreState( QByteArray::fromBase64(
				m_classroomManager->winCfg().toUtf8() ) );
	QStringList hidden_buttons = m_classroomManager->toolBarCfg().
								split( '#' );
	foreach( QAction * a, m_toolBar->actions() )
	{
		if( hidden_buttons.contains( a->text() ) )
		{
			a->setVisible( FALSE );
		}
	}

	foreach( KMultiTabBarTab * tab, m_sideBar->tabs() )
	{
		if( hidden_buttons.contains( tab->text() ) )
		{
			tab->setTabVisible( FALSE );
		}
	}

	while( 1 )
	{
		if( isdConnection::initAuthentication() == FALSE )
		{
			if( __role != ISD::RoleTeacher )
			{
				__role = ISD::RoleTeacher;
				continue;
			}
			if( splashScreen != NULL )
			{
				splashScreen->hide();
			}
			messageBox::information( tr( "No valid keys found" ),
			tr( 	"No authentication-keys were found or your "
				"old ones were broken. Please create a new "
				"key-pair using ICA (see documentation at "
		"http://italc.sf.net/wiki/index.php?title=Installation).\n"
				"Otherwise you won't be able to access "
						"computers using iTALC." ) );
		}
		break;
	}
	m_localISD = new isdConnection( QHostAddress(
				__isd_host ).toString() +
					":" + QString::number( __isd_port ) );
	if( m_localISD->open() != isdConnection::Connected )
	{
		messageBox::information( tr( "iTALC service not running" ),
			tr( 	"There seems to be no iTALC service running "
				"on this computer or the authentication-keys "
				"aren't set up properly. The service is "
				"required for running iTALC. Contact your "
				"administrator for solving this problem." ),
				QPixmap( ":/resources/stop.png" ) );
		return;
	}

	m_localISD->demoServerRun( __demo_quality,
						localSystem::freePort( 5858 ) );


	m_localISD->hideTrayIcon();

	QIcon icon( ":/resources/icon16.png" );
	icon.addFile( ":/resources/icon22.png" );
	icon.addFile( ":/resources/icon32.png" );

	__systray_icon = new QSystemTrayIcon( icon, this );
	__systray_icon->setToolTip( tr( "iTALC Master Control" ) );
	__systray_icon->show();
	connect( __systray_icon, SIGNAL( activated(
					QSystemTrayIcon::ActivationReason ) ),
		this, SLOT( handleSystemTrayEvent(
					QSystemTrayIcon::ActivationReason ) ) );


	QTimer::singleShot( 2000, m_classroomManager, SLOT( updateClients() ) );

	m_updateThread = new mainWindowUpdateThread( this );
}




mainWindow::~mainWindow()
{
	m_classroomManager->doCleanupWork();

#ifdef BUILD_WIN32
	qApp->processEvents( QEventLoop::AllEvents, 3000 );
	localSystem::sleep( 3000 );
#endif

	// also delets clients
	delete m_workspace;

	m_localISD->gracefulClose();

	delete m_localISD;
	m_localISD = NULL;

	__systray_icon->hide();
	delete __systray_icon;

#ifdef BUILD_WIN32
	qApp->processEvents( QEventLoop::AllEvents, 3000 );
	localSystem::sleep( 3000 );
	exit( 0 );
#endif
}




void mainWindow::keyPressEvent( QKeyEvent * _e )
{
	if( _e->key() == Qt::Key_F11 )
	{
		QWidget::setWindowState( QWidget::windowState() ^
							Qt::WindowFullScreen );
	}
	else
	{
		QMainWindow::keyPressEvent( _e );
	}
}




void mainWindow::closeEvent( QCloseEvent * _ce )
{
	s_atExit = TRUE;

	m_updateThread->quit();
	m_updateThread->wait();
	delete m_updateThread;
	m_updateThread = NULL;

	QList<client *> clients = m_workspace->findChildren<client *>();
	foreach( client * c, clients )
	{
		c->quit();
	}

	m_classroomManager->savePersonalConfig();
	m_classroomManager->saveGlobalClientConfig();

	_ce->accept();
	deleteLater();
}




void mainWindow::handleSystemTrayEvent( QSystemTrayIcon::ActivationReason _r )
{
	switch( _r )
	{
		case QSystemTrayIcon::Trigger:
			setVisible( !isVisible() );
			break;
		case QSystemTrayIcon::Context:
		{
			QMenu m( this );
			m.addAction( __systray_icon->toolTip() )->setEnabled( FALSE );
			foreach( QAction * a, m_sysTrayActions )
			{
				m.addAction( a );
			}

			m.addSeparator();

			QMenu rcm( this );
			QAction * rc = m.addAction( tr( "Remote control" ) );
			rc->setMenu( &rcm );
			foreach( client * c,
					m_classroomManager->visibleClients() )
			{
				rcm.addAction( c->name() )->
						setData( c->hostname() );
			}
			connect( &rcm, SIGNAL( triggered( QAction * ) ),
				this,
				SLOT( remoteControlClient( QAction * ) ) );

			m.addSeparator();

			QAction * qa = m.addAction(
					QIcon( ":/resources/quit.png" ),
					tr( "Quit" ) );
			connect( qa, SIGNAL( triggered( bool ) ),
					this, SLOT( close() ) );
			m.exec( QCursor::pos() );
			break;
		}
		default:
			break;
	}
}




void mainWindow::remoteControlClient( QAction * _a )
{
	show();
	remoteControlDisplay( _a->data().toString(),
				m_classroomManager->clientDblClickAction() );
}




void mainWindow::remoteControlDisplay( const QString & _hostname,
						bool _view_only,
						bool _stop_demo_afterwards )
{
	QWriteLocker wl( &m_rctrlLock );
	if( m_remoteControlWidget  )
	{
		return;
	}
	m_remoteControlWidget = new remoteControlWidget( _hostname, _view_only,
									this );
	int x = 0;
	for( int i = 0; i < m_remoteControlScreen; ++i )
	{
		x += QApplication::desktop()->screenGeometry( i ).width();
	}
	m_remoteControlWidget->move( x, 0 );
	m_stopDemo = _stop_demo_afterwards;
	connect( m_remoteControlWidget, SIGNAL( destroyed( QObject * ) ),
			this, SLOT( remoteControlWidgetClosed( QObject * ) ) );
}




void mainWindow::remoteControlWidgetClosed( QObject * )
{
	m_rctrlLock.lockForWrite();
	m_remoteControlWidget = NULL;
	m_rctrlLock.unlock();
	if( m_stopDemo )
	{
		m_classroomManager->changeGlobalClientMode(
							client::Mode_Overview );
		m_stopDemo = FALSE;
	}
}




void mainWindow::aboutITALC( void )
{
	aboutDialog( this ).exec();
}




void mainWindow::changeGlobalClientMode( int _mode )
{
	client::modes new_mode = static_cast<client::modes>( _mode );
	if( new_mode == m_classroomManager->globalClientMode()/* &&
					new_mode != client::Mode_Overview*/ )
	{
		m_classroomManager->changeGlobalClientMode(
							client::Mode_Overview );
		m_modeGroup->button( client::Mode_Overview )->setChecked(
									TRUE );
	}
	else
	{
		m_classroomManager->changeGlobalClientMode( _mode );
	}
}







mainWindowUpdateThread::mainWindowUpdateThread( mainWindow * _main_window ) :
	QThread(),
	m_mainWindow( _main_window )
{
	start( QThread::LowestPriority );
}




void mainWindowUpdateThread::update( void )
{
	m_mainWindow->m_localISD->handleServerMessages();

	if( client::reloadSnapshotList() )
	{
		m_mainWindow->m_snapshotList->reloadList();
	}
	client::resetReloadOfSnapshotList();

	// now do cleanup-work
	m_mainWindow->getClassroomManager()->doCleanupWork();
}


void mainWindowUpdateThread::run( void )
{
	QTimer t;
	connect( &t, SIGNAL( timeout() ), this, SLOT( update() ) );
	t.start( m_mainWindow->getClassroomManager()->updateInterval() * 1000 );
	exec();
}





clientWorkspace::clientWorkspace( QScrollArea * _parent ) :
	QWidget( _parent ),
	m_contextMenu( NULL )
{
	_parent->setWidget( this );
	_parent->setWidgetResizable( TRUE );
	setSizePolicy( QSizePolicy( QSizePolicy::MinimumExpanding,
					QSizePolicy::MinimumExpanding ) );
	show();

}



QSize clientWorkspace::sizeHint( void ) const
{
	return( childrenRect().size() );
}




void clientWorkspace::contextMenuEvent( QContextMenuEvent * _event )
{
	m_contextMenu->exec( _event->globalPos() );
	_event->accept();
}



