# 设备终端

控制台使用 [anytermqt v0.1.2](https://github.com/scottpeterman/anytermqt/tree/9943b0ee15ced240e1e146c7f107b9ab202193f1) 的原生 Qt 6 终端组件，直接显示串口、WebSocket 和刷机工具的输出。组件不启动本机 shell。

连接设备后，在终端的 `> ` 提示符后直接输入命令，按 Enter 发送。左右方向键、Home、End、Backspace、Delete 可以编辑命令；上下方向键可以调取历史命令，向下返回未提交的草稿。Ctrl+A/E 移动到行首或行尾，Ctrl+U/K 删除光标前后内容，Ctrl+C 取消当前输入。

Ctrl+Shift+C/V 或右键菜单用于复制、粘贴。粘贴中的换行转为空格，只有按 Enter 才发送命令。断开连接时清空输入和命令历史；含密码的管理命令不加入历史。

终端位于底部时跟随新输出，向上滚动时保留阅读位置，回到底部后恢复跟随。未提交的命令不会因新输出丢失。输出最多保留组件默认的 5000 行滚动历史。

首次配置 CMake 时需要联网下载终端组件和 utf8proc；版本与 SHA-256 校验值固定在 `cmake/TerminalDependencies.cmake`，后续构建复用构建目录内的缓存。两个依赖均静态链接到程序，不增加部署 DLL。离线构建可分别使用 CMake 标准变量 `FETCHCONTENT_SOURCE_DIR_ANYTERMQT` 和 `FETCHCONTENT_SOURCE_DIR_UTF8PROC` 指向对应版本的源码目录。

`cmake/PreserveTerminalHistory.cmake` 在编译副本中修复上游 v0.1.2 收窄窗口时裁掉历史内容的问题，原始下载源码保持不变。历史行在窗口变窄时暂时裁切显示，恢复宽度后可以继续查看完整内容。

anytermqt 使用 GPL-3.0 许可证，utf8proc 使用 MIT/Unicode 许可证，均与本项目的 GPL-3.0 许可证兼容。分发包在 `licenses/` 中提供这些依赖的许可证。
