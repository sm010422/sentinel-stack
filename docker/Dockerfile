# =============================================================================
# sentinel-stack Linux 빌드 환경
#
# Ubuntu 22.04 LTS 기반 빌드 이미지입니다.
# NET_ADMIN, NET_RAW 권한 필요 (패킷 캡처)
#
# 빌드:
#   docker build -t sentinel-stack ./docker/
#
# 실행:
#   docker run --rm --cap-add=NET_ADMIN --cap-add=NET_RAW sentinel-stack
# =============================================================================

FROM ubuntu:22.04

LABEL maintainer="Park Sang Min <sm0104224@gmail.com>"
LABEL description="sentinel-stack: 전술 통신 미들웨어 스택 빌드 환경"
LABEL version="1.0"

# 비대화형 모드
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul

# ── 의존성 설치 ──────────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    # 빌드 도구
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    # 암호화
    libssl-dev \
    # 패킷 캡처
    libpcap-dev \
    # 수학 라이브러리
    libm-dev 2>/dev/null || true \
    # 분석 도구
    valgrind \
    # 유틸리티
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# ── 작업 디렉토리 ────────────────────────────────────────────────────────────
WORKDIR /sentinel-stack

# ── 소스 복사 ────────────────────────────────────────────────────────────────
COPY . .

# ── 빌드 ─────────────────────────────────────────────────────────────────────
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

# ── 테스트 실행 ──────────────────────────────────────────────────────────────
RUN ctest --test-dir build --output-on-failure || true

# ── 기본 명령 ────────────────────────────────────────────────────────────────
CMD ["./build/rtos-scheduler/rtos_sim", "--algo", "rms", "--tasks", "5", "--duration", "10s"]
