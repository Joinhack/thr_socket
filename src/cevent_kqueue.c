#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

typedef struct {
	int kqfd;
	struct kevent events[MAX_EVENTS];
} kqueue_priv;

static int cevents_create_priv_impl(cevents *cevts) {
	kqueue_priv *priv = jmalloc(sizeof(kqueue_priv));
	memset(priv, 0, sizeof(kqueue_priv));
	priv->kqfd = kqueue();
	snprintf(cevts->impl_name, sizeof(cevts->impl_name), "%s", "kqueue");
	cevts->priv_data = priv;
	return 0;
}

static int cevents_destroy_priv_impl(cevents *cevts) {
	jfree(cevts->priv_data);
	cevts->priv_data = NULL;
	return 0;
}

static int cevents_add_event_impl(cevents *cevts, int fd, int mask) {
	kqueue_priv *priv = (kqueue_priv*)cevts->priv_data;
	struct kevent kevt;
	memset(&kevt, 0, sizeof(kevt));
	if (mask & CEV_READ) {
		EV_SET(&kevt, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
		if (kevent(priv->kqfd, &kevt, 1, NULL, 0, NULL) == -1) 
			return -1;
	}
	if (mask & CEV_WRITE) {
		EV_SET(&kevt, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
		if (kevent(priv->kqfd, &kevt, 1, NULL, 0, NULL) == -1) 
			return -1;
	}
	return 0;
}

static int cevents_del_event_impl(cevents *cevts, int fd, int delmask) {
	kqueue_priv *priv = (kqueue_priv*)cevts->priv_data;
	struct kevent kevt;
	int mask = (cevts->events + fd)->mask;
	memset(&kevt, 0, sizeof(kevt));
	if (delmask & CEV_READ) {
		EV_SET(&kevt, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		kevent(priv->kqfd, &kevt, 1, NULL, 0, NULL);
	}
	if (delmask & CEV_WRITE) {
		EV_SET(&kevt, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		kevent(priv->kqfd, &kevt, 1, NULL, 0, NULL);
	}
	return 0;
}

static int cevents_poll_impl(cevents *cevts, msec_t ms) {
	kqueue_priv *priv = (kqueue_priv*)cevts->priv_data;
	int rs, i, mask, count = 0;
	cevent_fired *fired;
	struct kevent *kevt;
	struct timespec timeout;
	timeout.tv_sec = (long) (ms / 1000);
	timeout.tv_nsec = (long) (ms % 1000);
	rs = kevent(priv->kqfd, NULL, 0, priv->events, MAX_EVENTS, &timeout);
	if(rs > 0) {
		for(i = 0; i < rs; i++) {
			mask = CEV_NONE;
			kevt = priv->events + i;
			fired = cevts->fired + count;
			if(kevt->filter == EVFILT_READ)
				mask |= CEV_READ;
			if(kevt->filter == EVFILT_WRITE)
				mask |= CEV_WRITE;
			fired->fd = kevt->ident;
			fired->mask = mask;
			count++;
		}
	}
	return count;
}
