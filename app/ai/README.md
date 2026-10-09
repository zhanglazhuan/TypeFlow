# app/ai

v2 内容：自动补全、下一个词预测。

实现后在根 `CMakeLists.txt` 里取消对应的注释行，把本目录加进 `EXTRA_COMPONENT_DIRS`。

设计约束：和 `app/ime` 一样尽量保持平台无关，这样预测模型能在 PC 上评测。
若走云端推理则需要 WiFi —— 注意 WiFi 与 BLE 共用射频，和 `hidout_ble` 同时工作
需要开共存(coexistence)，届时要实测吞吐和连接稳定性。
