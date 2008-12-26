/*
 * italc_rfb_ext.h - an extension of the RFB-protocol, used for communication
 *                   between master and clients
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

#ifndef _ITALC_RFB_EXT_H
#define _ITALC_RFB_EXT_H

#include <rfb/rfbproto.h>
#include <rfb/rfbclient.h>

#include "types.h"

// new rfb-command which tells server or client that an italc-request/response
// is following
#define rfbItalcServiceRequest		19
#define rfbItalcServiceResponse		rfbItalcServiceRequest


#define rfbEncodingItalc 19
#define rfbEncodingItalcCursor 20

#define rfbSecTypeItalc 19


enum PortOffsets
{
	PortOffsetIVS = 11100
} ;


struct italcRectEncodingHeader
{
	Q_UINT8 compressed;
	Q_UINT32 bytesLZO;
	Q_UINT32 bytesRLE;
} ;


enum ItalcAuthTypes
{
	// no authentication needed
	ItalcAuthNone,

	// only hosts in internal host-list are allowed
	ItalcAuthHostBased,

	// client has to sign some data to verify it's authority
	ItalcAuthDSA,

	// almost the same as ItalcAuthDSA - suppresses checks concerning
	// teacher-role when connecting to local ISD (otherwise a question
	// would appear for confirming access when starting iTALC as teacher
	// and ICA is running in teacher-mode as well)
	ItalcAuthLocalDSA,

	// used for authentication of demo-server against IVS which is done by
	// simply showing IVS that demo-server runs inside the same application
	// by sending generated challenge which is a global variable and can be
	// accessed by demo-server
	ItalcAuthAppInternalChallenge,

	// similiar to ItalcAuthAppInternalChallenge with the only difference
	// that authentication is done via a file which is only readable by
	// owner - only used by Linux/X11-version as IVS is run in separate
	// process and therefore ItalcAuthAppInternalChallenge won't work
	ItalcAuthChallengeViaAuthFile,
} ;


enum ItalcAuthResults
{
	ItalcAuthOK,
	ItalcAuthFailed
} ;

#ifdef __cplusplus
extern "C"
{
#endif
int handleSecTypeItalc( rfbClient * _cl );
#ifdef __cplusplus
}
#endif

#endif
