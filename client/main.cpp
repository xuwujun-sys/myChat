#include "chatwindow.h"
#include "logindialog.h"
#include "tcpmanger.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Tcpmanger tcp;
    LoginDialog login(&tcp);
    // exec() 等待登录结果；用户关闭登录框时退出程序。
    if (login.exec() != QDialog::Accepted) return 0;
    QObject::disconnect(&tcp, nullptr, &login, nullptr);
    ChatWindow window(login.username(), &tcp, login.takeBufferedData());
    window.show();
    return app.exec();
}
