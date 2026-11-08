#include "uistyle.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyleFactory>

namespace petdesk::ui {
namespace {
QPixmap drawIcon(Icon icon, QColor color) {
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(2);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (icon) {
    case Icon::Library:
        for (int y : {3, 11}) for (int x : {3, 11}) p.drawRoundedRect(QRectF(x, y, 6, 6), 1.4, 1.4);
        break;
    case Icon::Sliders:
        p.drawLine(QPointF(3, 6), QPointF(17, 6)); p.drawLine(QPointF(3, 14), QPointF(17, 14));
        p.setBrush(color); p.drawEllipse(QPointF(7, 6), 2, 2); p.drawEllipse(QPointF(13, 14), 2, 2);
        break;
    case Icon::Settings:
        p.drawEllipse(QRectF(4, 4, 12, 12)); p.drawEllipse(QRectF(8, 8, 4, 4));
        for (int i = 0; i < 4; ++i) { p.save(); p.translate(10, 10); p.rotate(90 * i); p.drawLine(QPointF(0, -8), QPointF(0, -9)); p.restore(); }
        break;
    case Icon::Info:
        p.drawEllipse(QRectF(3, 3, 14, 14)); p.drawLine(QPointF(10, 9), QPointF(10, 14));
        p.setBrush(color); p.drawEllipse(QPointF(10, 6.5), 0.6, 0.6); break;
    case Icon::Plus: p.drawLine(QPointF(10, 4), QPointF(10, 16)); p.drawLine(QPointF(4, 10), QPointF(16, 10)); break;
    case Icon::Folder: {
        QPainterPath path; path.moveTo(3, 5); path.lineTo(8, 5); path.lineTo(9.5, 7);
        path.lineTo(17, 7); path.lineTo(17, 15.5); path.lineTo(3, 15.5); path.closeSubpath(); p.drawPath(path); break;
    }
    case Icon::Eye: {
        QPainterPath path; path.moveTo(2.5, 10); path.cubicTo(6, 4.4, 14, 4.4, 17.5, 10);
        path.cubicTo(14, 15.6, 6, 15.6, 2.5, 10); p.drawPath(path); p.drawEllipse(QPointF(10, 10), 2.2, 2.2); break;
    }
    case Icon::Hide:
        p.drawRoundedRect(QRectF(3, 4, 14, 12), 2, 2); p.drawLine(QPointF(6, 10), QPointF(14, 10)); break;
    case Icon::Exit:
        p.drawLine(QPointF(9, 4), QPointF(5, 4)); p.drawLine(QPointF(5, 4), QPointF(5, 16));
        p.drawLine(QPointF(5, 16), QPointF(9, 16)); p.drawLine(QPointF(9, 10), QPointF(17, 10));
        p.drawLine(QPointF(14, 7), QPointF(17, 10)); p.drawLine(QPointF(14, 13), QPointF(17, 10)); break;
    case Icon::Arrow: p.drawLine(QPointF(4, 10), QPointF(16, 10)); p.drawLine(QPointF(12, 6), QPointF(16, 10)); p.drawLine(QPointF(12, 14), QPointF(16, 10)); break;
    case Icon::Restore:
        p.drawArc(QRectF(3, 3, 14, 14), 30 * 16, -285 * 16); p.drawLine(QPointF(4, 3), QPointF(4, 7)); p.drawLine(QPointF(4, 7), QPointF(8, 7)); break;
    }
    return pixmap;
}
}

QIcon icon(Icon value, const QColor &normal, const QColor &selected) {
    QIcon result;
    result.addPixmap(drawIcon(value, normal), QIcon::Normal, QIcon::Off);
    result.addPixmap(drawIcon(value, selected.isValid() ? selected : normal), QIcon::Normal, QIcon::On);
    result.addPixmap(drawIcon(value, normal), QIcon::Active, QIcon::Off);
    result.addPixmap(drawIcon(value, selected.isValid() ? selected : normal), QIcon::Active, QIcon::On);
    result.addPixmap(drawIcon(value, QColor("#b5b5b5")), QIcon::Disabled, QIcon::Off);
    return result;
}

void applyApplicationStyle() {
    if (auto *fusion = QStyleFactory::create("Fusion")) QApplication::setStyle(fusion);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#ffffff"));
    palette.setColor(QPalette::WindowText, QColor("#202123"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7f7f8"));
    palette.setColor(QPalette::Text, QColor("#202123"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#202123"));
    palette.setColor(QPalette::Highlight, QColor("#4e83ed"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#8e8e8e"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a3a3a3"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a3a3a3"));
    QApplication::setPalette(palette);
    QFont font = QApplication::font();
    font.setFamilies({"Inter", "SF Pro Text", "PingFang SC", "Microsoft YaHei UI", "Noto Sans CJK SC", "sans-serif"});
    font.setPointSizeF(10.5);
    QApplication::setFont(font);
}

QString styleSheet() {
    return QString::fromUtf8(R"(
QMainWindow#console, QDialog#importPreview { background: #ffffff; color: #202123; }
QWidget#consoleRoot, QWidget#pageContent, QWidget#pageColumn, QScrollArea#pageScroll,
QScrollArea#petLibrary, QScrollArea#removedPets, QTabWidget#libraryTabs::pane { background: transparent; border: 0; }
QScrollArea#petLibrary QWidget#qt_scrollarea_viewport, QScrollArea#removedPets QWidget#qt_scrollarea_viewport,
QWidget#petListContent { background: transparent; }
QFrame#sidebar { background: #f8f8f8; border: 0; border-right: 1px solid #ededed; }
QLabel#wordmark { color: #202123; font-size: 16px; font-weight: 700; }
QLabel#sideFoot { color: #8a8a8a; font-size: 11px; }
QPushButton[nav="true"] { background: transparent; color: #383838; text-align: left;
  border: 1px solid transparent; border-radius: 7px; padding: 0 9px; font-size: 13px; font-weight: 500; }
QPushButton[nav="true"]:hover { background: #eeeeef; }
QPushButton[nav="true"]:checked { background: #e9e9ea; color: #202123; font-weight: 600; }
QPushButton[nav="true"]:focus { border: 1px solid #aebbd7; }
QLabel#pageTitle { color: #202123; font-size: 23px; font-weight: 700; }
QLabel#helper, QLabel#description, QLabel#settingValue { color: #6e6e70; font-size: 12px; }
QLabel#sectionTitle { color: #262626; font-size: 13px; font-weight: 600; }
QLabel#sectionCaption { color: #737373; font-size: 11px; }
QLabel#emptyTitle { color: #262626; font-size: 15px; font-weight: 600; }
QLabel#settingTitle { color: #2f2f2f; font-size: 12px; font-weight: 600; }
QLabel#petName { color: #202123; font-size: 18px; font-weight: 700; }
QLabel#capabilities { color: #6e6e70; font-size: 11px; }
QLabel#currentPetName { color: #6e6e70; font-size: 12px; }
QFrame#sectionCard, QFrame#detailCard, QFrame#emptyCard,
QFrame#removedCard, QFrame#aboutCard { background: #ffffff; border: 1px solid #e7e7e7; border-radius: 9px; }
QFrame#listCard { background: transparent; border: 0; }
QLabel#divider { background: #ececec; max-height: 1px; }
QPushButton { color: #303030; background: #ffffff; border: 1px solid #dedede;
  border-radius: 6px; padding: 7px 12px; font-size: 12px; font-weight: 500; }
QPushButton:hover { background: #f7f7f7; border-color: #c8c8c8; }
QPushButton:pressed { background: #eeeeee; }
QPushButton:focus { border: 1px solid #8eaef3; }
QPushButton:disabled { color: #a8a8a8; background: #f7f7f7; border-color: #ededed; }
QPushButton[role="primary"] { color: #ffffff; background: #252525; border-color: #252525; }
QPushButton[role="primary"]:hover { background: #3d3d3d; border-color: #3d3d3d; }
QPushButton[role="primary"]:pressed { background: #111111; }
QPushButton[role="primary"]:disabled { color: #f5f5f5; background: #b7b7b7; border-color: #b7b7b7; }
QPushButton[role="quiet"] { background: transparent; border-color: transparent; color: #5e5e5e; }
QPushButton[role="quiet"]:hover { background: #f3f3f3; color: #202123; }
QPushButton[role="danger"] { color: #a53f3f; background: transparent; border-color: transparent; }
QPushButton[role="danger"]:hover { color: #8d2929; background: #fbefef; }
QRadioButton[petChoice="true"] { color: #303030; background: transparent; border: 1px solid transparent;
  border-radius: 7px; padding: 7px 10px; spacing: 9px; font-size: 12px; font-weight: 500; }
QRadioButton[petChoice="true"]:hover { background: #f6f6f6; }
QRadioButton[petChoice="true"]:checked { background: #eeeeef; color: #202123; }
QRadioButton[petChoice="true"]:focus { border-color: #aebbd7; }
QRadioButton[petChoice="true"]::indicator { width: 0px; height: 0px; image: none; background: transparent; border: 0; }
QTabWidget#libraryTabs::pane { border: 0; top: 0px; }
QTabWidget#libraryTabs::tab-bar { alignment: left; }
QTabBar::tab { color: #707070; background: transparent; border: 0; padding: 10px 12px 9px 0px; margin-right: 18px; font-size: 12px; font-weight: 500; }
QTabBar::tab:selected { color: #202123; border-bottom: 2px solid #333333; font-weight: 600; }
QTabBar::tab:hover { color: #202123; }
QComboBox { color: #303030; background: #ffffff; border: 1px solid #dedede; border-radius: 6px;
  padding: 6px 30px 6px 10px; min-height: 20px; font-size: 12px; }
QComboBox:hover { border-color: #bdbdbd; }
QComboBox:focus { border-color: #8eaef3; }
QComboBox:disabled { color: #aaaaaa; background: #f7f7f7; }
QComboBox::drop-down { border: 0; width: 24px; }
QComboBox::down-arrow { image: none; }
QComboBox QAbstractItemView { color: #303030; background: #ffffff; border: 1px solid #dedede;
  selection-background-color: #ededed; selection-color: #202123; outline: 0; padding: 4px; }
QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #e5e5e5; }
QSlider::sub-page:horizontal { background: #4e83ed; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -6px 0; border-radius: 7px;
  background: #ffffff; border: 1px solid #bdbdbd; }
QSlider::handle:horizontal:hover { border-color: #888888; }
QSlider:focus::handle:horizontal { border: 2px solid #4e83ed; }
QLabel#petSizeValue { color: #555555; font-size: 12px; }
QScrollBar:vertical { background: transparent; width: 9px; margin: 5px 0; }
QScrollBar::handle:vertical { background: #d5d5d5; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QLabel#notice { color: #345d9c; background: #eef4ff; border: 0; border-radius: 6px; padding: 10px 14px; font-size: 12px; }
QLabel#notice[severity="error"] { color: #963b3b; background: #fbefef; }
QFrame#bottomBar { background: #ffffff; border-top: 1px solid #ececec; }
QLabel#status { color: #666666; font-size: 12px; }
QLabel#footDot { color: #4e83ed; font-size: 11px; }
QLabel#footDot[running="false"] { color: #c3c3c3; }
)");
}

ElidedLabel::ElidedLabel(QWidget *parent) : QLabel(parent) {
    setTextFormat(Qt::PlainText); setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}
void ElidedLabel::setFullText(const QString &value) {
    m_fullText = value; setAccessibleName(value); setToolTip(value); updateText();
}
void ElidedLabel::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event); updateText();
}
void ElidedLabel::updateText() {
    const QString elided = fontMetrics().elidedText(m_fullText, Qt::ElideRight, contentsRect().width());
    if (text() != elided) setText(elided);
}
ComboBox::ComboBox(QWidget *parent) : QComboBox(parent) {}
void ComboBox::paintEvent(QPaintEvent *event) {
    QComboBox::paintEvent(event);
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(isEnabled() ? QColor("#777777") : QColor("#b5b5b5"), 1.5,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal x = layoutDirection() == Qt::RightToLeft ? 17 : width() - 17;
    const qreal y = height() / 2.0 - 2;
    p.drawLine(QPointF(x - 4, y), QPointF(x, y + 4));
    p.drawLine(QPointF(x, y + 4), QPointF(x + 4, y));
}
Toggle::Toggle(const QString &name, QWidget *parent) : QCheckBox(parent) {
    setAccessibleName(name); setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus); setFixedSize(44, 28);
}
QSize Toggle::sizeHint() const { return {44, 28}; }
QSize Toggle::minimumSizeHint() const { return sizeHint(); }
bool Toggle::hitButton(const QPoint &position) const { return rect().contains(position); }
void Toggle::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRectF track(4, 5, 36, 18);
    if (hasFocus()) { p.setPen(QPen(QColor("#a8bff1"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(track.adjusted(-2, -2, 2, 2), 11, 11); }
    p.setPen(Qt::NoPen);
    p.setBrush(!isEnabled() ? QColor("#e5e5e5") : isChecked() ? QColor("#4e83ed") : QColor("#d5d5d5"));
    p.drawRoundedRect(track, 9, 9);
    p.setBrush(!isEnabled() ? QColor("#f5f5f5") : Qt::white);
    p.drawEllipse(QRectF(isChecked() ? 22 : 6, 7, 14, 14));
}
PreviewStage::PreviewStage(QWidget *parent) : QWidget(parent) {
    setObjectName("previewStage"); setMinimumHeight(190); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void PreviewStage::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#f7f7f8"));
    p.drawRoundedRect(rect(), 7, 7);
}
Mark::Mark(QWidget *parent) : QWidget(parent) { setFixedSize(37, 37); }
void Mark::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#777777"), 1.5)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(7, 8, 23, 21), 5, 5);
    p.drawEllipse(QPointF(15, 17), 1, 1); p.drawEllipse(QPointF(22, 17), 1, 1);
    p.drawLine(QPointF(15, 23), QPointF(22, 23));
}
}
