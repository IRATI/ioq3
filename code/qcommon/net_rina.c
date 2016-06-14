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
#include <stdlib.h>

#include <ouroboros/dev.h>

#define AE_NAME "QUAKE3"
#define FDS_SIZE 255

int fds[FDS_SIZE];

static void add_fd(int fd)
{
        int i;
        for (i = 0; i < FDS_SIZE; i++) {
                if (fds[i] == -1) {
                        fds[i] = fd;
                        break;
                }
        }
}

void RINA_Resolve(char * s, netadr_t * a)
{
        int fd;
        int result;

        a->type = NA_RINA;

        fd = flow_alloc(s, AE_NAME, NULL);
        if (fd < 0) {
                printf("Failed to allocate flow\n");
                return;
        }

        result = flow_alloc_res(fd);
        if (result < 0) {
                printf("Flow allocation refused\n");
                flow_dealloc(fd);
                return;
        }

        flow_cntl(fd, FLOW_F_SETFL, FLOW_O_NONBLOCK);

        a->fd = fd;

        add_fd(fd);
}

void RINA_Sendto(int length, const void * data, netadr_t * to)
{
        flow_write(to->fd, (void *) data, length);
}

int RINA_Recvfrom(msg_t * msg, netadr_t * from)
{
        ssize_t count = 0;
        int i         = 0;

        for (i = 0; i < FDS_SIZE; i++) {
                if (fds[i] == -1)
                        break;

                count = flow_read(fds[i], msg->data, msg->maxsize);
                if (count < 0) {
                        continue;
                }

                if (count > msg->maxsize) {
                        printf("Oversized packet received");
                        return 0;
                }

                from->type = NA_RINA;
                from->fd = fds[i];
                msg->cursize = count;
                msg->readcount = 0;

                return count;
        }

        return 0;
}

void * RINA_Server_Listen(void * o)
{
        int client_fd;
        char * client_ae = NULL;

        for (;;) {
                client_fd = flow_accept(&client_ae);
                if (client_fd < 0) {
                        printf("Failed to accept flow\n");
                        continue;
                }

                if(strcmp(client_ae, AE_NAME)) {
                        printf("Wrong client AE");
                        continue;
                }

                printf("New client.\n");

                if (flow_alloc_resp(client_fd, 0)) {
                        printf("Failed to give an allocate response\n");
                        flow_dealloc(client_fd);
                        continue;
                }

                flow_cntl(client_fd, FLOW_F_SETFL, FLOW_O_NONBLOCK);

                add_fd(client_fd);
        }

}

void RINA_Init(int server)
{
        pthread_t listen_thread;
        int i = 0;

        for (i = 0; i < FDS_SIZE; i++) {
                fds[i] = -1;
        }

        if (server) {

                if (ap_init("ioq3ded.x86_64")) {
                        printf("Failed to init.\n");
                        return;
                }

                pthread_create(&listen_thread,
                               NULL,
                               RINA_Server_Listen,
                               NULL);
                pthread_detach(listen_thread);
        } else {
                if (ap_init("ioquake3.x86_64")) {
                        printf("Failed to init.\n");
                        return;
                }
        }
}

void RINA_Fini(int server)
{
        ap_fini();
}
