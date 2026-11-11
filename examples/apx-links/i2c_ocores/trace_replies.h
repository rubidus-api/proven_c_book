/* 출력 백엔드의 대답 표. 상태 레지스터(0x1003_0010)는 언제나 「전송 끝, ACK 받음」(0)
   이라고 대답하고, 받은 바이트(0x1003_000C)는 0x19 와 0x80 을 낸다 --- 25.5 도다. */
static const struct { uintptr_t addr; unsigned n; uint32_t values[2]; } replies[] = {
    { 0x10030010u, 1, { 0x00u } },
    { 0x1003000Cu, 2, { 0x19u, 0x80u } },
};
