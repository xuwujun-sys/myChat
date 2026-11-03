#pragma once

// 项目配置集中放在这里，修改后需要重新编译。
#define LISTEN_PORT 8888
#define MAX_PENDING_BYTES (1024 * 1024) // 每个客户端的待发送数据上限，单位：字节。
#define MAX_HTTP_HEADER_BYTES 8192 // HTTP 请求头上限，单位：字节。
#define HTTP_FILES_DIR "/home/xuwu/Desktop/httptest" // HTTP 文件服务目录。
#define MAX_UPLOAD_BYTES (512ULL * 1024 * 1024) // 上传上限，单位：字节。
#define CONNECTION_IDLE_SECONDS 30 // 连接无收发进展的超时时间。
