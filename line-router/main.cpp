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

#include "pconsumer.h"
#include <QtCore>
#include "qpairingheap.h"

static inline int bit_scan_forward_asm64(unsigned long long v)
{
	long r;
	asm volatile(" bsfq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

static inline int bit_scan_reverse_asm64(unsigned long long v)
{
	long r;
	asm volatile(" bsrq %1, %0": "=r"(r): "rm"(v) );
	return r;
}

int main(int argc, char *argv[])
{
	runPacketFilter(argc, argv);
	//QPairingHeap_test();

//	__attribute__((aligned(8))) quint64 x = 6ULL;
//	qDebug() << bit_scan_forward_asm64(x);
}

