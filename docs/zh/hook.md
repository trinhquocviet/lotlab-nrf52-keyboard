[English](../hook.md) | [Tiếng Việt](../vi/hook.md) | **中文**

# 键盘钩子说明

## `hook_bootmagic()`

用于处理 bootmagic 的 hook。

这个 hook 是 TMK 自带的，在这个工程中实现了绑定擦除和按键开机的功能。如果要实现自定义的开机流程，或者添加/删除 bootmagic 命令，可以覆盖这个 hook。

## `action_function()`

TMK 内建的自定义动作处理的 hook。

这个 hook 默认实现了切换 USB 和关机功能，以及自定义动作模块的处理。不建议直接覆盖此 hook。若仅是要添加新的 Fn 功能，请参见 [driver.md](driver.md) 内的相关信息添加新的自定义动作模块。
