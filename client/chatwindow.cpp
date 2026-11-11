#include "chatwindow.h"
#include "ui_chatwindow.h"
#include "tcpmanger.h"
#include <QTextBlockFormat>
#include <QTextCursor>

ChatWindow::ChatWindow(const QString &username, Tcpmanger *tcp,
                       const QByteArray &initialData, QWidget *parent)
    : QWidget(parent), ui_(new Ui::ChatWindow), username_(username), tcp_(tcp)
{
    // 加载 Designer 布局，并连接发送按钮和回车事件。
    ui_->setupUi(this);
    connect(ui_->sendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(ui_->messageInput, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);
    connect(tcp_, &Tcpmanger::dataReceived, this, &ChatWindow::receiveData);
    connect(tcp_, &Tcpmanger::disconnected, this, [this]() {
        disconnected(QStringLiteral("连接已断开"));
    });
    connect(tcp_, &Tcpmanger::errorOccurred, this, [this](const QString &error) {
        disconnected(QStringLiteral("连接错误：") + error);
    });
    receiveData(initialData);
}

ChatWindow::~ChatWindow()
{
    delete ui_;
}

void ChatWindow::appendMessage(const QString &message, bool ownMessage)
{
    // 在聊天记录末尾添加纯文本，自己的消息靠右显示。
    QTextCursor cursor(ui_->messages->document());
    cursor.movePosition(QTextCursor::End);
    if (!ui_->messages->document()->isEmpty()) cursor.insertBlock();
    QTextBlockFormat format;
    format.setAlignment(ownMessage ? Qt::AlignRight : Qt::AlignLeft);
    format.setTopMargin(12);
    format.setBottomMargin(12);
    cursor.setBlockFormat(format);
    cursor.insertText(message);
    ui_->messages->setTextCursor(cursor);
    ui_->messages->ensureCursorVisible();
}

void ChatWindow::sendMessage()
{
    const QString message = ui_->messageInput->text();
    if (message.trimmed().isEmpty()) return;
    const QByteArray body = message.toUtf8();
    if (body.size() > 1024) {
        appendMessage(QStringLiteral("消息不能超过 1024 字节"), false);
        return;
    }
    for (unsigned char c : body) {
        if (c < 32 || c == 127) {
            appendMessage(QStringLiteral("消息不能包含控制字符"), false);
            return;
        }
    }
    const QByteArray command = "MSG " + body + "\n";
    if (tcp_->send(command) != command.size()) {
        disconnected(QStringLiteral("发送失败，连接已关闭"));
        tcp_->stop();
        return;
    }
    appendMessage(QStringLiteral("我（") + username_ + QStringLiteral("）：") + message, true);
    ui_->messageInput->clear();
    ui_->messageInput->setFocus();
}

void ChatWindow::receiveData(const QByteArray &data)
{
    pending_ += data;
    int end;
    while ((end = pending_.indexOf('\n')) >= 0) {
        if (end > 1062) {
            disconnected(QStringLiteral("服务器响应过长"));
            tcp_->stop();
            return;
        }
        const QByteArray line = pending_.left(end);
        pending_.remove(0, end + 1);
        if (line.startsWith("CHAT ")) {
            const int separator = line.indexOf(' ', 5);
            if (separator > 5) {
                appendMessage(QString::fromUtf8(line.mid(5, separator - 5)) +
                              QStringLiteral("：") + QString::fromUtf8(line.mid(separator + 1)), false);
                continue;
            }
        }
        if (line.startsWith("ERROR ")) {
            appendMessage(QStringLiteral("服务器错误：") + QString::fromUtf8(line.mid(6)), false);
        }
    }
    if (pending_.size() > 1062) {
        disconnected(QStringLiteral("服务器响应过长"));
        tcp_->stop();
    }
}

void ChatWindow::disconnected(const QString &reason)
{
    if (!connected_) return;
    connected_ = false;
    pending_.clear();
    ui_->sendButton->setEnabled(false);
    ui_->messageInput->setEnabled(false);
    appendMessage(reason, false);
}
