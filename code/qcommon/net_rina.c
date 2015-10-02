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
#include <time.h>
#include <stdlib.h>

#include <librina-c/librina-c.h>

const char *server_app_name      = "rina.games.server.ioq3";
const char *server_app_instance  = "1";
const char *client_app_name      = "rina.games.client.ioq3";
const char *dif_name             = 0;

const unsigned long long one = 1;
unsigned long long maybeone  = 1;

#define MAX_BUFFER_SIZE (16*1024)
#define QUEUE_SIZE 512

struct {
	char buf[MAX_BUFFER_SIZE];
        uint size;
        int  flow;
} queue[QUEUE_SIZE];

volatile unsigned int l_queue = 0;
volatile unsigned int r_queue = 0;
volatile unsigned int current_queue_size = QUEUE_SIZE;

pthread_mutex_t       queue_mutex;

int application_register(const char *app_name,
                         const char *app_instance,
                         const char *dif_name)
{
	unsigned int seqnum;

        seqnum = rinaIPCManager_requestApplicationRegistration(app_name,
                                                               app_instance,
                                                               dif_name);

        // Wait until app is registered
        rina_ipcevent *event = rinaIPCEventProducer_eventWait();
	while (event &&
               !(rinaIPCEvent_eventType(event) == 8 &&
                 rinaIPCEvent_sequenceNumber(event) == seqnum))
		event = rinaIPCEventProducer_eventWait();

	if (rinaBaseApplicationRegistrationResponseEvent_result(event) == 0) {
		rinaIPCManager_commitPendingRegistration(seqnum, dif_name);
		return 0; /* registered application */
	} else {
		rinaIPCManager_withdrawPendingRegistration(seqnum);
		return 1; /* failed to register application */
	}
}

rina_flow_t allocate_flow(const char *local_app_name,
                          const char *local_app_instance,
                          const char *remote_app_name,
                          const char *remote_app_instance,
                          const char *dif_name)
{
	int seqnum;
        // See if we need to allocate the flow in a specific DIF
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
	while (event &&
               !(rinaIPCEvent_eventType(event) == 1 &&
                 rinaIPCEvent_sequenceNumber(event) == seqnum)) {
		event = rinaIPCEventProducer_eventWait();
	}

	char *event_dif_name = rinaAllocateFlowRequestResultEvent_difName(event);

	rina_flow_t id;
        id = rinaIPCManager_commitPendingFlow(rinaIPCEvent_sequenceNumber(event),
                                              rinaAllocateFlowRequestResultEvent_portId(event),
                                              event_dif_name);
	free(event_dif_name);

	if (!id || rinaFlow_getPortId(id) == -1)
		return 0;
	return id;
}

const char *getDIF(const char *difname)
{
        if (!difname || !*difname)
                return 0;

        return difname;
}

void *rina_read_flow(void *flowptr)
{
        char buf[MAX_BUFFER_SIZE];
        rina_flow_t id = (rina_flow_t) flowptr;

        uint buf_size = rinaFlow_readSDU(id, buf, MAX_BUFFER_SIZE);
        while (buf_size > 0) {
                pthread_mutex_lock(&queue_mutex);
                if (current_queue_size == 0) {
                        pthread_mutex_unlock(&queue_mutex);
                        continue;
                }
                memcpy(queue[r_queue].buf, buf, buf_size);
                queue[r_queue].size = buf_size;
                queue[r_queue].flow = id;
                r_queue = (r_queue + 1) % QUEUE_SIZE;
                // Set FD to one
                write(rina_event, &one, sizeof(unsigned long long));
                current_queue_size++;
                pthread_mutex_unlock(&queue_mutex);

                buf_size = rinaFlow_readSDU(id, buf, MAX_BUFFER_SIZE);
        }

        return NULL;
}

void *rina_server_listen(void *para)
{
        printf("RINA: serverlisten\n");
	rina_ipcevent *event = rinaIPCEventProducer_eventWait();
	char *dif_name;
        rina_flow_t flow = 0;
        pthread_t flow_thread;
	while (event) {
                switch (rinaIPCEvent_eventType(event)) {
                case 8:
                        dif_name = rinaRegisterApplicationResponseEvent_DIFName(event);
                        rinaIPCManager_commitPendingRegistration(rinaIPCEvent_sequenceNumber(event),
                                                                 dif_name);
                        free(dif_name);
                        event = rinaIPCEventProducer_eventWait();
                        break;
                case 0:
                        flow = rinaIPCManager_allocateFlowResponse(event, 0, 1);
                        pthread_create(&flow_thread,
                                       NULL,
                                       rina_read_flow,
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
        pthread_mutex_init(&queue_mutex, NULL);

        if(rina_event == -1)
                rina_event = eventfd(0, EFD_SEMAPHORE);

        rina_initialize("INFO", "");

        if (server) {
                if (application_register(server_app_name,
                                         server_app_instance,
                                         getDIF(dif_name)) != 0)
                        return;
                pthread_t listen_thread;
                pthread_create(&listen_thread,
                               NULL,
                               rina_server_listen,
                               NULL);
                pthread_detach(listen_thread);
        }
}

void rina_resolve(const char *s, netadr_t *a)
{
        srand(time(NULL));
        int client_api = rand();
        char client_api_str[10];
        sprintf(client_api_str, "%d", client_api);
        a->type = NA_RINA;
        rina_flow_t flow = allocate_flow(client_app_name,
                                         client_api_str,
                                         server_app_name,
                                         server_app_instance,
                                         getDIF(dif_name));
        a->flow = flow;
        pthread_t flow_thread;
        pthread_create(&flow_thread, NULL, rina_read_flow, (void*) flow);
        pthread_detach(flow_thread);
}

int rina_recvfrom(msg_t *msg, netadr_t *from)
{
        pthread_mutex_lock(&queue_mutex);
        if (rina_read_event()) {
                // Set FD appropriately
                read(rina_event, &maybeone, sizeof(unsigned long long));
                from->type = NA_RINA;
                from->flow = queue[l_queue].flow;
                msg->cursize = MIN(queue[l_queue].size, msg->maxsize);
                memcpy(msg->data, queue[l_queue].buf, msg->cursize);
                l_queue= (l_queue + 1) % QUEUE_SIZE;
                current_queue_size--;
                pthread_mutex_unlock(&queue_mutex);

                return 0;
        }
        pthread_mutex_unlock(&queue_mutex);

        return -1;
}

void rina_sendto(int length, const void *data, netadr_t *to)
{
        rinaFlow_writeSDU(to->flow, (void*) data, length);
}

char rina_read_event(void)
{
        return l_queue != r_queue;
}

int rina_event = -1;//invalid_socket
