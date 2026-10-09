/*
 * hidout — 通道选择、协议组帧、文本提交策略调度
 */

#include "hidout.h"
#include "hidout_backend.h"
#include "typeflow_proto.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "hidout";

static hidout_config_t          s_cfg;
static const target_profile_t  *s_prof = NULL;
static const hidout_backend_t  *s_be   = NULL;
static hidout_link_cb_t         s_link_cb = NULL;
static void                    *s_link_ctx = NULL;

/* 接收器握手后回填的能力位图。0 表示还没握手成功。 */
static uint32_t s_rx_caps = 0;

/* 接收器回包的累积缓冲。TCP/CDC 不保证帧边界，必须自己拼。 */
static uint8_t  s_rxbuf[TF_MAX_FRAME * 2];
static size_t   s_rxlen = 0;

/* ── ASCII -> HID keycode ────────────────────────────────────────────────
 * HID Usage Page 0x07。只覆盖可打印 ASCII(0x20~0x7E)。
 * -------------------------------------------------------------------- */
#define HID_MOD_LSHIFT  0x02

static uint8_t ascii_to_keycode(char c, bool *need_shift)
{
    *need_shift = false;
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
    if (c >= 'A' && c <= 'Z') { *need_shift = true; return (uint8_t)(0x04 + (c - 'A')); }
    if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
    if (c == '0') return 0x27;

    switch (c) {
        case ' ':  return 0x2C;
        case '-':  return 0x2D;
        case '=':  return 0x2E;
        case '[':  return 0x2F;
        case ']':  return 0x30;
        case '\\': return 0x31;
        case ';':  return 0x33;
        case '\'': return 0x34;
        case '`':  return 0x35;
        case ',':  return 0x36;
        case '.':  return 0x37;
        case '/':  return 0x38;
        case '\n': return 0x28;   /* Enter */
        case '\t': return 0x2B;   /* Tab */
        default: break;
    }
    *need_shift = true;
    switch (c) {
        case '!': return 0x1E;  case '@': return 0x1F;  case '#': return 0x20;
        case '$': return 0x21;  case '%': return 0x22;  case '^': return 0x23;
        case '&': return 0x24;  case '*': return 0x25;  case '(': return 0x26;
        case ')': return 0x27;  case '_': return 0x2D;  case '+': return 0x2E;
        case '{': return 0x2F;  case '}': return 0x30;  case '|': return 0x31;
        case ':': return 0x33;  case '"': return 0x34;  case '~': return 0x35;
        case '<': return 0x36;  case '>': return 0x37;  case '?': return 0x38;
        default: break;
    }
    *need_shift = false;
    return 0;
}

/* ── UTF-8 解码 ───────────────────────────────────────────────────────── */

static int utf8_next(const char *s, size_t len, uint32_t *cp)
{
    if (len == 0) return 0;
    const uint8_t *p = (const uint8_t *)s;

    if (p[0] < 0x80)                       { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && len >= 2) {
        *cp = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && len >= 3) {
        *cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6)
            | (p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && len >= 4) {
        *cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12)
            | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
    return -1;
}

/* ── 协议收发 ─────────────────────────────────────────────────────────── */

static esp_err_t proto_send(uint8_t type, const void *payload, uint16_t len)
{
    if (!s_be || !s_be->send_bytes) return ESP_ERR_NOT_SUPPORTED;

    uint8_t frame[TF_MAX_FRAME];
    size_t n = tf_encode(type, payload, len, frame, sizeof(frame));
    if (n == 0) return ESP_ERR_INVALID_SIZE;

    return s_be->send_bytes(frame, n);
}

static void handle_frame(const tf_frame_t *f)
{
    switch (f->type) {
        case TF_TYPE_HELLO_ACK:
            /* 载荷：品牌(1) + 能力位图(4) + 版本字符串 */
            if (f->payload_len >= 5) {
                s_rx_caps = (uint32_t)f->payload[1]
                          | ((uint32_t)f->payload[2] << 8)
                          | ((uint32_t)f->payload[3] << 16)
                          | ((uint32_t)f->payload[4] << 24);
                ESP_LOGI(TAG, "接收器已握手：品牌=%u 能力=0x%08" PRIx32 "%s",
                         f->payload[0], s_rx_caps,
                         (s_rx_caps & TF_CAP_COMMIT_TEXT) ? "" : " （不支持文本提交！）");
            }
            break;

        case TF_TYPE_COMMIT_ACK:
            if (f->payload_len >= 1 && f->payload[0] != TF_RESULT_OK) {
                /* NO_FOCUS 是最常见的：用户没点进输入框就选词，字会丢。
                 * 候选条应该据此提示，而不是静默丢弃。 */
                ESP_LOGW(TAG, "接收器提交失败，结果码 %u", f->payload[0]);
            }
            break;

        case TF_TYPE_PING:
            proto_send(TF_TYPE_PONG, NULL, 0);
            break;

        case TF_TYPE_PONG:
            break;

        default:
            ESP_LOGD(TAG, "忽略未知帧类型 0x%02X", f->type);
            break;
    }
}

/* 后端收到字节就丢进来。可能一次来半帧，也可能一次来好几帧。 */
static void on_rx(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;

    if (s_rxlen + len > sizeof(s_rxbuf)) {
        /* 缓冲溢出说明对端在乱发或我们漏了同步，整个丢掉重来比截断安全 */
        ESP_LOGW(TAG, "接收缓冲溢出，丢弃 %u 字节重新同步", (unsigned)s_rxlen);
        s_rxlen = 0;
        if (len > sizeof(s_rxbuf)) return;
    }
    memcpy(s_rxbuf + s_rxlen, data, len);
    s_rxlen += len;

    /* 反复解析直到解不出更多。tf_decode 即使失败也会告诉我们消耗了多少
     * （丢弃的垃圾字节），所以这个循环不会死转。 */
    for (;;) {
        tf_frame_t f;
        size_t used = 0;
        bool got = tf_decode(s_rxbuf, s_rxlen, &f, &used);

        if (got) handle_frame(&f);

        if (used == 0) break;                   /* 数据不够，等下一批 */
        memmove(s_rxbuf, s_rxbuf + used, s_rxlen - used);
        s_rxlen -= used;
        if (s_rxlen == 0) break;
    }
}

/* ── 后端选择 ─────────────────────────────────────────────────────────── */

static const hidout_backend_t *backend_for(target_link_t link)
{
    switch (link) {
        case TARGET_LINK_USB_HID:   return hidout_backend_usb();
        case TARGET_LINK_BLE:       return hidout_backend_ble();
        case TARGET_LINK_USB_HOST:  return hidout_backend_usbhost();
        case TARGET_LINK_TYPEFOLIO: return hidout_backend_typefolio();
        default:                    return NULL;
    }
}

/* ── 生命周期 ─────────────────────────────────────────────────────────── */

esp_err_t hidout_init(const hidout_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    s_prof = target_get(cfg->brand);
    if (!s_prof) return ESP_ERR_INVALID_ARG;

    target_link_t link = cfg->link_override != TARGET_LINK_NONE
                       ? cfg->link_override
                       : target_preferred_link(s_prof);

    /* 拦住不可能的组合。最典型的是「reMarkable 1/2 + BLE」——
     * 那两台机器压根没有蓝牙硬件，让它静默失败会浪费很多调试时间。 */
    if (!target_supports_link(s_prof, link)) {
        ESP_LOGE(TAG, "%s 不支持通道 %d。可用首选是 %d，依据见 receiver/README.md",
                 s_prof->name, (int)link, (int)target_preferred_link(s_prof));
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_cfg    = *cfg;
    s_be     = backend_for(link);
    s_rx_caps = 0;
    s_rxlen   = 0;
    if (!s_be) return ESP_ERR_NOT_SUPPORTED;

    ESP_LOGI(TAG, "目标=%s 通道=%s 提交=%s%s",
             s_prof->name, s_be->name,
             s_prof->commit == TARGET_COMMIT_RECEIVER    ? "接收器" :
             s_prof->commit == TARGET_COMMIT_HID_UNICODE ? "HID-Unicode" : "无",
             s_prof->needs_receiver ? "（需设备端接收器）" : "");

    esp_err_t ret = s_be->start(&s_cfg, on_rx, NULL);
    if (ret != ESP_OK) return ret;

    /* 接收器类通道连上后先握手，对端回 HELLO_ACK 才算真的能提交文本 */
    if (s_prof->commit == TARGET_COMMIT_RECEIVER && s_be->send_bytes && s_cfg.token) {
        uint8_t hello[TF_TOKEN_LEN + 16];
        memcpy(hello, s_cfg.token, TF_TOKEN_LEN);
        size_t vlen = strlen("0.1.0");
        memcpy(hello + TF_TOKEN_LEN, "0.1.0", vlen);
        proto_send(TF_TYPE_HELLO, hello, (uint16_t)(TF_TOKEN_LEN + vlen));
    }
    return ESP_OK;
}

esp_err_t hidout_deinit(void)
{
    if (!s_be) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = s_be->stop();
    s_be = NULL;
    s_rx_caps = 0;
    return ret;
}

void hidout_set_link_cb(hidout_link_cb_t cb, void *ctx)
{
    s_link_cb  = cb;
    s_link_ctx = ctx;
}

bool hidout_is_connected(void)
{
    return s_be && s_be->connected && s_be->connected();
}

bool hidout_can_commit_text(void)
{
    if (!s_prof || !hidout_is_connected()) return false;

    if (s_prof->commit == TARGET_COMMIT_RECEIVER) {
        return (s_rx_caps & TF_CAP_COMMIT_TEXT) != 0;
    }
    /* HID Unicode 注入序列还没实现，所以目前非 ASCII 一律发不出去 */
    return false;
}

/* ── 发送 ─────────────────────────────────────────────────────────────── */

static esp_err_t tap(uint8_t modifiers, uint8_t keycode)
{
    /* 接收器类通道没有 HID 报告，改用 KEY 帧透传 */
    if (!s_be->send_report) {
        uint8_t p[2] = { modifiers, keycode };
        return proto_send(TF_TYPE_KEY, p, sizeof(p));
    }

    uint8_t keys[6] = { keycode, 0, 0, 0, 0, 0 };
    esp_err_t ret = s_be->send_report(modifiers, keys);
    if (ret != ESP_OK) return ret;

    memset(keys, 0, sizeof(keys));
    return s_be->send_report(0, keys);   /* 松开 */
}

/* TODO 各 OS 的 Unicode 注入序列。仅 TARGET_BRAND_GENERIC_PC 会走到这里，
 * 两个一期目标品牌都走接收器，所以这条路优先级低。
 *   Windows: 按住 Alt + 小键盘十进制码；BMP 外要拆代理对
 *   macOS  : 需用户把输入源切到 Unicode Hex Input，再 Option + 4 位 hex
 *   Linux  : Ctrl+Shift+U + hex + 回车，依赖 ibus/fcitx */
static esp_err_t send_unicode_cp(uint32_t cp)
{
    ESP_LOGW(TAG, "U+%04" PRIX32 " 的 HID 注入序列尚未实现", cp);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t hidout_send_key(uint8_t modifiers, uint8_t keycode)
{
    if (!s_be) return ESP_ERR_INVALID_STATE;
    return tap(modifiers, keycode);
}

esp_err_t hidout_send_text(const char *utf8, size_t len)
{
    if (!utf8) return ESP_ERR_INVALID_ARG;
    if (!s_be || !s_prof) return ESP_ERR_INVALID_STATE;
    if (len == 0) return ESP_OK;

    /* 走接收器时整段一次推走，不逐字符转键码 —— 这也是它比 HID 注入快得多的
     * 原因：一个汉字省掉 4~6 次按键往返。 */
    if (s_prof->commit == TARGET_COMMIT_RECEIVER) {
        if (!(s_rx_caps & TF_CAP_COMMIT_TEXT)) {
            ESP_LOGW(TAG, "接收器未就绪，%s", s_prof->hint);
            return ESP_ERR_NOT_SUPPORTED;
        }
        if (len > TF_MAX_PAYLOAD) return ESP_ERR_INVALID_SIZE;
        return proto_send(TF_TYPE_COMMIT_TEXT, utf8, (uint16_t)len);
    }

    /* HID 路线：ASCII 逐字符转键码，非 ASCII 走注入序列 */
    size_t i = 0;
    while (i < len) {
        uint32_t cp = 0;
        int n = utf8_next(utf8 + i, len - i, &cp);
        if (n <= 0) return ESP_ERR_INVALID_ARG;
        i += (size_t)n;

        if (cp < 0x80) {
            bool shift = false;
            uint8_t kc = ascii_to_keycode((char)cp, &shift);
            if (kc == 0) continue;           /* 发不出的控制字符跳过 */
            esp_err_t ret = tap(shift ? HID_MOD_LSHIFT : 0, kc);
            if (ret != ESP_OK) return ret;
        } else {
            esp_err_t ret = send_unicode_cp(cp);
            if (ret != ESP_OK) return ret;
        }
    }
    return ESP_OK;
}

esp_err_t hidout_switch_link(target_link_t link)
{
    if (!s_prof) return ESP_ERR_INVALID_STATE;

    if (!target_supports_link(s_prof, link)) {
        ESP_LOGE(TAG, "%s 不支持通道 %d", s_prof->name, (int)link);
        return ESP_ERR_NOT_SUPPORTED;
    }
    const hidout_backend_t *be = backend_for(link);
    if (!be) return ESP_ERR_NOT_SUPPORTED;

    if (s_be) s_be->stop();
    s_be      = be;
    s_rx_caps = 0;
    s_rxlen   = 0;
    s_cfg.link_override = link;

    ESP_LOGI(TAG, "切换通道 -> %s", be->name);
    return s_be->start(&s_cfg, on_rx, NULL);
}
