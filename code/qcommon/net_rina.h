/*
 *  RINA calls for ioQuake3
 *
 *    Addy Bombeke      <addy.bombeke@ugent.be>
 *    Sander Vrijders   <sander.vrijders@intec.ugent.be>
 *    Dimitri Staessens <dimitri.staessens@intec.ugent.be>
 *
 *    Copyright (C) 2015
 *
 *  This software was written as part of the MSc thesis in electrical
 *  engineering,
 *
 *  "Comparing RINA to TCP/IP for latency-constrained applications",
 *
 *  at Ghent University, Academic Year 2014-2015
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NET_RINA_H
#define NET_RINA_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

void rina_init      (char server);
void rina_resolve   (const char* s, netadr_t* a);
int  rina_recvfrom  (msg_t* msg,netadr_t* from);
void rina_sendto    (int length, const void* data, netadr_t* to);
char rina_read_event(void);

extern int rina_event;

#endif//NET_RINA_H
