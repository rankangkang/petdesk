#include "petlist.h"
#include <QButtonGroup>
#include <QRadioButton>
#include <QResizeEvent>
#include <QSet>
#include <QVBoxLayout>
#include <algorithm>
namespace petdesk {
namespace {
class PetChoice final : public QRadioButton {
public:
    using QRadioButton::QRadioButton;
    void setFullText(const QString &text) {
        m_fullText = text; setAccessibleName(text); setToolTip(text); updateText();
    }
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
protected:
    void resizeEvent(QResizeEvent *event) override { QRadioButton::resizeEvent(event); updateText(); }
private:
    void updateText() {
        QString shown = fontMetrics().elidedText(m_fullText, Qt::ElideRight, std::max(0, width() - iconSize().width() - 40));
        shown.replace('&', "&&");
        if (QRadioButton::text() != shown) setText(shown);
    }
    QString m_fullText;
};
}
PetList::PetList() : m_group(new QButtonGroup(this)) {
    setWidgetResizable(true); setFrameShape(QFrame::NoFrame); setMinimumHeight(180);
    auto *content = new QWidget;
    content->setObjectName("petListContent");
    m_layout = new QVBoxLayout(content); m_layout->setContentsMargins(0, 0, 0, 0); m_layout->setSpacing(5);
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
            choice->setMinimumHeight(68); choice->setIconSize(QSize(48, 52));
            choice->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            choice->setCursor(Qt::PointingHandCursor); choice->setProperty("petChoice", true);
            choice->setObjectName("pet-" + entry.key); choice->setProperty("petKey", entry.key);
            m_group->addButton(choice); m_buttons.insert(entry.key, choice);
            connect(choice, &QRadioButton::toggled, this, [this, key = entry.key](bool checked) {
                if (checked) { m_selected = key; emit selectionChanged(); }
            });
        }
        const QString name = entry.name + (entry.key == currentKey ?
            (visible ? QStringLiteral("  · 正在显示") : QStringLiteral("  · 已隐藏")) : QString{});
        static_cast<PetChoice *>(choice)->setFullText(name); choice->setIcon(QIcon(entry.thumbnail));
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
