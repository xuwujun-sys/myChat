#pragma once
#include <QByteArray>
#include <QDialog>

namespace Ui { class LoginDialog; }
class Tcpmanger;
class QTimer;

class LoginDialog : public QDialog {
public:
    explicit LoginDialog(Tcpmanger *tcp, QWidget *parent = nullptr);
    ~LoginDialog() override;

    QString username() const;
    QByteArray takeBufferedData();

private:
    void submit();
    void receiveData(const QByteArray &data);
    void fail(const QString &message);
    Ui::LoginDialog *ui_;
    Tcpmanger *tcp_;
    QTimer *timer_;
    QByteArray pending_;
    bool waiting_ = false;
};
