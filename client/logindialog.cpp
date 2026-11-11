#include "logindialog.h"
#include "ui_logindialog.h"
#include "tcpmanger.h"
#include <QTimer>

LoginDialog::LoginDialog(Tcpmanger *tcp, QWidget *parent)
    : QDialog(parent), ui_(new Ui::LoginDialog), tcp_(tcp), timer_(new QTimer(this))
{
    ui_->setupUi(this);
    // 点击按钮或按回车时检查用户名。
    connect(ui_->loginButton, &QPushButton::clicked, this, &LoginDialog::submit);
    connect(ui_->username, &QLineEdit::returnPressed, this, &LoginDialog::submit);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]() { fail(QStringLiteral("连接或登录超时")); });

    connect(tcp_, &Tcpmanger::connected, this, [this]() {
        if (!waiting_) return;
        const QByteArray command = "LOGIN " + username().toUtf8() + "\n";
        if (tcp_->send(command) != command.size()) fail(QStringLiteral("发送登录请求失败"));
    });

    connect(tcp_, &Tcpmanger::dataReceived, this, &LoginDialog::receiveData);

    connect(tcp_, &Tcpmanger::errorOccurred, this, [this](const QString &message) {
        if (waiting_) fail(QStringLiteral("连接失败：") + message);
    });

    connect(tcp_, &Tcpmanger::disconnected, this, [this]() {
        if (waiting_) fail(QStringLiteral("连接已断开"));
    });
}

LoginDialog::~LoginDialog()
{
    delete ui_;
}

QString LoginDialog::username() const
{
    return ui_->username->text().trimmed();
}

void LoginDialog::submit()
{
    if (waiting_) return;
    const QByteArray name = username().toUtf8();
    if (name.isEmpty() || name.size() > 32) {
        ui_->errorLabel->setText(QStringLiteral("用户名需为 1 到 32 字节"));
        return;
    }
    for (unsigned char c : name) {
        if (c <= 32 || c == 127) {
            ui_->errorLabel->setText(QStringLiteral("用户名不能包含空格或控制字符"));
            return;
        }
    }
    const QString address = ui_->serverAddress->text().trimmed();
    if (address.isEmpty()) {
        ui_->errorLabel->setText(QStringLiteral("请输入服务器地址"));
        return;
    }
    pending_.clear();
    ui_->errorLabel->clear();
    ui_->loginButton->setEnabled(false);
    ui_->username->setEnabled(false);
    ui_->serverAddress->setEnabled(false);
    ui_->serverPort->setEnabled(false);
    // 先关闭旧连接，再开始这次登录。
    tcp_->stop();
    waiting_ = true;
    timer_->start(10000);
    tcp_->start(address, static_cast<quint16>(ui_->serverPort->value()));
}

void LoginDialog::receiveData(const QByteArray &data)
{
    if (!waiting_) return;
    pending_ += data;
    int end;
    while ((end = pending_.indexOf('\n')) >= 0) {
        if (end > 1062) { fail(QStringLiteral("服务器响应过长")); return; }
        const QByteArray line = pending_.left(end);
        pending_.remove(0, end + 1);
        if (line == "OK LOGIN") {
            waiting_ = false;
            timer_->stop();
            accept();
            return;
        }
        if (line == "ERROR NICKNAME_TAKEN") {
            fail(QStringLiteral("用户名已被使用"));
            return;
        }
        fail(QStringLiteral("登录失败：") + QString::fromUtf8(line));
        return;
    }
    if (pending_.size() > 1062) fail(QStringLiteral("服务器响应过长"));
}

QByteArray LoginDialog::takeBufferedData()
{
    QByteArray data;
    data.swap(pending_);
    return data;
}

void LoginDialog::fail(const QString &message)
{
    waiting_ = false;
    timer_->stop();
    tcp_->stop();
    pending_.clear();
    ui_->loginButton->setEnabled(true);
    ui_->username->setEnabled(true);
    ui_->serverAddress->setEnabled(true);
    ui_->serverPort->setEnabled(true);
    ui_->errorLabel->setText(message);
}
