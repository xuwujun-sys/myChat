#pragma once
#include <QByteArray>
#include <QWidget>

namespace Ui { class ChatWindow; }
class Tcpmanger;

class ChatWindow : public QWidget {
public:
    explicit ChatWindow(const QString &username, Tcpmanger *tcp,
                        const QByteArray &initialData = QByteArray(), QWidget *parent = nullptr);
    ~ChatWindow() override;

private:
    void appendMessage(const QString &message, bool ownMessage);
    void sendMessage();
    void receiveData(const QByteArray &data);
    void disconnected(const QString &reason);
    Ui::ChatWindow *ui_;
    QString username_;
    Tcpmanger *tcp_;
    QByteArray pending_;
    bool connected_ = true;
};
