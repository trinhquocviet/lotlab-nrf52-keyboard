**English** | [Tiếng Việt](vi/hook.md) | [中文](zh/hook.md)

# Keyboard Hooks Reference

## `hook_bootmagic()`

Hook for handling TMK BootMagic at startup.

This hook is built into TMK. In this project, it implements bond clearing and key-press wake/power-on. Override this hook if you want to customize the boot sequence or add/remove BootMagic commands.

## `action_function()`

TMK's built-in hook for custom action handling.

By default, this hook implements USB/BLE switching, sleep/power-off, and dispatching to modular custom action handlers. Overriding this hook directly is not recommended. If you only need to add new `Fn` functions, see [driver.md](driver.md) to register a modular `FN_HANDLER`.
