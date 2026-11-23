[English](../porting.md) | [Tiếng Việt](../vi/porting.md) | **中文**

# 快速移植指南

0. 将 `keyboard/template` 文件夹复制一份，修改为你键盘的名称；
1. 打开 `Makefile`，根据你的主控芯片的型号修改对应的配置项目；
2. 准备好你的键盘的线路图，接下来修改要用到；
3. 打开 `config.h`，根据你的键盘的配置，修改按键阵列的行数和列数并修改底下的 IO 列表；修改按键灯的 IO；修改电量检测和 UART 的 IO；
4. 根据你的需要，修改 `config.h` 里面剩余配置项目的配置；
5. 打开 `keymap_common.h`，修改按键与按键阵列之间的对应关系。`tmk/keyboard` 下应该有许多已经做好的模板，只需复制即可；
6. 打开 `keymap_plain.c`，编辑默认的按键配列；
7. 在命令行中输入 `make`，尝试进行编译。

## 更多功能的设置

请参见 [hook.md](hook.md)，来查看键盘中的各种 hook，以及其作用。
