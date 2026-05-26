/**
 * @file io_kqueue.c
 * @brief macOS/BSD kqueue 기반 I/O 멀티플렉서 구현
 *
 * macOS(BSD) 전용 kqueue 인터페이스를 사용합니다.
 * Linux에서는 io_epoll.c 가 사용됩니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifdef __APPLE__

#include "io_multiplexer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

int io_init(io_ctx_t *ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    (void)memset(ctx, 0, sizeof(io_ctx_t));

    ctx->mux_fd = kqueue();
    if (ctx->mux_fd < 0) {
        perror("[KQUEUE] kqueue 생성 실패");
        return -1;
    }

    ctx->initialized = true;
    printf("[IO] kqueue 멀티플렉서 초기화 완료 (fd=%d)\n", ctx->mux_fd);
    return 0;
}

int io_add_fd(io_ctx_t *ctx, int fd, uint32_t events, void *userdata)
{
    struct kevent changelist[2];
    int           nchanges = 0;
    uint32_t      i;

    if (ctx == NULL || !ctx->initialized || fd < 0) {
        return -1;
    }

    if (ctx->fd_count >= IO_MAX_FDS) {
        fprintf(stderr, "[KQUEUE] fd 풀 포화\n");
        return -1;
    }

    if ((events & IO_EVENT_READ) != 0U) {
        EV_SET(&changelist[nchanges], (uintptr_t)fd,
               EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, userdata);
        nchanges++;
    }

    if ((events & IO_EVENT_WRITE) != 0U) {
        EV_SET(&changelist[nchanges], (uintptr_t)fd,
               EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, userdata);
        nchanges++;
    }

    if (kevent(ctx->mux_fd, changelist, nchanges, NULL, 0, NULL) != 0) {
        perror("[KQUEUE] kevent ADD 실패");
        return -1;
    }

    for (i = 0U; i < IO_MAX_FDS; i++) {
        if (ctx->watched_fds[i] == 0) {
            ctx->watched_fds[i] = fd;
            ctx->userdata[i]    = userdata;
            break;
        }
    }
    ctx->fd_count++;

    return 0;
}

int io_remove_fd(io_ctx_t *ctx, int fd)
{
    struct kevent changelist[2];
    uint32_t      i;

    if (ctx == NULL || !ctx->initialized || fd < 0) {
        return -1;
    }

    EV_SET(&changelist[0], (uintptr_t)fd, EVFILT_READ,  EV_DELETE, 0, 0, NULL);
    EV_SET(&changelist[1], (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    /* 오류 무시 (등록되지 않은 필터 삭제 시 발생 가능) */
    (void)kevent(ctx->mux_fd, changelist, 2, NULL, 0, NULL);

    for (i = 0U; i < IO_MAX_FDS; i++) {
        if (ctx->watched_fds[i] == fd) {
            ctx->watched_fds[i] = 0;
            ctx->userdata[i]    = NULL;
            if (ctx->fd_count > 0U) {
                ctx->fd_count--;
            }
            break;
        }
    }

    return 0;
}

int io_wait(io_ctx_t *ctx, io_event_t *events, int max_events, int timeout_ms)
{
    struct kevent kev[IO_MAX_EVENTS];
    struct timespec ts;
    struct timespec *tsp;
    int              n;
    int              i;

    if (ctx == NULL || !ctx->initialized || events == NULL || max_events <= 0) {
        return -1;
    }

    if (max_events > (int)IO_MAX_EVENTS) {
        max_events = (int)IO_MAX_EVENTS;
    }

    if (timeout_ms < 0) {
        tsp = NULL; /* 무한 대기 */
    } else {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }

    n = kevent(ctx->mux_fd, NULL, 0, kev, max_events, tsp);

    if (n < 0) {
        if (errno != EINTR) {
            perror("[KQUEUE] kevent 대기 실패");
        }
        return -1;
    }

    for (i = 0; i < n; i++) {
        events[i].fd       = (int)kev[i].ident;
        events[i].events   = 0U;
        events[i].userdata = kev[i].udata;

        if (kev[i].filter == EVFILT_READ) {
            events[i].events |= IO_EVENT_READ;
        }
        if (kev[i].filter == EVFILT_WRITE) {
            events[i].events |= IO_EVENT_WRITE;
        }
        if ((kev[i].flags & EV_ERROR) != 0U) {
            events[i].events |= IO_EVENT_ERROR;
        }
        if ((kev[i].flags & EV_EOF) != 0U) {
            events[i].events |= IO_EVENT_ERROR;
        }
    }

    return n;
}

void io_close(io_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    if (ctx->mux_fd >= 0) {
        (void)close(ctx->mux_fd);
        ctx->mux_fd = -1;
    }

    ctx->initialized = false;
    ctx->fd_count    = 0U;
}

#endif /* __APPLE__ */
