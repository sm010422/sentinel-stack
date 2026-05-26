/**
 * linker.ld — ARM Cortex-M3 링커 스크립트 (QEMU mps2-an385)
 *
 * 메모리 맵 (mps2-an385):
 *   FLASH: 0x00000000 ~ 0x003FFFFF (4MB)
 *   RAM:   0x20000000 ~ 0x2007FFFF (512KB)
 *
 * Cortex-M3 벡터 테이블:
 *   0x00000000: 초기 스택 포인터
 *   0x00000004: Reset 핸들러
 *   0x00000008 ~: 인터럽트 벡터
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* ── 벡터 테이블 (FLASH 시작) ─────────────────────── */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* ── 코드 섹션 ─────────────────────────────────────── */
    .text :
    {
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ── ARM 예외 처리 테이블 ──────────────────────────── */
    .ARM.extab :
    {
        *(.ARM.extab*)
        *(.gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* ── 초기화 데이터 (FLASH에 저장, RAM으로 복사) ──── */
    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* ── 미초기화 데이터 (RAM, 0으로 초기화) ─────────── */
    .bss :
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* ── 스택 (RAM 끝에서 위로 성장) ─────────────────── */
    ._stack :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + 0x4000;  /* 16KB 스택 */
        . = ALIGN(8);
        _estack = .;
    } > RAM

    /* ── 힙 (MISRA-C: 사용 금지) ─────────────────────── */
    _heap_start = _ebss;
    _heap_end   = _sstack;

    /* ── 심볼 익스포트 ────────────────────────────────── */
    PROVIDE(end = _ebss);
}
