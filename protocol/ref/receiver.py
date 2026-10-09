#!/usr/bin/env python3
"""TypeFlow 参考接收器 —— 开发期用的对端。

用途：让固件侧不必等 reMarkable 的 XOVI 扩展写好，就能在 PC 上跑通
      Wi-Fi → 协议 → 文本 这条完整链路。

它不注入任何东西，只把收到的文本打到终端（Windows 上也能跑）。
真正的注入实现在各品牌目录下：
    ../remarkable/xovi-ext/   Qt postEvent（见其 SPIKE.md）
    ../boox/ime/             Android commitText()

用法：
    python receiver.py                 # 监听 0.0.0.0:14500
    python receiver.py --port 9000
    python receiver.py --no-focus      # 模拟「没有聚焦输入框」，回 NO_FOCUS
    python receiver.py --no-commit     # 模拟「接收器不支持文本提交」，测固件降级路径
"""

import argparse
import socket
import sys

import protocol as p

RECEIVER_VERSION = "0.1.0"


def handle_client(conn, addr, args):
    print(f"[+] 键盘已连接 {addr[0]}:{addr[1]}")
    buf = bytearray()
    committed = []

    try:
        while True:
            chunk = conn.recv(4096)
            if not chunk:
                break
            buf += chunk

            # 反复解析直到解不出更多。decode 即使失败也会报告要丢弃多少字节，
            # 所以这个循环不会死转 —— 和 C 版 hidout.c 的 on_rx 逻辑一致。
            while True:
                frame, used = p.decode(bytes(buf))
                if frame is not None:
                    reply = dispatch(frame, args, committed)
                    if reply:
                        conn.sendall(reply)
                if used == 0:
                    break
                del buf[:used]
                if not buf:
                    break
    except ConnectionResetError:
        print("[!] 连接被对端重置")
    finally:
        conn.close()
        print(f"[-] 断开 {addr[0]}，本次共收到 {len(committed)} 段文本")
        if committed:
            print(f"    全文: {''.join(committed)}")


def dispatch(frame, args, committed):
    ftype, payload = frame

    if ftype == p.Type.HELLO:
        token = payload[:p.TOKEN_LEN]
        version = payload[p.TOKEN_LEN:].decode("utf-8", "replace")
        print(f"    HELLO  固件版本={version!r} token={token.hex()[:8]}…")

        caps = p.CAP_KEY_PASSTHRU | p.CAP_FOCUS_REPORT
        if not args.no_commit:
            caps |= p.CAP_COMMIT_TEXT
        else:
            print("    （--no-commit：故意不声明 COMMIT_TEXT 能力，测固件降级）")

        ack = bytes([p.Brand.GENERIC_PC]) + caps.to_bytes(4, "little") \
            + RECEIVER_VERSION.encode()
        return p.encode(p.Type.HELLO_ACK, ack)

    if ftype == p.Type.COMMIT_TEXT:
        text = payload.decode("utf-8", "replace")
        if args.no_focus:
            print(f"    COMMIT {text!r} -> NO_FOCUS（模拟没有聚焦输入框）")
            return p.encode(p.Type.COMMIT_ACK, bytes([p.Result.NO_FOCUS]))
        committed.append(text)
        print(f"    COMMIT {text!r}")
        return p.encode(p.Type.COMMIT_ACK, bytes([p.Result.OK]))

    if ftype == p.Type.KEY:
        if len(payload) >= 2:
            print(f"    KEY    modifiers=0x{payload[0]:02X} keycode=0x{payload[1]:02X}")
        return None

    if ftype == p.Type.PING:
        return p.encode(p.Type.PONG)

    if ftype == p.Type.PONG:
        return None

    print(f"    ?      未知帧类型 0x{ftype:02X}，已忽略")
    return None


def main():
    ap = argparse.ArgumentParser(description="TypeFlow 参考接收器")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=p.DEFAULT_PORT)
    ap.add_argument("--no-focus", action="store_true",
                    help="所有 COMMIT_TEXT 都回 NO_FOCUS")
    ap.add_argument("--no-commit", action="store_true",
                    help="握手时不声明 COMMIT_TEXT 能力")
    args = ap.parse_args()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(1)
    print(f"TypeFlow 参考接收器 v{RECEIVER_VERSION}")
    print(f"监听 {args.host}:{args.port}，等键盘连接… (Ctrl-C 退出)")

    try:
        while True:
            conn, addr = srv.accept()
            handle_client(conn, addr, args)
    except KeyboardInterrupt:
        print("\n退出")
    finally:
        srv.close()


if __name__ == "__main__":
    sys.exit(main())
