#pragma once

// 项目配置集中放在这里，修改后需要重新编译。
#define LISTEN_PORT 8888
#define MAX_PENDING_BYTES (1024 * 1024) // 每个客户端的待发送数据上限，单位：字节。
#define MAX_HTTP_HEADER_BYTES 8192 // HTTP 请求头上限，单位：字节。
#define HTTP_FILES_DIR "/home/xuwu/Desktop/httptest" // HTTP 文件服务目录。
#define MAX_UPLOAD_BYTES (512ULL * 1024 * 1024) // 文件上传上限，单位：字节。
#define CLIENT_IDLE_TIMEOUT_SECONDS 60 // 连接长时间没有收发进展时关闭，单位：秒。
#define MAX_CHAT_NICKNAME_BYTES 32 // 聊天昵称上限，单位：字节。
#define MAX_CHAT_MESSAGE_BYTES 1024 // 单条聊天内容上限，单位：字节。
