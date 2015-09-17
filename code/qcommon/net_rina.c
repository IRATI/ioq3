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

#include "net_rina.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <librina-c/librina-c.h>

const char* server_app_name      = "rina.games.server.ioq3";
const char* server_app_instance  = "1";
const char* client_app_name      = "rina.games.client.ioq3";
const char* client_app_instance  = "1";
const char* dif_name             = 0;

const unsigned long long one = 1;
unsigned long long maybeone  = 1;

#define c_p (16*1024)
#define c_pq 16
struct {
    char d[c_p];
    uint s;
    int  f;
} pq[c_pq];

volatile unsigned int l_pq=0;
volatile unsigned int r_pq=0;
pthread_mutex_t       push_q_mtx;

int application_register(const char* app_name,
                         const char* app_instance,
                         const char* dif_name)
{
	unsigned int seqnum =
                rinaIPCManager_requestApplicationRegistration(app_name,
                                                              app_instance,
                                                              dif_name);
	rina_ipcevent* event = rinaIPCEventProducer_eventWait();
	while (event &&
               !(rinaIPCEvent_eventType(event) == 8 &&
                 rinaIPCEvent_sequenceNumber(event) == seqnum)) {
		event = rinaIPCEventProducer_eventWait();
	}
	if (rinaBaseApplicationRegistrationResponseEvent_result(event) == 0) {
		rinaIPCManager_commitPendingRegistration(seqnum, dif_name);
		return 0; /* registered application */
	} else {
		rinaIPCManager_withdrawPendingRegistration(seqnum);
		return 1; /* failed to register application */
	}
}

rina_flow_t create_flow(const char* local_app_name,
                        const char* local_app_instance,
                        const char* remote_app_name,
                        const char* remote_app_instance,
                        const char* dif_name)
{
	int seqnum;
	if (dif_name != 0) {
                seqnum = rinaIPCManager_requestFlowAllocationInDIF(local_app_name,
                                                                   local_app_instance,
                                                                   remote_app_name,
                                                                   remote_app_instance,
                                                                   dif_name,
                                                                   0);
	} else {
		seqnum = rinaIPCManager_requestFlowAllocation(local_app_name,
                                                              local_app_instance,
                                                              remote_app_name,
                                                              remote_app_instance,
                                                              0);
	}
	rina_ipcevent* event = rinaIPCEventProducer_eventWait();
	while (event && !(
                      rinaIPCEvent_eventType(event) == 1 &&
                      rinaIPCEvent_sequenceNumber(event) == seqnum)) {
		event = rinaIPCEventProducer_eventWait();
	}
	char* event_dif_name = rinaAllocateFlowRequestResultEvent_difName(event);
	rina_flow_t ret = rinaIPCManager_commitPendingFlow(
						   rinaIPCEvent_sequenceNumber(event),
						   rinaAllocateFlowRequestResultEvent_portId(event),
						   event_dif_name);
	free(event_dif_name);
	if (!ret || rinaFlow_getPortId(ret) == -1)
		return 0;
	return ret;
}

const char* getDIF(const char* difname)
{
        if((difname == 0) || ((*difname) == 0))
                return 0;
        return difname;
}

void* rina_flow_connection(void* flowptr)
{
        char d[c_p];
        rina_flow_t flow = (rina_flow_t)flowptr;
        uint n = rinaFlow_readSDU(flow,d,c_p);
        while (n>0) {
                pthread_mutex_lock(&push_q_mtx);
                memcpy(pq[r_pq].d, d, n);
                pq[r_pq].s=n;
                pq[r_pq].f=flow;
                r_pq=(r_pq+1)%c_pq;
                write(rina_event, &one, sizeof(unsigned long long));
                pthread_mutex_unlock(&push_q_mtx);
                n = rinaFlow_readSDU(flow, d, c_p);
        }
        return NULL;
}

void* rina_server_listen(void* para)
{
        printf("RINA: serverlisten\n");
	rina_ipcevent* event = rinaIPCEventProducer_eventWait();
	char* event_dif_name;
        rina_flow_t flow = 0;
        pthread_t flow_thread;
	while(event)
        {
		switch(rinaIPCEvent_eventType(event)) {
		case 8:
                        event_dif_name = rinaRegisterApplicationResponseEvent_DIFName(event);
			rinaIPCManager_commitPendingRegistration(rinaIPCEvent_sequenceNumber(event),
                                                                 event_dif_name);
			free(event_dif_name);
			event = rinaIPCEventProducer_eventWait();
			break;
		case 0:
			flow = rinaIPCManager_allocateFlowResponse(event, 0, 1);
                        pthread_create(&flow_thread,
                                       NULL,
                                       rina_flow_connection,
                                       (void*) flow);
                        pthread_detach(flow_thread);
			event = rinaIPCEventProducer_eventWait();
			break;
		default:
			event = 0;
			break;
		}
        }
	return NULL;
}

void rina_init(char server)
{
        pthread_mutex_init(&push_q_mtx, NULL);
        if(rina_event == -1)
                rina_event=eventfd(0, EFD_SEMAPHORE);
        rina_initialize("INFO", "");
        if(server)
        {
                if (application_register(server_app_name,
                                         server_app_instance,
                                         getDIF(dif_name)) != 0)
                        return;
                pthread_t listen_thread;
                pthread_create(&listen_thread,NULL,rina_server_listen,NULL);
                pthread_detach(listen_thread);
        }
}

void rina_resolve(const char* s, netadr_t* a)
{
      a->type = NA_RINA;
      rina_flow_t flow = create_flow(client_app_name,
                                     client_app_instance,
                                     server_app_name,
                                     server_app_instance,
                                     getDIF(dif_name));
      a->flow = flow;
      pthread_t flow_thread;
      pthread_create(&flow_thread, NULL, rina_flow_connection, (void*) flow);
      pthread_detach(flow_thread);
}

int rina_recvfrom(msg_t* msg, netadr_t* from)
{
        if(rina_read_event()) {
                read(rina_event, &maybeone, sizeof(unsigned long long));
                from->type = NA_RINA;
                from->flow = pq[l_pq].f;
                msg->cursize = MIN(pq[l_pq].s, msg->maxsize);
                memcpy(msg->data, pq[l_pq].d, msg->cursize);
                l_pq=(l_pq+1)%c_pq;
                return 0;
        }
        return -1;
}

void rina_sendto(int length, const void* data, netadr_t* to)
{
        rinaFlow_writeSDU(to->flow, (void*) data, length);
}


char rina_read_event(void)
{
        return l_pq != r_pq;
}

int rina_event=-1;//invalid_socket
