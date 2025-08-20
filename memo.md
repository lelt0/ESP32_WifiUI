# ESP-IDFでWebSocket有効化
1. `idf.py menuconfig`
1. `[*] Enable WebSocket support`
![alt text](doc/res/image.png)
![alt text](doc/res/image-1.png)

# Request Header 拡張
1. `idf.py menuconfig`
1. ![alt text](doc/res/image-2.png)
- これをしないと一部のAndroidスマホからのレスポンスが「Header fields are too long」としてESPサーバが怒ってくる

# パーティションサイズ拡張
1. `idf.py menuconfig`
1. ![alt text](doc/res/image-3.png)
   ↓
   ![alt text](doc/res/image-4.png)
   ![alt text](doc/res/image-5.png)
1. ![alt text](doc/res/image-6.png)
- plotly.min.js（gzip圧縮で1.4MB）をアプリに乗せるために必要

# TODO
- captive portal
- スマホ、PC
- グラフ
- 3次元グラフ
- AP.NET両対応