/**
 * @file io_multiplexer.h
 * @brief I/O 멀티플렉서 플랫폼 추상화 레이어
 *
 * Linux의 epoll과 macOS/BSD의 kqueue를 동일한 인터페이스로 감쌉니다.
 * 컴파일 타임에 플랫폼을 감지하여 적절한 구현을 선택합니다.
 *
 * 지원 이벤트:
 *  - IO_EVENT_READ:  읽기 가능 (데이터 수신 등)
 *  - IO_EVENT_WRITE: 쓰기 가능 (연결 완료 등)
 *  - IO_EVENT_ERROR: 오류 발생
 *
 * 사용 예:
 * @code
 *   io_ctx_t io;
 *   io_init(&io);
 *   io_add_fd(&io, tcp_fd, IO_EVENT_READ, NULL);
 *   io_add_fd(&io, udp_fd, IO_EVENT_READ, NULL);
 *
 *   io_event_t events[16];
 *   int n = io_wait(&io, events, 16, 1000);  // 1초 타임아웃
 *   for (int i = 0; i < n; i++) {
 *       handle_event(&events[i]);
 *   }
 * @endcode
 *
 * @author  Park Sang Min
 * @date    2025
 */

#ifndef SENTINEL_IO_MULTIPLEXER_H
#define SENTINEL_IO_MULTIPLEXER_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * 이벤트 타입
 * ========================================================================= */

/** 읽기 가능 이벤트 */
#define IO_EVENT_READ   (1U << 0)

/** 쓰기 가능 이벤트 */
#define IO_EVENT_WRITE  (1U << 1)

/** 오류 이벤트 */
#define IO_EVENT_ERROR  (1U << 2)

/** 최대 감시 fd 수 */
#define IO_MAX_FDS      64U

/** io_wait() 최대 반환 이벤트 수 */
#define IO_MAX_EVENTS   32U

/* =========================================================================
 * 이벤트 구조체
 * ========================================================================= */

/**
 * @brief 발생한 I/O 이벤트 정보
 */
typedef struct {
    int      fd;        /**< 이벤트가 발생한 파일 디스크립터 */
    uint32_t events;    /**< 발생한 이벤트 비트마스크 (IO_EVENT_*) */
    void    *userdata;  /**< io_add_fd() 에 등록한 사용자 데이터 */
} io_event_t;

/* =========================================================================
 * I/O 컨텍스트 (플랫폼별 내부 구현)
 * ========================================================================= */

/**
 * @brief I/O 멀티플렉서 컨텍스트
 *
 * 내부 구현은 플랫폼에 따라 다르지만 공개 인터페이스는 동일합니다.
 */
typedef struct {
    int      mux_fd;                      /**< epoll_fd (Linux) / kqueue_fd (macOS) */
    int      watched_fds[IO_MAX_FDS];     /**< 감시 중인 fd 목록 */
    void    *userdata[IO_MAX_FDS];        /**< fd별 사용자 데이터 */
    uint32_t fd_count;                    /**< 감시 fd 수 */
    bool     initialized;
} io_ctx_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief I/O 멀티플렉서 초기화
 *
 * Linux: epoll_create1(0)
 * macOS: kqueue()
 *
 * @param ctx 초기화할 컨텍스트
 * @return 0 성공, -1 실패
 */
int io_init(io_ctx_t *ctx);

/**
 * @brief fd 감시 등록
 *
 * @param ctx      I/O 컨텍스트
 * @param fd       감시할 파일 디스크립터
 * @param events   감시할 이벤트 비트마스크 (IO_EVENT_READ | IO_EVENT_WRITE)
 * @param userdata 이벤트 발생 시 반환할 사용자 데이터
 * @return 0 성공, -1 실패
 */
int io_add_fd(io_ctx_t *ctx, int fd, uint32_t events, void *userdata);

/**
 * @brief fd 감시 해제
 *
 * @param ctx I/O 컨텍스트
 * @param fd  해제할 파일 디스크립터
 * @return 0 성공, -1 실패
 */
int io_remove_fd(io_ctx_t *ctx, int fd);

/**
 * @brief 이벤트 대기 (블로킹)
 *
 * @param ctx         I/O 컨텍스트
 * @param events      [출력] 발생한 이벤트 배열
 * @param max_events  events 배열의 최대 크기
 * @param timeout_ms  타임아웃 (ms, 음수이면 무한 대기)
 * @return 발생한 이벤트 수 (≥0), -1 오류
 */
int io_wait(io_ctx_t *ctx, io_event_t *events, int max_events,
            int timeout_ms);

/**
 * @brief I/O 멀티플렉서 해제
 *
 * @param ctx I/O 컨텍스트
 */
void io_close(io_ctx_t *ctx);

#endif /* SENTINEL_IO_MULTIPLEXER_H */
