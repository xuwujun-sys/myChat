# 文件服务器

默认共享目录是 `/home/xuwu/Desktop/httptest`，需事先创建并赋予服务器进程读写权限。可在 `config/config.h` 修改目录、端口、上传上限和空闲超时，修改后重新编译。

在项目目录下使用 Linux 编译器：

```sh
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp core/*.cpp handlers/*.cpp -o mychat
./mychat
```

浏览器访问 `http://服务器IP:8888/`；页面支持查看、上传和下载文件。默认单个上传上限为 512 MiB，同名文件不会被覆盖。暂不支持子目录和符号链接；文件列表只显示 UTF-8 文件名。

接口：`GET /api/files` 获取文件列表，`GET /files/文件名` 下载，`PUT /files/文件名` 上传原始请求体，文件名需进行 URL 编码。

服务器目前没有登录认证和 HTTPS，应只运行在可信网络中。磁盘读写仍在事件循环中进行，慢速磁盘可能影响其他连接。
