/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MESSAGES_H
#define MESSAGES_H

// client to server
#define OFFSET_C2S_SEQNO     0
#define SIZE_C2S_SEQNO       (sizeof(long long))
#define OFFSET_C2S_TSCLIENT  (OFFSET_C2S_SEQNO + SIZE_C2S_SEQNO)
#define SIZE_C2S_TSCLIENT    (sizeof(long long))
#define MIN_C2S_PAYLOAD      (OFFSET_C2S_TSCLIENT + SIZE_C2S_TSCLIENT)

// server to client
#define OFFSET_S2C_SEQNO     OFFSET_C2S_SEQNO
#define SIZE_S2C_SEQNO       SIZE_C2S_SEQNO
#define OFFSET_S2C_TSECHOED  OFFSET_C2S_TSCLIENT
#define SIZE_S2C_TSECHOED    SIZE_C2S_TSCLIENT
#define OFFSET_S2C_TSSERVER  (OFFSET_S2C_TSECHOED + SIZE_S2C_TSECHOED)
#define SIZE_S2C_TSSERVER    (sizeof(long long))
#define OFFSET_S2C_CLIENTIP  (OFFSET_S2C_TSSERVER + SIZE_S2C_TSSERVER)
#define SIZE_S2C_CLIENTIP    4
#define MIN_S2C_PAYLOAD      (OFFSET_S2C_CLIENTIP + SIZE_S2C_CLIENTIP)

#define MIN_PAYLOAD          (MIN_C2S_PAYLOAD < MIN_S2C_PAYLOAD ? MIN_S2C_PAYLOAD : MIN_C2S_PAYLOAD)

#endif // MESSAGES_H
