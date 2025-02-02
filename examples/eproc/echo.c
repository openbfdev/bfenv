/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2024 John Sanpe <sanpeqf@gmail.com>
 */

#include <bfenv/eproc.h>
#include <bfdev/log.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static int
client_echo(bfenv_eproc_event_t *event, void *pdata)
{
    char buff[1024];
    ssize_t length;
    int retval;

    length = recv(event->fd, buff, sizeof(buff), 0);
    if (length > 0) {
        retval = send(event->fd, buff, length, 0);
        if (retval < 0)
            return retval;
        return 0;
    }

    bfdev_log_info("Connection closed by peer\n");
    shutdown(event->fd, 0);

    bfenv_eproc_event_remove(event->eproc, event);
    free(event);

    return 0;
}

static int
server_accept(bfenv_eproc_event_t *event, void *pdata)
{
    bfenv_eproc_event_t *cevent;
    struct sockaddr_in caddr;
    socklen_t socklen;
    int csock;

    socklen = sizeof(caddr);
    csock = accept(event->fd, (struct sockaddr *)&caddr, &socklen);
    if (csock < 0)
        return csock;

    cevent = malloc(sizeof(*cevent));
    if (!cevent)
        return 1;

    cevent->fd = csock;
    cevent->flags = BFENV_EPROC_READ;
    cevent->func = client_echo;

    bfdev_log_info("Connected by %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    return bfenv_eproc_event_add(event->eproc, cevent);
}

int
main(void)
{
    bfenv_eproc_t *eproc;
    bfenv_eproc_event_t event;
    struct sockaddr_in saddr;
    int ssock, retval;

    ssock = socket(PF_INET, SOCK_STREAM, 0);
    if (ssock < 0)
        return ssock;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(7410);

    if ((retval = bind(ssock, (struct sockaddr *)&saddr, sizeof(saddr))) ||
        (retval = listen(ssock, 32)))
        return retval;

    bfdev_log_info("Echo server is running on port %d\n", ntohs(saddr.sin_port));
    eproc = bfenv_eproc_create(NULL, "select");
    if (!eproc)
        return 1;

    event.fd = ssock;
    event.flags = BFENV_EPROC_READ;
    event.func = server_accept;

    retval = bfenv_eproc_event_add(eproc, &event);
    if (retval)
        return retval;

    retval = bfenv_eproc_run(eproc, -1);
    if (retval)
        return retval;

    shutdown(ssock, 0);
    bfenv_eproc_destory(eproc);

    return 0;
}
