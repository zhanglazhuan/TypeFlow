# system/hidout/CMakeLists.txt
#
# 注意 typeflow_proto.c 是从顶层 protocol/ 直接引进来编的，不是复制一份。
# 固件和 reMarkable 接收器共用同一份协议源码 —— 协议双边不一致是最难查的
# 一类 bug，共用源码从根上避免。改协议先改 protocol/PROTOCOL.md。
idf_component_register(
    SRCS
        "hidout.c"
        "hidout_usb.c"
        "hidout_ble.c"
        "hidout_usbhost.c"
        "hidout_typefolio.c"
        "../../protocol/typeflow_proto.c"
    INCLUDE_DIRS "." "../../protocol"
    REQUIRES target esp_common
)
