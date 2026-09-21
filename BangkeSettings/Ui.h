#pragma once

#include <QAbstractButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

// 度量集中在这里:卡片内边距、设置行高、页头间距。Theme.cpp 只管颜色与字号。
namespace Ui {
constexpr int kCardPadX = 20;
constexpr int kCardPadTop = 16;
constexpr int kCardPadBottom = 18;
constexpr int kRowPadY = 14;   // 行高 = 14 + 一行 17 + 14 ≈ 46
constexpr int kRowGap = 16;
}  // namespace Ui

// 微信式滑动开关:药丸轨道 + 滑块,替代 QCheckBox
class ToggleSwitch : public QAbstractButton {
 public:
  explicit ToggleSwitch(QWidget* parent = nullptr) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(46, 26);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? (dark ? QColor(0x2D, 0xD4, 0xBF) : QColor(0x0E, 0x93, 0x84))
                           : (dark ? QColor(0x33, 0x3B, 0x3D)
                                   : QColor(0xD8, 0xDD, 0xDE)));
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    const int d = height() - 6;
    const int x = isChecked() ? width() - d - 3 : 3;
    p.setBrush(dark && isChecked() ? QColor(0x08, 0x20, 0x1C) : QColor(0xFF, 0xFF, 0xFF));
    p.drawEllipse(x, 3, d, d);
  }
};

// 卡片容器:QFrame#card + 可选标题。底色/圆角/发丝边由 Theme 的 QSS 提供,
// 投影只能在这里挂 —— QSS 没有 box-shadow。只给卡片加,不做成通用开关。
inline QFrame* makeCard(const QString& title, QLayout* inner,
                        QWidget* parent = nullptr) {
  auto* f = new QFrame(parent);
  f->setObjectName(QStringLiteral("card"));
  auto* v = new QVBoxLayout(f);
  v->setContentsMargins(Ui::kCardPadX, Ui::kCardPadTop, Ui::kCardPadX,
                        Ui::kCardPadBottom);
  v->setSpacing(10);
  if (!title.isEmpty()) {
    auto* t = new QLabel(title, f);
    t->setObjectName(QStringLiteral("cardTitle"));
    v->addWidget(t);
  }
  v->addLayout(inner);
  auto* shadow = new QGraphicsDropShadowEffect(f);
  shadow->setBlurRadius(26);
  shadow->setOffset(0, 6);
  shadow->setColor(QColor(16, 24, 26, 28));
  f->setGraphicsEffect(shadow);
  return f;
}

// 侧栏图标:16x16 线性图,用 QPainterPath 画 —— 引 QtSvg 只为一个图标不值当,
// 那是多一个依赖加几个 DLL。两张 pixmap 分别给常态与选中态(item view 会按 Selected
// 模式取),颜色因此跟着主题走。
inline QIcon makeNavIcon(int which) {
  const QColor normal(0x5C, 0x66, 0x68);
  const QColor selected(0x0A, 0x6A, 0x60);
  const auto draw = [which](const QColor& c) {
    QPixmap pm(32, 32);
    pm.setDevicePixelRatio(2.0);  // 16 逻辑像素,高分屏不糊
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (which) {
      case 0:  // 通用:三条滑杆
        for (int i = 0; i < 3; ++i) {
          const qreal y = 3.5 + i * 4.5;
          p.drawLine(QPointF(1.5, y), QPointF(14.5, y));
        }
        p.setBrush(c);
        p.drawEllipse(QPointF(5.0, 3.5), 2.0, 2.0);
        p.drawEllipse(QPointF(11.0, 8.0), 2.0, 2.0);
        p.drawEllipse(QPointF(7.0, 12.5), 2.0, 2.0);
        break;
      case 1:  // 界面样式:圆 + 横线(调色/光圈的意象)
        p.drawEllipse(QPointF(8.0, 8.0), 6.4, 6.4);
        p.drawLine(QPointF(1.6, 8.0), QPointF(14.4, 8.0));
        p.drawArc(QRectF(3.4, 3.4, 9.2, 9.2), 60 * 16, 120 * 16);
        break;
      case 2:  // AI:四角星
        {
          QPainterPath star;
          star.moveTo(8.0, 1.4);
          star.quadTo(8.9, 6.2, 14.6, 8.0);
          star.quadTo(8.9, 9.8, 8.0, 14.6);
          star.quadTo(7.1, 9.8, 1.4, 8.0);
          star.quadTo(7.1, 6.2, 8.0, 1.4);
          p.drawPath(star);
        }
        break;
      default:  // 高级:两层(堆叠)
        p.drawRoundedRect(QRectF(2.0, 2.6, 12.0, 4.6), 1.6, 1.6);
        p.drawRoundedRect(QRectF(2.0, 9.0, 12.0, 4.6), 1.6, 1.6);
        break;
    }
    p.end();
    return pm;
  };
  QIcon icon;
  icon.addPixmap(draw(normal), QIcon::Normal);
  icon.addPixmap(draw(selected), QIcon::Selected);
  return icon;
}

// 微信式设置行:标题(+灰色副文案)居左,控件贴右缘垂直居中
inline QWidget* makeSettingRow(const QString& title, QWidget* control,
                               const QString& helper = QString()) {
  auto* row = new QWidget;
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, Ui::kRowPadY, 0, Ui::kRowPadY);
  h->setSpacing(Ui::kRowGap);
  auto* text = new QVBoxLayout();
  text->setSpacing(2);
  auto* t = new QLabel(title);
  t->setObjectName(QStringLiteral("rowTitle"));
  text->addWidget(t);
  if (!helper.isEmpty()) {
    auto* sub = new QLabel(helper);
    sub->setObjectName(QStringLiteral("rowHint"));
    text->addWidget(sub);
  }
  h->addLayout(text);
  h->addStretch(1);
  if (control)
    h->addWidget(control, 0, Qt::AlignRight | Qt::AlignVCenter);
  return row;
}

inline QFrame* makeHairline() {
  auto* line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  return line;
}

// 行组:上下排布,行间自动插发丝分隔线
inline QLayout* makeSettingRows(const std::vector<QWidget*>& rows) {
  auto* v = new QVBoxLayout();
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (i)
      v->addWidget(makeHairline());
    v->addWidget(rows[i]);
  }
  return v;
}

// 非阻塞完成提示:父窗口内浮层,自动消散
inline void makeToast(const QString& text, QWidget* host) {
  auto* toast = new QLabel(text, host->window());
  toast->setObjectName(QStringLiteral("toast"));
  toast->setAlignment(Qt::AlignCenter);
  toast->adjustSize();
  auto* w = host->window();
  toast->move(w->width() / 2 - toast->width() / 2,
              w->height() - toast->height() - 56);
  toast->show();
  toast->raise();
  QTimer::singleShot(1800, toast, &QObject::deleteLater);
}
