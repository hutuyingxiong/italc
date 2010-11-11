/*
 * AccessKeyAssistant.cpp - a wizard assisting in managing access keys
 *
 * Copyright (c) 2010 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
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

#include <QtCore/QDir>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>

#include "AccessKeyAssistant.h"
#include "DsaKey.h"
#include "ImcCore.h"
#include "ItalcConfiguration.h"
#include "ItalcCore.h"
#include "LocalSystem.h"
#include "Logger.h"
#include "ui_AccessKeyAssistant.h"


AccessKeyAssistant::AccessKeyAssistant() :
	QWizard(),
	m_ui( new Ui::AccessKeyAssistant )
{
	m_ui->setupUi( this );

	m_ui->assistantModePage->setUi( m_ui );
	m_ui->directoriesPage->setUi( m_ui );

	// init destination directory line edit
	QDir d( LocalSystem::Path::expand( ItalcCore::config->privateKeyBaseDir() ) );
	d.cdUp();
	m_ui->destDirEdit->setText( QDTNS( d.absolutePath() ) );

	connect( m_ui->openDestDir, SIGNAL( clicked() ),
				this, SLOT( openDestDir() ) );

	connect( m_ui->openPubKeyDir, SIGNAL( clicked() ),
				this, SLOT( openPubKeyDir() ) );
}



AccessKeyAssistant::~AccessKeyAssistant()
{
}




void AccessKeyAssistant::openPubKeyDir()
{
	if( m_ui->modeCreateKeys->isChecked() )
	{
		QString pkDir = LocalSystem::Path::expand( m_ui->publicKeyDir->text() );
		if( !QFileInfo( pkDir ).isDir() )
		{
			pkDir = QDir::homePath();
		}
		pkDir = QFileDialog::getExistingDirectory( this,
				tr( "Select directory in which to export the public key" ),
				pkDir,
				QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
		if( !pkDir.isEmpty() )
		{
			m_ui->publicKeyDir->setText( pkDir );
		}
	}
	else if( m_ui->modeImportPublicKey->isChecked() )
	{
		QString pk = LocalSystem::Path::expand( m_ui->publicKeyDir->text() );
		if( QFileInfo( pk ).isFile() )
		{
			pk = QFileInfo( pk ).absolutePath();
		}
		else
		{
			pk = QDir::homePath();
		}
		pk = QFileDialog::getOpenFileName( this,
				tr( "Select public key to import" ),
				pk, tr( "Key files (*.key.txt)" ) );
		if( !pk.isEmpty() )
		{
			if( PublicDSAKey( pk ).isValid () )
			{
				m_ui->publicKeyDir->setText( pk );
			}
			else
			{
				QMessageBox::critical( this, tr( "Invalid public key" ),
					tr( "The selected file does not contain a valid public "
						"iTALC access key!" ) );
			}
		}
	}
}




void AccessKeyAssistant::openDestDir()
{
	QString destDir = LocalSystem::Path::expand( m_ui->destDirEdit->text() );
	if( !QFileInfo( destDir ).isDir() )
	{
		destDir = QDir::homePath();
	}
	destDir = QFileDialog::getExistingDirectory( this,
				tr( "Select destination directory" ),
				destDir,
				QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
	if( !destDir.isEmpty() )
	{
		m_ui->destDirEdit->setText( destDir );
	}
}




void AccessKeyAssistant::accept()
{
	if( m_ui->modeCreateKeys->isChecked() )
	{
		ItalcCore::UserRole role =
			static_cast<ItalcCore::UserRole>(
					m_ui->userRole->currentIndex() + ItalcCore::RoleTeacher );
		QString destDir;
		if( m_ui->useCustomDestDir->isChecked() ||
				// trap the case public and private key path are equal
				LocalSystem::Path::publicKeyPath( role ) ==
					LocalSystem::Path::privateKeyPath( role ) )
		{
			destDir = m_ui->destDirEdit->text();
		}

		if( ImcCore::createKeyPair( role, destDir ) )
		{
			if( m_ui->exportPublicKey->isChecked() )
			{
				QFile f( LocalSystem::Path::publicKeyPath( role, destDir ) );
				f.copy( QDTNS( m_ui->publicKeyDir->text() +
										"/italc_public_key.key.txt" ) );
			}
			QMessageBox::information( this, tr( "Access key creation" ),
				tr( "Access keys were created and written successfully to %1 and %2." ).
					arg( LocalSystem::Path::privateKeyPath( role, destDir ) ).
					arg( LocalSystem::Path::publicKeyPath( role, destDir ) ) );
		}
		else
		{
			QMessageBox::critical( this, tr( "Access key creation" ),
					tr( "An error occured while creating the access keys. "
						"You probably are not permitted to write to the "
						"selected directories." ) );
		}

	}
	else if( m_ui->modeImportPublicKey->isChecked() )
	{
	}

	QWizard::accept();
}
