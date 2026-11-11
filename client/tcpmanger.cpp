#include "tcpmanger.h"
#include <QNetworkProxy>

Tcpmanger::Tcpmanger(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    // 直连服务器，不使用系统代理。
    m_socket->setProxy(QNetworkProxy::NoProxy);
    // 收到数据时读取当前可用字节，交给外部处理。
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        emit dataReceived(m_socket->readAll());
    });
    // 将连接状态和错误转发给界面或调用方。
    connect(m_socket, &QTcpSocket::connected, this, &Tcpmanger::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &Tcpmanger::disconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket->errorString());
    });
}

Tcpmanger::~Tcpmanger()
{
    stop();
}

void Tcpmanger::start(const QString &ip, quint16 port)
{
    // 连接是异步的，成功与失败通过信号通知。
    stop();
    m_socket->connectToHost(ip, port);
}

void Tcpmanger::stop()
{
    // 立即断开连接，未发出的待发送数据会被丢弃。
    m_socket->abort();
}

qint64 Tcpmanger::send(const QByteArray &data)
{
    // write 返回写入 Qt 发送缓冲区的字节数，不代表对方已收到。
    if (m_socket->state() != QAbstractSocket::ConnectedState) return -1;
    return m_socket->write(data);
}
