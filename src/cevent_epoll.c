#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include "jmalloc.h"

typedef struct {
	int epfd;
	struct epoll_event events[MAX_EVENTS];
} epoll_priv;

static int cevents_create_priv_impl(cevents *cevts) {
	epoll_priv *priv = jmalloc(sizeof(epoll_priv));
	memset(priv, 0, sizeof(epoll_priv));
	priv->epfd = epoll_create(1024);
	snprintf(cevts->impl_name, sizeof(cevts->impl_name), "%s", "epoll");
	cevts->priv_data = priv;
}

static int cevents_destroy_priv_impl(cevents *cevts) {
	jfree(cevts->priv_data);
	cevts->priv_data = NULL;
	return 0;
}

static int cevents_add_event_impl(cevents *cevts, int fd, int mask) {
	int operation = EPOLL_CTL_ADD;
	int old_mask = cevts->events[fd].mask;
	struct epoll_event ep_event;
	memset(&ep_event, 0, sizeof(struct epoll_event));
	epoll_priv *priv = (epoll_priv*)cevts->priv_data;
	if(old_mask != CEV_NONE)
		operation = EPOLL_CTL_MOD;
	//set old mask;
	mask |= old_mask;
	if(mask & CEV_READ) ep_event.events |= EPOLLIN;
	if(mask & CEV_WRITE) ep_event.events |= EPOLLOUT;
	ep_event.data.fd = fd;
	return epoll_ctl(priv->epfd, operation, fd, &ep_event);
}

static int cevents_del_event_impl(cevents *cevts, int fd, int mask) {
	int operation = EPOLL_CTL_DEL;
	epoll_priv *priv = (epoll_priv*)cevts->priv_data;
	int old_mask = cevts->events[fd].mask;
	struct epoll_event ep_event;
	memset(&ep_event, 0, sizeof(struct epoll_event));
	old_mask &= ~mask;
	if(old_mask & CEV_READ) ep_event.events |= EPOLLIN;
	if(old_mask & CEV_WRITE) ep_event.events |= EPOLLOUT;
	if(old_mask != CEV_NONE)
		operation = EPOLL_CTL_MOD;
	ep_event.data.fd = fd;
	return epoll_ctl(priv->epfd, operation, fd, &ep_event);
}


static int cevents_poll_impl(cevents *cevts, msec_t ms) {
	epoll_priv *priv = (epoll_priv*)cevts->priv_data;
	int rs, i, mask, count = 0;
	struct epoll_event *ep_event;
	cevent_fired *fired;
	rs = epoll_wait(priv->epfd, priv->events, MAX_EVENTS, ms);
	if(rs > 0) {
		for(i = 0; i < rs; i++) {
			mask = CEV_NONE;
			ep_event = priv->events + i;
			fired = cevts->fired + count;
			if (ep_event->events & EPOLLIN) 
				mask |= CEV_READ;
			if (ep_event->events & EPOLLOUT) 
				mask |= CEV_WRITE;
			fired->fd = ep_event->data.fd;
			fired->mask = mask;
			count++;
		}
	}
	return count;
}

