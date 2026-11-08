#pragma once
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QIcon>
#include <QLabel>
#include <QString>
#include <QWidget>

namespace petdesk::ui {
enum class Icon { Library, Sliders, Settings, Info, Plus, Folder, Eye, Hide, Exit, Arrow, Restore };
void applyApplicationStyle();
QString styleSheet();
QIcon icon(Icon icon, const QColor &normal, const QColor &selected = QColor());

class ElidedLabel final : public QLabel {
public:
    explicit ElidedLabel(QWidget *parent = nullptr);
    void setFullText(const QString &text);
protected:
    void resizeEvent(QResizeEvent *) override;
private:
    void updateText();
    QString m_fullText;
};

class ComboBox final : public QComboBox {
public:
    explicit ComboBox(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

class Toggle final : public QCheckBox {
public:
    explicit Toggle(const QString &accessibleName, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &position) const override;
};

class PreviewStage final : public QWidget {
public:
    explicit PreviewStage(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

class Mark final : public QWidget {
public:
    explicit Mark(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};
}
