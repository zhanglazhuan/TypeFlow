"""端到端自测：起接收器 + 模拟键盘，验证完整握手与提交流程。

python test_e2e.py
"""
import socket, subprocess, sys, threading, time
import protocol as p

PORT = 14577
fails = []

def check(name, cond):
    print(("  PASS  " if cond else "  FAIL  ") + name)
    if not cond: fails.append(name)

# -u 必须有：子进程 stdout 不是 tty 时 Python 会缓冲，日志抓不到
srv = subprocess.Popen([sys.executable, "-u", "receiver.py", "--port", str(PORT)],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, encoding="utf-8", errors="replace")
out_lines = []
def drain():
    for line in srv.stdout:
        out_lines.append(line.rstrip())
threading.Thread(target=drain, daemon=True).start()

try:
    # 等监听就绪
    sock = None
    for _ in range(50):
        try:
            sock = socket.create_connection(("127.0.0.1", PORT), timeout=2)
            break
        except OSError:
            time.sleep(0.1)
    check("接收器可连接", sock is not None)
    if not sock:
        raise SystemExit(1)

    buf = bytearray()
    def recv_frame(timeout=2.0):
        sock.settimeout(timeout)
        deadline = time.time() + timeout
        while time.time() < deadline:
            frame, used = p.decode(bytes(buf))
            if frame is not None:
                del buf[:used]
                return frame
            if used: del buf[:used]
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                return None
            if not chunk: return None
            buf.extend(chunk)
        return None

    # 1. HELLO -> HELLO_ACK
    sock.sendall(p.encode(p.Type.HELLO, bytes(16) + b"0.1.0"))
    f = recv_frame()
    check("收到 HELLO_ACK", f is not None and f[0] == p.Type.HELLO_ACK)
    caps = int.from_bytes(f[1][1:5], "little") if f else 0
    check("声明了 COMMIT_TEXT 能力", bool(caps & p.CAP_COMMIT_TEXT))
    check("声明了 KEY 透传能力", bool(caps & p.CAP_KEY_PASSTHRU))

    # 2. COMMIT_TEXT 中文 -> OK
    sock.sendall(p.encode(p.Type.COMMIT_TEXT, "你好世界".encode()))
    f = recv_frame()
    check("中文提交返回 COMMIT_ACK", f is not None and f[0] == p.Type.COMMIT_ACK)
    check("结果码 OK", f is not None and f[1][0] == p.Result.OK)

    # 3. 日文
    sock.sendall(p.encode(p.Type.COMMIT_TEXT, "こんにちは".encode()))
    f = recv_frame()
    check("日文提交 OK", f is not None and f[1][0] == p.Result.OK)

    # 4. KEY 不需要回包
    sock.sendall(p.encode(p.Type.KEY, bytes([0x00, 0x2A])))

    # 5. PING -> PONG
    sock.sendall(p.encode(p.Type.PING))
    f = recv_frame()
    check("PING 得到 PONG", f is not None and f[0] == p.Type.PONG)

    # 6. 粘包：两帧一次发出，接收器要都处理
    sock.sendall(p.encode(p.Type.COMMIT_TEXT, "甲".encode())
                 + p.encode(p.Type.COMMIT_TEXT, "乙".encode()))
    f1 = recv_frame(); f2 = recv_frame()
    check("粘包两帧都回了 ACK",
          f1 is not None and f2 is not None
          and f1[0] == p.Type.COMMIT_ACK and f2[0] == p.Type.COMMIT_ACK)

    # 7. 分片：一帧拆成两次发
    frame = p.encode(p.Type.COMMIT_TEXT, "丙".encode())
    sock.sendall(frame[:4]); time.sleep(0.15); sock.sendall(frame[4:])
    f = recv_frame()
    check("分片帧能重组并回 ACK", f is not None and f[0] == p.Type.COMMIT_ACK)

    # 8. 垃圾字节后接正常帧，接收器应能重同步
    sock.sendall(b"\xDE\xAD\xBE\xEF" + p.encode(p.Type.COMMIT_TEXT, "丁".encode()))
    f = recv_frame()
    check("垃圾字节后能重同步", f is not None and f[0] == p.Type.COMMIT_ACK)

    sock.close()
    time.sleep(0.4)
finally:
    srv.terminate()
    try: srv.wait(timeout=3)
    except subprocess.TimeoutExpired: srv.kill()

print("\n接收器输出：")
for l in out_lines:
    print("   | " + l)

joined = "\n".join(out_lines)
check("日志里出现中文原文", "你好世界" in joined)
check("日志里出现日文原文", "こんにちは" in joined)
check("日志里出现 KEY 帧", "keycode=0x2A" in joined)

print()
if fails:
    print(f"{len(fails)} 项失败: {fails}"); raise SystemExit(1)
print("端到端全部通过")
