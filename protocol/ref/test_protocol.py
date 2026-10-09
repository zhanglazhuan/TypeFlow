"""协议实现自测。无需硬件，直接 python test_protocol.py。

重点测 decode 的流式重同步：垃圾字节、被切断的帧、CRC 错、长度离谱 ——
这些是 TCP/CDC 上真实会遇到的情况，也是最容易写错的部分。
"""

import protocol as p

fails = []


def check(name, cond):
    print(("  PASS  " if cond else "  FAIL  ") + name)
    if not cond:
        fails.append(name)


print("CRC-16/CCITT-FALSE")
# 标准校验值。C 版 tf_crc16 用同样的 poly/init/不反转/不异或，必须一致。
check("标准向量 crc16('123456789') == 0x29B1", p.crc16(b"123456789") == 0x29B1)
check("空输入 == 0xFFFF", p.crc16(b"") == 0xFFFF)

print("\n编码")
f = p.encode(p.Type.COMMIT_TEXT, "你好".encode())
check("帧长 = 6 头 + 6 载荷 + 2 校验", len(f) == 14)
check("magic 小端 'TF'", f[0:2] == b"TF")
check("version", f[2] == 1)
check("type", f[3] == 0x10)
check("payload_len 小端", f[4] == 6 and f[5] == 0)
check("载荷是 UTF-8 原文", f[6:12] == "你好".encode())

print("\n解码：正常")
frame, used = p.decode(f)
check("解出一帧", frame is not None)
check("消耗整帧", used == len(f))
check("类型正确", frame and frame[0] == 0x10)
check("载荷还原", frame and frame[1].decode() == "你好")

print("\n解码：空载荷")
ping = p.encode(p.Type.PING)
frame, used = p.decode(ping)
check("PING 可解", frame is not None and frame[0] == 0x30)
check("PING 载荷为空", frame and frame[1] == b"")

print("\n解码：流式")
frame, used = p.decode(f[:3])
check("头不全 -> 不解出且不消耗", frame is None and used == 0)
frame, used = p.decode(f[:10])
check("载荷不全 -> 不解出且不消耗", frame is None and used == 0)

two = f + ping
frame, used = p.decode(two)
check("连包：先解出第一帧", frame is not None and used == len(f))
frame2, used2 = p.decode(two[used:])
check("连包：再解出第二帧", frame2 is not None and frame2[0] == 0x30)

print("\n解码：重同步")
junk = b"\x01\x02\x03" + f
frame, used = p.decode(junk)
check("前置垃圾 -> 报告丢弃字节数", frame is None and used == 3)
frame, used = p.decode(junk[3:])
check("丢弃后可解", frame is not None and frame[1].decode() == "你好")

bad = bytearray(f)
bad[-1] ^= 0xFF
frame, used = p.decode(bytes(bad))
check("CRC 错 -> 跳过 magic 重同步", frame is None and used == 2)

badlen = bytearray(f)
badlen[4], badlen[5] = 0xFF, 0xFF      # plen = 65535 > MAX_PAYLOAD
frame, used = p.decode(bytes(badlen))
check("长度离谱 -> 跳过 magic", frame is None and used == 2)

badver = bytearray(f)
badver[2] = 99
frame, used = p.decode(bytes(badver))
check("版本不认识 -> 跳过 magic", frame is None and used == 2)

frame, used = p.decode(b"\x54")
check("单个 magic 首字节 -> 全部保留", frame is None and used == 0)

frame, used = p.decode(b"\xAA\xBB\xCC")
check("纯垃圾 -> 保留最后 1 字节", frame is None and used == 2)

print("\n边界")
try:
    p.encode(p.Type.COMMIT_TEXT, b"x" * (p.MAX_PAYLOAD + 1))
    check("超长载荷应报错", False)
except ValueError:
    check("超长载荷报错", True)

big = p.encode(p.Type.COMMIT_TEXT, b"x" * p.MAX_PAYLOAD)
frame, used = p.decode(big)
check("最大载荷可往返", frame is not None and len(frame[1]) == p.MAX_PAYLOAD)
check("最大帧长符合 MAX_FRAME", len(big) == p.MAX_FRAME)

print()
if fails:
    print(f"{len(fails)} 项失败: {fails}")
    raise SystemExit(1)
print("全部通过")
