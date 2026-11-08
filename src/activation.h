#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <functional>

namespace petdesk {
class ActivationMailbox final : public QObject {
public:
    ActivationMailbox(QString root, std::function<void()> callback, QObject *parent = nullptr);
    static bool request(const QString &root);
    void consume();
private:
    QString m_root;
    std::function<void()> m_callback;
    QFileSystemWatcher m_watcher;
};
void setReopenHandler(std::function<void()> callback);
}
