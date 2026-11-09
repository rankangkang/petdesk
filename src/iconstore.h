#pragma once
#include <QImage>
#include <QString>

namespace petdesk {
// A local, normalized copy keeps the selected icon independent of its source file.
class IconStore {
public:
    explicit IconStore(const QString &dataRoot);
    QImage importFile(const QString &path) const;
    QImage load() const;
private:
    QString m_path;
};
}
