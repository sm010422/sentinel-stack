/**
 * @file io_epoll.c
 * @brief Linux epoll 기반 I/O 멀티플렉서 구현
 *
 * Linux 전용 epoll 인터페이스를 사용합니다.
 * macOS에서는 io_kqueue.c 가 사용됩니다.
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifdef __linux__

#include "io_multiplexer.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/epoll.h>

/* =========================================================================
 * 구현
 * ========================================================================= */

int io_init(io_ctx_t *ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    (void)memset(ctx, 0, sizeof(io_ctx_t));

    ctx->mux_fd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx->mux_fd < 0) {
        perror("[EPOLL] epoll_create1 실패");
        return -1;
    }

    ctx->initialized = true;
    printf("[IO] epoll 멀티플렉서 초기화 완료 (fd=%d)\n", ctx->mux_fd);
    return 0;
}

int io_add_fd(io_ctx_t *ctx, int fd, uint32_t events, void *userdata)
{
    struct epoll_event ev;
    uint32_t           i;

    if (ctx == NULL || !ctx->initialized || fd < 0) {
        return -1;
    }

    if (ctx->fd_count >= IO_MAX_FDS) {
        fprintf(stderr, "[EPOLL] fd 풀 포화\n");
        return -1;
    }

    (void)memset(&ev, 0, sizeof(ev));

    if ((events & IO_EVENT_READ) != 0U) {
        ev.events |= (uint32_t)EPOLLIN;
    }
    if ((events & IO_EVENT_WRITE) != 0U) {
        ev.events |= (uint32_t)EPOLLOUT;
    }
    ev.events |= (uint32_t)EPOLLERR | (uint32_t)EPOLLHUP;
    ev.data.fd = fd;

    if (epoll_ctl(ctx->mux_fd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        perror("[EPOLL] epoll_ctl ADD 실패");
        return -1;
    }

    /* fd 목록에 저장 */
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
    uint32_t i;

    if (ctx == NULL || !ctx->initialized || fd < 0) {
        return -1;
    }

    if (epoll_ctl(ctx->mux_fd, EPOLL_CTL_DEL, fd, NULL) != 0) {
        perror("[EPOLL] epoll_ctl DEL 실패");
        return -1;
    }

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
    struct epoll_event ep_events[IO_MAX_EVENTS];
    int                n;
    int                i;
    uint32_t           j;

    if (ctx == NULL || !ctx->initialized || events == NULL || max_events <= 0) {
        return -1;
    }

    if (max_events > (int)IO_MAX_EVENTS) {
        max_events = (int)IO_MAX_EVENTS;
    }

    n = epoll_wait(ctx->mux_fd, ep_events, max_events, timeout_ms);

    if (n < 0) {
        if (errno != EINTR) {
            perror("[EPOLL] epoll_wait 실패");
        }
        return -1;
    }

    /* epoll 결과를 공통 io_event_t 형식으로 변환 */
    for (i = 0; i < n; i++) {
        events[i].fd     = ep_events[i].data.fd;
        events[i].events = 0U;

        if ((ep_events[i].events & (uint32_t)EPOLLIN) != 0U) {
            events[i].events |= IO_EVENT_READ;
        }
        if ((ep_events[i].events & (uint32_t)EPOLLOUT) != 0U) {
            events[i].events |= IO_EVENT_WRITE;
        }
        if ((ep_events[i].events & ((uint32_t)EPOLLERR | (uint32_t)EPOLLHUP)) != 0U) {
            events[i].events |= IO_EVENT_ERROR;
        }

        /* userdata 복원 */
        events[i].userdata = NULL;
        for (j = 0U; j < IO_MAX_FDS; j++) {
            if (ctx->watched_fds[j] == ep_events[i].data.fd) {
                events[i].userdata = ctx->userdata[j];
                break;
            }
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

#endif /* __linux__ */
