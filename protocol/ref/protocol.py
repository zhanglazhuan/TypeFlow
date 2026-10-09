"""TypeFlow 线协议 v1 —— Python 参考实现。

这是 host/protocol/typeflow_proto.c 的等价移植，用于开发期测试。
规范见 ../protocol/PROTOCOL.md；改协议要同步三处（.c / .py / BOOX 的 Kotlin）。
"""

import struct
from enum import IntEnum

MAGIC = 0x4654          # 线上字节 'T','F'（小端 uint16 即 0x4654）
VERSION = 1
HEADER_LEN = 6
CRC_LEN = 2
MAX_PAYLOAD = 1024
MAX_FRAME = HEADER_LEN + MAX_PAYLOAD + CRC_LEN
TOKEN_LEN = 16

DEFAULT_PORT = 14500


class Type(IntEnum):
    HELLO       = 0x01
    HELLO_ACK   = 0x02
    COMMIT_TEXT = 0x10
    COMMIT_ACK  = 0x11
    KEY         = 0x20
    PING        = 0x30
    PONG        = 0x31


class Brand(IntEnum):
    UNKNOWN    = 0
    REMARKABLE = 1
    BOOX       = 2
    GENERIC_PC = 3


CAP_COMMIT_TEXT  = 1 << 0
CAP_KEY_PASSTHRU = 1 << 1
CAP_FOCUS_REPORT = 1 << 2
CAP_BAR_STATE    = 1 << 3


class Result(IntEnum):
    OK       = 0
    NO_FOCUS = 1
    TOO_LONG = 2
    INTERNAL = 3


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, 不反转, 不异或输出.

    标准校验值: crc16(b"123456789") == 0x29B1
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(frame_type: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload 超限: {len(payload)} > {MAX_PAYLOAD}")
    head = struct.pack("<HBBH", MAGIC, VERSION, frame_type, len(payload))
    body = head + payload
    return body + struct.pack("<H", crc16(body))


def decode(buf: bytes):
    """尝试从字节流解出一帧。

    返回 (frame, consumed)。frame 为 None 表示没解出来 ——
    此时 consumed 可能 >0，表示应从缓冲丢弃的垃圾字节数。
    对应 C 版的 tf_decode，重同步行为必须保持一致。
    """
    magic_lo = MAGIC & 0xFF
    magic_hi = MAGIC >> 8

    # 找 magic
    start = 0
    while start + 2 <= len(buf):
        if buf[start] == magic_lo and buf[start + 1] == magic_hi:
            break
        start += 1
    else:
        # 没找到。保留最后 1 字节：可能是被切断的 magic 首字节。
        return None, max(0, len(buf) - 1)

    if start + 2 > len(buf):
        return None, max(0, len(buf) - 1)
    if start > 0:
        return None, start                      # 先丢垃圾，下次再解

    if len(buf) < HEADER_LEN:
        return None, 0                          # 头没收全

    if buf[2] != VERSION:
        return None, 2                          # 版本不认识，跳过 magic

    plen = buf[4] | (buf[5] << 8)
    if plen > MAX_PAYLOAD:
        return None, 2                          # 长度离谱，当误同步

    total = HEADER_LEN + plen + CRC_LEN
    if len(buf) < total:
        return None, 0                          # 载荷没收全

    want = crc16(buf[:HEADER_LEN + plen])
    got = buf[HEADER_LEN + plen] | (buf[HEADER_LEN + plen + 1] << 8)
    if want != got:
        return None, 2                          # 校验错，重新同步

    return (buf[3], bytes(buf[HEADER_LEN:HEADER_LEN + plen])), total
