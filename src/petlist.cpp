#include "petlist.h"
#include <QButtonGroup>
#include <QRadioButton>
#include <QSet>
#include <QVBoxLayout>
namespace petdesk {
namespace {
class PetChoice final : public QRadioButton {
public:
    using QRadioButton::QRadioButton;
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
};
}
PetList::PetList() : m_group(new QButtonGroup(this)) {
    setWidgetResizable(true); setFrameShape(QFrame::StyledPanel); setMinimumHeight(156);
    auto *content = new QWidget;
    m_layout = new QVBoxLayout(content); m_layout->setContentsMargins(8, 6, 8, 6); m_layout->setSpacing(2);
    m_layout->addStretch(); setWidget(content);
    m_group->setExclusive(true);
}
void PetList::setEntries(const QVector<PetEntry> &entries, const QString &currentKey, bool visible) {
    QSet<QString> keys;
    for (const auto &entry : entries) keys.insert(entry.key);
    for (const auto &key : m_buttons.keys()) if (!keys.contains(key)) {
        auto *choice = m_buttons.take(key); m_group->removeButton(choice);
        m_layout->removeWidget(choice); choice->hide(); choice->deleteLater();
        if (m_selected == key) m_selected.clear();
    }
    int row = 0;
    for (const auto &entry : entries) {
        auto *choice = m_buttons.value(entry.key);
        if (!choice) {
            choice = new PetChoice;
            choice->setMinimumHeight(72); choice->setIconSize(QSize(60, 65));
            choice->setObjectName("pet-" + entry.key); choice->setProperty("petKey", entry.key);
            m_group->addButton(choice); m_buttons.insert(entry.key, choice);
            connect(choice, &QRadioButton::toggled, this, [this, key = entry.key](bool checked) {
                if (checked) { m_selected = key; emit selectionChanged(); }
            });
        }
        const QString name = entry.name + (entry.key == currentKey ?
            (visible ? QStringLiteral("  · 正在显示") : QStringLiteral("  · 已隐藏")) : QString{});
        choice->setText(QString(name).replace('&', "&&"));
        choice->setAccessibleName(name); choice->setIcon(QIcon(entry.thumbnail));
        m_layout->insertWidget(row++, choice);
    }
    if (m_buttons.contains(m_selected)) selectKey(m_selected);
    else if (m_buttons.contains(currentKey)) selectKey(currentKey);
    else if (!entries.isEmpty()) selectKey(entries.first().key);
    emit selectionChanged();
}
void PetList::selectKey(const QString &key) {
    if (auto *choice = m_buttons.value(key)) {
        choice->setChecked(true); m_selected = key;
    }
}
}
