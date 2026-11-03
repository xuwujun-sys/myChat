#pragma once

// 项目配置集中放在这里，修改后需要重新编译。
#define LISTEN_PORT 8888
#define MAX_PENDING_BYTES (1024 * 1024) // 每个客户端的待发送数据上限，单位：字节。
#define MAX_HTTP_HEADER_BYTES 8192 // HTTP 请求头上限，单位：字节。
#define HTTP_FILES_DIR "./files" // 文件目录，相对于服务器启动时的工作目录。
