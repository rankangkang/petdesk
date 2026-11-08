#include "activation.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>

namespace petdesk {
ActivationMailbox::ActivationMailbox(QString root, std::function<void()> callback, QObject *parent)
    : QObject(parent), m_root(std::move(root)), m_callback(std::move(callback)) {
    m_watcher.addPath(m_root);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { consume(); });
}
bool ActivationMailbox::request(const QString &root) {
    QSaveFile file(QDir(root).filePath("open-console.request"));
    const QByteArray message = "open-console/" + QUuid::createUuid().toByteArray();
    return file.open(QIODevice::WriteOnly) && file.write(message) == message.size() && file.commit();
}
void ActivationMailbox::consume() {
    QFile file(QDir(m_root).filePath("open-console.request"));
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray message = file.read(128);
    file.close();
    if (file.remove() && message.startsWith("open-console/") && m_callback) m_callback();
}
#ifndef Q_OS_MACOS
void setReopenHandler(std::function<void()>) {}
#endif
}
