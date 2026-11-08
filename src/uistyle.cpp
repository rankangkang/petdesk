#include "uistyle.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyleFactory>
#include <algorithm>

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
    result.addPixmap(drawIcon(value, QColor("#aab5aa")), QIcon::Disabled, QIcon::Off);
    return result;
}

void applyApplicationStyle() {
    if (auto *fusion = QStyleFactory::create("Fusion")) QApplication::setStyle(fusion);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f5f5ef"));
    palette.setColor(QPalette::WindowText, QColor("#263b32"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f2f4ee"));
    palette.setColor(QPalette::Text, QColor("#263b32"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#263b32"));
    palette.setColor(QPalette::Highlight, QColor("#436c55"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#859188"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a7b0a8"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a7b0a8"));
    QApplication::setPalette(palette);
    QFont font = QApplication::font();
    font.setFamilies({"Inter", "SF Pro Text", "PingFang SC", "Microsoft YaHei UI", "Noto Sans CJK SC", "sans-serif"});
    font.setPointSizeF(10.5);
    QApplication::setFont(font);
}

QString styleSheet() {
    return QString::fromUtf8(R"(
QMainWindow#console, QDialog#importPreview { background: #f5f5ef; color: #263b32; }
QWidget#consoleRoot, QWidget#pageContent, QScrollArea#pageScroll, QScrollArea#petLibrary,
QScrollArea#removedPets, QTabWidget#libraryTabs::pane { background: transparent; border: 0; }
QScrollArea#petLibrary QWidget#qt_scrollarea_viewport, QScrollArea#removedPets QWidget#qt_scrollarea_viewport,
QWidget#petListContent { background: transparent; }
QFrame#sidebar { background: #213a31; border: 0; }
QLabel#wordmark { color: #f7f8f1; font-size: 22px; font-weight: 700; }
QLabel#brandCaption { color: #a8bca9; font-size: 10px; }
QLabel#sideSection { color: #8da594; font-size: 10px; font-weight: 600; }
QLabel#sideFoot { color: #a8bca9; font-size: 11px; }
QFrame#sideTip { background: #2c483a; border: 1px solid #42604d; border-radius: 12px; }
QLabel#sideTipTitle { color: #e1ebe0; font-size: 11px; font-weight: 700; }
QLabel#sideTipText { color: #abc2ae; font-size: 11px; }
QPushButton[nav="true"] { background: transparent; color: #c5d4c5; text-align: left;
  border: 1px solid transparent; border-radius: 10px; padding: 0 13px; font-size: 13px; font-weight: 500; }
QPushButton[nav="true"]:hover { background: #2d4a3d; color: #ffffff; }
QPushButton[nav="true"]:checked { background: #3d5c4c; border-color: #506e5b; color: #ffffff; font-weight: 600; }
QPushButton[nav="true"]:focus { border: 1px solid #a4bd9d; }
QLabel#eyebrow { color: #607466; font-size: 10px; font-weight: 700; }
QLabel#pageTitle { color: #203a30; font-size: 29px; font-weight: 700; }
QLabel#pageSubtitle, QLabel#helper, QLabel#description { color: #607466; font-size: 12px; }
QLabel#sectionTitle { color: #30463a; font-size: 14px; font-weight: 700; }
QLabel#sectionCaption { color: #607466; font-size: 11px; }
QLabel#emptyTitle { color: #253e30; font-size: 18px; font-weight: 700; }
QLabel#settingTitle { color: #344d3b; font-size: 12px; font-weight: 600; }
QLabel#libraryCount { color: #607466; font-size: 11px; }
QLabel#petName { color: #253b31; font-size: 22px; font-weight: 700; }
QLabel#statusPill { color: #416a4e; background: #e8f1e5; border-radius: 9px; padding: 4px 9px; font-size: 11px; font-weight: 600; }
QLabel#capabilities { color: #607466; font-size: 11px; font-weight: 600; }
QLabel#currentPetName { color: #476a55; background: #e7ede1; border-radius: 11px; padding: 11px 15px; font-size: 13px; font-weight: 600; }
QFrame#sectionCard, QFrame#detailCard, QFrame#emptyCard, QFrame#listCard,
QFrame#removedCard, QFrame#aboutCard { background: #ffffff; border: 1px solid #e5eae2; border-radius: 16px; }
QFrame#previewStage { background: #edf2e8; border: 0; border-radius: 13px; }
QLabel#divider { background: #e9eee7; max-height: 1px; }
QPushButton { color: #476052; background: #ffffff; border: 1px solid #dce5db;
  border-radius: 9px; padding: 7px 13px; font-size: 12px; font-weight: 600; }
QPushButton:hover { background: #f2f6f0; border-color: #bdd0bd; }
QPushButton:pressed { background: #e5eee3; }
QPushButton:focus { border: 1px solid #80a184; }
QPushButton:disabled { color: #b2bbb2; background: #f3f5f1; border-color: #e7ebe5; }
QPushButton[role="primary"] { color: #ffffff; background: #365d47; border-color: #365d47; }
QPushButton[role="primary"]:hover { background: #426e54; border-color: #426e54; }
QPushButton[role="primary"]:pressed { background: #284b38; }
QPushButton[role="primary"]:disabled { color: #e7ebe6; background: #aebcb0; border-color: #aebcb0; }
QPushButton[role="quiet"] { background: transparent; border-color: transparent; color: #607466; }
QPushButton[role="quiet"]:hover { background: #eef3ec; color: #365d47; }
QPushButton[role="danger"] { color: #956458; background: transparent; border-color: transparent; }
QPushButton[role="danger"]:hover { color: #95584a; background: #faf0ec; }
QRadioButton[petChoice="true"] { color: #415a49; background: transparent; border: 1px solid transparent;
  border-radius: 11px; padding: 7px 11px; spacing: 9px; font-size: 12px; font-weight: 600; }
QRadioButton[petChoice="true"]:hover { background: #f3f7f0; }
QRadioButton[petChoice="true"]:checked { background: #edf4e9; border-color: #d8e7d4; color: #2c523a; }
QRadioButton[petChoice="true"]:focus { border-color: #83a788; }
QRadioButton[petChoice="true"]::indicator { width: 0px; height: 0px; image: none; background: transparent; border: 0; }
QTabWidget#libraryTabs::pane { border: 0; top: 0px; }
QTabWidget#libraryTabs::tab-bar { alignment: left; }
QTabBar::tab { color: #607466; background: transparent; border: 0; padding: 10px 15px 9px 0px; margin-right: 18px; font-size: 12px; font-weight: 600; }
QTabBar::tab:selected { color: #365d47; border-bottom: 2px solid #54795e; }
QTabBar::tab:hover { color: #365d47; }
QComboBox { color: #354e3e; background: #fbfcfa; border: 1px solid #dce5db; border-radius: 8px;
  padding: 7px 32px 7px 12px; min-height: 20px; font-size: 12px; }
QComboBox:hover { border-color: #a9c2ab; }
QComboBox:focus { border-color: #739b7a; }
QComboBox:disabled { color: #afb8af; background: #f3f5f1; }
QComboBox::drop-down { border: 0; width: 24px; }
QComboBox::down-arrow { image: none; }
QComboBox QAbstractItemView { color: #30463a; background: #ffffff; border: 1px solid #dce5db;
  selection-background-color: #e9f1e6; selection-color: #264832; outline: 0; padding: 4px; }
QScrollBar:vertical { background: transparent; width: 9px; margin: 5px 0; }
QScrollBar::handle:vertical { background: #d1dbcf; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QLabel#notice { color: #365d47; background: #e7f1e4; border: 0; border-radius: 10px; padding: 10px 14px; font-size: 12px; }
QLabel#notice[severity="error"] { color: #874f42; background: #f9eae4; }
QFrame#bottomBar { background: #fbfcf9; border-top: 1px solid #e7ede5; }
QLabel#status { color: #657c69; font-size: 12px; font-weight: 600; }
QLabel#footDot { color: #72a77a; font-size: 12px; }
QLabel#footDot[running="false"] { color: #c1c8bd; }
QDialog#importPreview QLabel { color: #314a3a; }
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
    p.setPen(QPen(isEnabled() ? QColor("#788e7d") : QColor("#b1bcb2"), 1.5,
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
    QRectF track(1, 3, 42, 22);
    if (hasFocus()) { p.setPen(QPen(QColor("#8caf8f"), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(track.adjusted(-1, -1, 1, 1), 12, 12); }
    p.setPen(Qt::NoPen);
    p.setBrush(!isEnabled() ? QColor("#e5e9e2") : isChecked() ? QColor("#54795e") : QColor("#ccd6cb"));
    p.drawRoundedRect(track, 11, 11);
    p.setBrush(!isEnabled() ? QColor("#f5f6f4") : Qt::white);
    p.drawEllipse(QRectF(isChecked() ? 23 : 3, 5, 18, 18));
}
PreviewStage::PreviewStage(QWidget *parent) : QWidget(parent) {
    setObjectName("previewStage"); setMinimumHeight(190); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
void PreviewStage::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip; clip.addRoundedRect(rect(), 13, 13); p.setClipPath(clip);
    p.fillRect(rect(), QColor("#edf2e8"));
    const qreal side = std::min(width() * 0.72, height() * 0.95);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#e1eadd"));
    p.drawEllipse(QPointF(width() * 0.50, height() * 0.57), side / 2, side / 2);
    p.setBrush(QColor("#e8c5ad")); p.drawEllipse(QPointF(width() * 0.79, height() * 0.23), 5, 5);
    p.setPen(QPen(QColor("#cbdacb"), 1));
    p.drawLine(QPointF(width() * 0.14, height() * 0.77), QPointF(width() * 0.24, height() * 0.77));
    p.drawLine(QPointF(width() * 0.19, height() * 0.72), QPointF(width() * 0.19, height() * 0.82));
}
Mark::Mark(QWidget *parent) : QWidget(parent) { setFixedSize(37, 37); }
void Mark::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
    p.setBrush(QColor("#c8d9bb")); p.drawRoundedRect(rect(), 11, 11);
    p.setBrush(QColor("#32533e"));
    p.drawRoundedRect(QRectF(9, 11, 18, 15), 3, 3);
    p.setBrush(QColor("#c8d9bb")); p.drawRoundedRect(QRectF(11, 14, 14, 9), 1.5, 1.5);
    p.setBrush(QColor("#32533e")); p.drawEllipse(QPointF(15, 18.5), 1, 1); p.drawEllipse(QPointF(21, 18.5), 1, 1);
}
}
