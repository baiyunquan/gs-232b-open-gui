# 应用标志

- `logo.svg`：512 × 512 矢量源文件，背景四角透明。
- `logo.png`：512 × 512，用于 Qt 应用窗口图标及预览。
- `app.ico`：包含 16、20、24、32、40、48、64、128、256 像素图层，用于 Windows 可执行文件图标。
- `app.rc`：Windows 原生图标资源。

修改 SVG 后，使用 librsvg 和 Pillow 重新导出：

```sh
rsvg-convert -w 512 -h 512 -o assets/logo.png assets/logo.svg
python3 - <<'PY'
from PIL import Image
with Image.open('assets/logo.png') as logo:
    logo.save('assets/app.ico', sizes=[(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
PY
```
