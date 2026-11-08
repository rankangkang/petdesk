#pragma once
#include "petformat.h"
#include <QHash>
#include <QScrollArea>
class QButtonGroup;
class QVBoxLayout;
class QRadioButton;
namespace petdesk {
// Native accessible controls, without synthesized table-row/column interfaces.
class PetList final : public QScrollArea {
    Q_OBJECT
public:
    PetList();
    void setEntries(const QVector<PetEntry> &entries, const QString &currentKey = {}, bool visible = false);
    void selectKey(const QString &key);
    QString currentKey() const { return m_selected; }
    int count() const { return m_buttons.size(); }
signals:
    void selectionChanged();
private:
    QButtonGroup *m_group;
    QVBoxLayout *m_layout;
    QHash<QString, QRadioButton *> m_buttons;
    QString m_selected;
};
}
