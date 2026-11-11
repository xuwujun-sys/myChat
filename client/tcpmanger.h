#ifndef TCPMANGER_H
#define TCPMANGER_H

#include <QObject>
#include <QTcpSocket>

// 负责客户端 TCP 连接和字节收发，不处理聊天协议。
class Tcpmanger : public QObject
{
    Q_OBJECT

public:
    explicit Tcpmanger(QObject *parent = nullptr);
    ~Tcpmanger() override;

    void start(const QString &ip, quint16 port); // 断开旧连接后连接服务器。
    void stop(); // 断开连接。
    qint64 send(const QByteArray &data); // 返回入队字节数，未连接或失败时返回 -1。

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    // 收到原始字节；完整消息需由协议层处理。
    void dataReceived(QByteArray data);

private:
    QTcpSocket *m_socket;
};

#endif // TCPMANGER_H
