// 蚌壳拼音安装器前端：UI 由本程序承担，MSI 以 msiexec /qn 子进程执行。
// 子进程隔离保证 UI 崩溃不影响安装事务（2026-09-06 一次 UI 层 UAF 崩溃验证了这一点）。
// 阶段进度来自对 verbose log 的增量读取，不做假百分比。
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include <tuple>
#include <cstring>

static const wchar_t* kUpgradeCode = L"{8F1D4B33-9C2A-4E6D-B0F7-3A5C8E21D940}";

static QString FindMsi() {
  QDir dir(QApplication::applicationDirPath());
  const auto msis = dir.entryList({QStringLiteral("BangkeSetup-*.msi")}, QDir::Files, QDir::Name);
  return msis.isEmpty() ? QString() : dir.absoluteFilePath(msis.last());
}

static QString InstalledProductCode() {
  wchar_t code[40] = {0};
  for (DWORD i = 0;; ++i) {
    DWORD sz = 40;
    UINT r = MsiEnumRelatedProductsW(kUpgradeCode, 0, i, code);
    if (r != ERROR_SUCCESS)
      break;
    if (MsiQueryProductState(code) == INSTALLSTATE_DEFAULT)
      return QString::fromWCharArray(code);
  }
  return QString();
}

static QString InstalledVersion(const QString& code) {
  wchar_t v[64] = {0};
  DWORD sz = 64;
  if (MsiGetProductInfoW(code.toStdWString().c_str(), INSTALLPROPERTY_VERSIONSTRING, v, &sz) == ERROR_SUCCESS)
    return QString::fromWCharArray(v);
  return QStringLiteral(u"未知");
}

// 从安装包的 Property 表读 ProductVersion，用于欢迎页的版本对比展示
static QString MsiPackageVersion(const QString& path) {
  MSIHANDLE db = 0;
  if (MsiOpenDatabaseW(path.toStdWString().c_str(), MSIDBOPEN_READONLY, &db) != ERROR_SUCCESS)
    return QString();
  QString result;
  MSIHANDLE view = 0;
  if (MsiDatabaseOpenViewW(db,
                           L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'",
                           &view) == ERROR_SUCCESS) {
    if (MsiViewExecute(view, 0) == ERROR_SUCCESS) {
      MSIHANDLE rec = 0;
      if (MsiViewFetch(view, &rec) == ERROR_SUCCESS) {
        wchar_t buf[64] = {0};
        DWORD sz = 64;
        if (MsiRecordGetStringW(rec, 1, buf, &sz) == ERROR_SUCCESS)
          result = QString::fromWCharArray(buf, sz);
        MsiCloseHandle(rec);
      }
      MsiViewClose(view);
    }
    MsiCloseHandle(view);
  }
  MsiCloseHandle(db);
  return result;
}

static std::tuple<int, int, int> ParseVersion(const QString& v) {
  const auto parts = v.split('.');
  int a = 0, b = 0, c = 0;
  if (parts.size() > 0) a = parts[0].toInt();
  if (parts.size() > 1) b = parts[1].toInt();
  if (parts.size() > 2) c = parts[2].toInt();
  return {a, b, c};
}

// ---------------- MSI 执行 ----------------

struct MsiJob {
  enum Op { Install, Repair, Uninstall } op;
  QString msiPath, productCode;
  bool wasInstalled = false;  // 操作开始前产品是否已装（区分安装/升级文案）
};

static QString MsiLogPath(const QString& msi) { return msi + ".log"; }

static QStringList BuildArgs(const MsiJob& job) {
  const QString log = QDir::toNativeSeparators(MsiLogPath(job.msiPath));
  // 修复与卸载的对象是"已经装上的那个产品",所以按它的 ProductCode 走,不能拿随包这份
  // MSI 的路径当 target:WiX 没写死 ProductCode,每次构建都是一个新 GUID,/x <这份包的
  // 路径> 卸的是"这份包代表的产品",而它还没装上 —— 报 1605(该操作只对当前已安装的产品
  // 有效),用户看到的就是"点卸载报错、产品还在"。
  // 安装/升级则必须走 /i <路径>:MajorUpgrade 按 UpgradeCode 接管旧版本。
  const QString installed = job.productCode.isEmpty()
                                ? QDir::toNativeSeparators(job.msiPath)
                                : job.productCode;
  QStringList args;
  switch (job.op) {
    case MsiJob::Install:
      args << "/i" << QDir::toNativeSeparators(job.msiPath);
      break;
    case MsiJob::Repair:
      args << "/f" << installed;
      break;
    case MsiJob::Uninstall:
      args << "/x" << installed;
      break;
  }
  // REBOOT=ReallySuppress:没有它,当 MSI 需要重启才能完成文件替换时(用户的
  // System32\bangke.dll 被 explorer/msedge 这些 TSF 宿主占着,替换要等重启),
  // msiexec 会直接重启机器 —— 用户看到的是"装完毫无提示就重启了"。
  // 重启或注销与否由用户在完成页自己决定(见 msiFinished)。
  args << "/qn" << "/l*v" << log << "REBOOT=ReallySuppress";
  return args;
}

// ---------------- 动效部件 ----------------

// 不确定进度条:细轨道 + 循环扫光,替代 QProgressBar 的呆板 busy 块
class BusyBar : public QWidget {
 public:
  explicit BusyBar(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedHeight(6);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* a = new QVariantAnimation(this);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setDuration(1400);
    a->setLoopCount(-1);
    connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
      m_phase = v.toReal();
      update();
    });
    a->start();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x0b, 0x11, 0x19));
    p.drawRoundedRect(rect(), 3, 3);
    const qreal chunk = width() * 0.36;
    const qreal x = -chunk + (width() + chunk * 2) * m_phase;
    QLinearGradient g(x, 0, x + chunk, 0);
    g.setColorAt(0.0, QColor(0x2f, 0x7c, 0xd6, 0));
    g.setColorAt(0.5, QColor(0x4a, 0xa8, 0xf0));
    g.setColorAt(1.0, QColor(0x2f, 0x7c, 0xd6, 0));
    p.setBrush(g);
    p.drawRoundedRect(QRectF(x, 0, chunk, height()), 3, 3);
  }

 private:
  qreal m_phase = 0.0;
};

// 完成页对勾:环先画出,勾/X 随笔画出
class CheckMark : public QWidget {
 public:
  explicit CheckMark(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedSize(72, 72);
    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(900);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
      m_t = v.toReal();
      update();
    });
  }
  void start(bool ok) {
    m_ok = ok;
    m_t = 0.0;
    update();
    m_anim->stop();
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->start();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor c = m_ok ? QColor(0x4c, 0xc3, 0x8a) : QColor(0xe0, 0x56, 0x56);
    const QRectF r = rect().adjusted(6, 6, -6, -6);
    QPen pen(c, 3.2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    const qreal ringT = qBound(0.0, m_t / 0.55, 1.0);
    p.drawArc(r, 90 * 16, -int(360 * 16 * ringT));
    if (m_t > 0.55) {
      const qreal t = (m_t - 0.55) / 0.45;
      const QPointF c0 = r.center();
      if (m_ok) {
        const QPointF a = c0 + QPointF(-r.width() * 0.24, r.height() * 0.02);
        const QPointF b = c0 + QPointF(-r.width() * 0.05, r.height() * 0.22);
        const QPointF d = c0 + QPointF(r.width() * 0.27, -r.height() * 0.20);
        const qreal t2 = t * 2;
        if (t2 <= 1.0)
          p.drawLine(a, a + (b - a) * t2);
        else {
          p.drawLine(a, b);
          p.drawLine(b, b + (d - b) * (t2 - 1.0));
        }
      } else {
        const QPointF tl = r.topLeft() + QPointF(r.width() * 0.22, r.height() * 0.22);
        const QPointF br = r.bottomRight() + QPointF(-r.width() * 0.22, -r.height() * 0.22);
        const QPointF bl = r.bottomLeft() + QPointF(r.width() * 0.22, -r.height() * 0.22);
        const QPointF tr = r.topRight() + QPointF(-r.width() * 0.22, r.height() * 0.22);
        const qreal t2 = t * 2;
        if (t2 <= 1.0)
          p.drawLine(tl, tl + (br - tl) * t2);
        else {
          p.drawLine(tl, br);
          p.drawLine(bl, bl + (tr - bl) * (t2 - 1.0));
        }
      }
    }
  }

 private:
  QVariantAnimation* m_anim = nullptr;
  qreal m_t = 0.0;
  bool m_ok = true;
};

// ---------------- UI ----------------

// 海滩场景:天/海/沙 + 循环冲刷的浪 + 随安装阶段张开的蚌壳与珍珠。
// 开合度 setProgress 绑定真实阶段序(非假百分比),插值缓动过渡
class BeachScene : public QWidget {
 public:
  explicit BeachScene(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedSize(340, 200);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this] {
      m_wave += 0.016;
      if (m_wave >= 1.0)
        m_wave -= 1.0;
      update();
    });
    m_openAnim = new QVariantAnimation(this);
    m_openAnim->setDuration(700);
    m_openAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_openAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) {
              m_open = v.toReal();
              update();
            });
  }
  void setProgress(qreal p) {
    p = qBound(0.0, p, 1.0);
    m_openAnim->stop();
    m_openAnim->setStartValue(m_open);
    m_openAnim->setEndValue(p);
    m_openAnim->start();
  }

 protected:
  void showEvent(QShowEvent* e) override {
    QWidget::showEvent(e);
    m_timer->start(33);  // 波浪只在可见时跑,不空烧 CPU
  }
  void hideEvent(QHideEvent* e) override {
    QWidget::hideEvent(e);
    m_timer->stop();
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int w = width(), h = height();
    QPainterPath clip;
    clip.addRoundedRect(rect(), 12, 12);
    p.setClipPath(clip);

    // 天空(晨光)
    QLinearGradient sky(0, 0, 0, h * 0.42);
    sky.setColorAt(0, QColor(0xff, 0xe8, 0xc2));
    sky.setColorAt(1, QColor(0xff, 0xc9, 0x8f));
    p.fillRect(QRect(0, 0, w, h * 0.42), sky);
    // 太阳光晕
    QRadialGradient sun(QPointF(w * 0.78, h * 0.15), 42);
    sun.setColorAt(0, QColor(255, 250, 230, 220));
    sun.setColorAt(1, QColor(255, 250, 230, 0));
    p.setBrush(sun);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(w * 0.78, h * 0.15), 42, 42);

    // 海
    QLinearGradient sea(0, h * 0.40, 0, h * 0.68);
    sea.setColorAt(0, QColor(0x3f, 0x9e, 0xd9));
    sea.setColorAt(1, QColor(0x1d, 0x6f, 0xa5));
    p.fillRect(QRect(0, h * 0.40, w, h * 0.28), sea);
    // 海面波光:两条微动细线
    for (int i = 0; i < 2; ++i) {
      const qreal y = h * (0.46 + i * 0.07);
      p.setPen(QPen(QColor(255, 255, 255, 70), 1.4));
      QPainterPath linePath;
      for (int x = 0; x <= w; x += 8) {
        const qreal yy = y + 1.6 * sin(x * 0.05 + m_wave * 6.28 + i * 2.0);
        if (x == 0)
          linePath.moveTo(x, yy);
        else
          linePath.lineTo(x, yy);
      }
      p.drawPath(linePath);
    }

    // 沙滩
    QLinearGradient sand(0, h * 0.66, 0, h);
    sand.setColorAt(0, QColor(0xf4, 0xdc, 0xb6));
    sand.setColorAt(1, QColor(0xe6, 0xc4, 0x92));
    p.setPen(Qt::NoPen);
    p.setBrush(sand);
    p.drawChord(QRect((int)(-w * 0.2), (int)(h * 0.60), (int)(w * 1.4),
                     (int)(h * 0.45)),
                 0, 180 * 16);

    // 浪沫:相位推进再退回,冲刷沙滩
    const qreal wt = m_wave < 0.55 ? m_wave / 0.55 : 1.0 - (m_wave - 0.55) / 0.45;
    const qreal eased = 1.0 - (1.0 - wt) * (1.0 - wt);
    const qreal waveY = h * 0.72 - 24 * eased;
    p.setPen(QPen(QColor(255, 255, 255, 90 + 90 * eased), 3.0, Qt::SolidLine,
                  Qt::RoundCap));
    QPainterPath foam;
    for (int x = 0; x <= w; x += 8) {
      const qreal yy = waveY + 2.2 * sin(x * 0.04 + m_wave * 6.28);
      if (x == 0)
        foam.moveTo(x, yy);
      else
        foam.lineTo(x, yy);
    }
    p.drawPath(foam);

    // ---- 蚌壳(中心铰链,两瓣随 m_open 张开) ----
    const QPointF hinge(w * 0.5, h * 0.86);
    const int R = 52;
    // 珍珠:开合度驱动大小与光晕
    if (m_open > 0.05) {
      const QPointF pc = hinge + QPointF(0, -R * 0.42 - 6 * m_open);
      const int pr = 11 + 5 * m_open;
      QRadialGradient glow(pc, pr * 2.2);
      glow.setColorAt(0, QColor(255, 245, 250, 150 * m_open));
      glow.setColorAt(1, QColor(255, 245, 250, 0));
      p.setBrush(glow);
      p.drawEllipse(pc, pr * 2.2, pr * 2.2);
      QRadialGradient pearl(pc.x() - pr * 0.3, pc.y() - pr * 0.3, pr * 1.6);
      pearl.setColorAt(0, QColor(0xff, 0xff, 0xff));
      pearl.setColorAt(0.55, QColor(0xf8, 0xe3, 0xea));
      pearl.setColorAt(1, QColor(0xd9, 0xb8, 0xc4));
      p.setBrush(pearl);
      p.drawEllipse(pc, pr, pr);
    }
    // 两瓣扇壳
    for (int side = -1; side <= 1; side += 2) {
      p.save();
      p.translate(hinge);
      p.rotate(side * (8 + 40 * m_open));  // 闭合时微张缝,进度满时 ~48°
      QPainterPath valve;
      valve.moveTo(0, 0);
      valve.arcTo(QRectF(-R, -R, R * 2, R * 2), 90, -side * 58.0);
      valve.closeSubpath();
      QLinearGradient shellGrad(0, -R, 0, 0);
      shellGrad.setColorAt(0, QColor(0xf7, 0xc9, 0x9b));
      shellGrad.setColorAt(1, QColor(0xd1, 0x92, 0x5c));
      p.setBrush(shellGrad);
      p.setPen(QPen(QColor(0xb5, 0x7c, 0x48), 1.2));
      p.drawPath(valve);
      // 放射肋线
      p.setPen(QPen(QColor(0xc0, 0x85, 0x50, 160), 1.0));
      for (int k = 1; k <= 3; ++k) {
        const qreal ang = (90 - side * (58.0 * k / 4.0)) * 3.14159 / 180.0;
        p.drawLine(QPointF(0, 0),
                   QPointF(R * 0.94 * cos(ang), -R * 0.94 * sin(ang)));
      }
      p.restore();
    }
    // 铰链阴影
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xb5, 0x8a, 0x5e, 90));
    p.drawEllipse(hinge + QPointF(0, 3), R * 0.8, 7);
  }

 private:
  QTimer* m_timer = nullptr;
  QVariantAnimation* m_openAnim = nullptr;
  qreal m_open = 0.0;
  qreal m_wave = 0.0;
};

// 自绘关闭钮:粗白 × 由线条画出,不依赖字体/系统图标(两者在用户环境均失灵)
class CloseButton : public QWidget {
 public:
  explicit CloseButton(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedSize(40, 28);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral(u"关闭"));
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    // 静止:浅钢蓝胶囊,与深底拉开层次;悬停/按下:红系
    p.setBrush(underMouse() ? QColor(0xd1, 0x34, 0x38)
                            : (isDown() ? QColor(0x2a, 0x3a, 0x4e)
                                        : QColor(0x2c, 0x3d, 0x52)));
    p.drawRoundedRect(rect(), 6, 6);
    QPen pen(isDown() ? QColor(0xff, 0xff, 0xff) : QColor(0xea, 0xf2, 0xfb),
             1.7);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    // 叉:占按钮约 40%,四边留白均匀
    const int m = 12;
    p.drawLine(m, 9, width() - m, height() - 9);
    p.drawLine(width() - m, 9, m, height() - 9);
  }
  void enterEvent(QEnterEvent*) override { update(); }
  void leaveEvent(QEvent*) override { update(); }
  void mousePressEvent(QMouseEvent*) override {
    pressed_ = true;
    update();
  }
  void mouseReleaseEvent(QMouseEvent* e) override {
    pressed_ = false;
    update();
    if (rect().contains(e->pos()))
      topLevelWidget()->close();
  }

 private:
  bool isDown() const { return pressed_; }
  bool pressed_ = false;
};

class MainWindow : public QWidget {
  Q_OBJECT

 public:
  MainWindow() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowOpacity(0.0);  // 入场动画从全透明起步
    resize(600, 440);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QFrame(this);
    panel->setObjectName("panel");
    // 深底浮层感:面板投影
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 150));
    panel->setGraphicsEffect(shadow);
    root->setContentsMargins(18, 14, 18, 22);  // 给投影留边
    root->addWidget(panel);

    auto* box = new QVBoxLayout(panel);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);

    auto* title = new QHBoxLayout();
    auto* titleText = new QLabel(QStringLiteral(u"蚌壳拼音 · 安装"), panel);
    titleText->setObjectName("title");
    m_closeBtn = new CloseButton(panel);
    title->addSpacing(24);
    title->addWidget(titleText);
    title->addStretch();
    title->addWidget(m_closeBtn);
    box->addLayout(title);

    m_stack = new QStackedWidget(panel);
    box->addWidget(m_stack, 1);
    buildWelcomePage();
    buildProgressPage();
    buildFinishPage();

    refreshState();
  }

 protected:
  void showEvent(QShowEvent* e) override {
    QWidget::showEvent(e);
    if (m_entered)
      return;
    m_entered = true;
    // 入场:淡入 + 14px 上滑
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(320);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
    const QPoint dst = pos();
    move(dst + QPoint(0, 14));
    auto* slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(380);
    slide->setStartValue(dst + QPoint(0, 14));
    slide->setEndValue(dst);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    slide->start(QAbstractAnimation::DeleteWhenStopped);
  }
  // 任务进行中禁止关闭：中断 msiexec 并不可靠（客户端进程被杀后服务端事务照跑），
  // 且安装中途退出 UI 曾引发 Qt 内部 UAF 崩溃，索性从结构上禁止该路径。
  void closeEvent(QCloseEvent* e) override {
    if (m_running) {
      e->ignore();
      m_progressHint->setText(QStringLiteral(u"操作正在进行，请稍候…"));
      QTimer::singleShot(2000, this, [this] {
        if (m_running)
          m_progressHint->setText(progressHint());
      });
      return;
    }
    e->accept();
  }
  void keyPressEvent(QKeyEvent* e) override {
    if (e->key() == Qt::Key_Escape)
      close();  // 运行中会被 closeEvent 拦下并提示
    QWidget::keyPressEvent(e);
  }
  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton)
      m_dragPos = e->globalPosition().toPoint();
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    if (!m_dragPos.isNull()) {
      move(pos() + e->globalPosition().toPoint() - m_dragPos);
      m_dragPos = e->globalPosition().toPoint();
    }
  }
  void mouseReleaseEvent(QMouseEvent*) override { m_dragPos = {}; }

 private:
  static QString progressHint() { return QStringLiteral(u"过程中请勿关闭本窗口"); }

  // 子部件淡入;播完移除效果,避免常驻离屏渲染
  void fadeInWidget(QWidget* w, int ms = 240) {
    auto* fx = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(fx);
    auto* a = new QPropertyAnimation(fx, "opacity", w);
    a->setDuration(ms);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    connect(a, &QPropertyAnimation::finished, this, [w] { w->setGraphicsEffect(nullptr); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
  }
  void switchPage(QFrame* next) {
    m_stack->setCurrentWidget(next);
    fadeInWidget(next);
  }
  // 阶段文案:淡出→换字→淡入
  void setActionText(const QString& text) {
    if (m_actionLabel->text() == text)
      return;
    auto* fx = new QGraphicsOpacityEffect(m_actionLabel);
    m_actionLabel->setGraphicsEffect(fx);
    auto* out = new QPropertyAnimation(fx, "opacity", m_actionLabel);
    out->setDuration(140);
    out->setStartValue(1.0);
    out->setEndValue(0.0);
    connect(out, &QPropertyAnimation::finished, this, [this, text, fx] {
      m_actionLabel->setText(text);
      auto* inAnim = new QPropertyAnimation(fx, "opacity", m_actionLabel);
      inAnim->setDuration(140);
      inAnim->setStartValue(0.0);
      inAnim->setEndValue(1.0);
      connect(inAnim, &QPropertyAnimation::finished, this,
              [this] { m_actionLabel->setGraphicsEffect(nullptr); });
      inAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    out->start(QAbstractAnimation::DeleteWhenStopped);
  }

  void startJob(MsiJob::Op op) {
    if (m_msiPath.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                           QStringLiteral(u"未找到 BangkeSetup-*.msi，请与安装程序放在同一目录。"));
      return;
    }
    m_job = MsiJob{op, m_msiPath, m_productCode, !m_productCode.isEmpty()};
    setActionText(QStringLiteral(u"正在准备…"));
    m_progressTitle->setText(op == MsiJob::Uninstall ? QStringLiteral(u"正在卸载 蚌壳拼音")
                                : op == MsiJob::Repair  ? QStringLiteral(u"正在修复 蚌壳拼音")
                                                        : QStringLiteral(u"正在安装 蚌壳拼音"));
    m_progressHint->setText(progressHint());
    m_running = true;
    switchPage(progressPage_);

    QFile::remove(MsiLogPath(m_msiPath));
    m_proc = new QProcess(this);
    m_proc->setProgram("msiexec.exe");
    m_proc->setArguments(BuildArgs(m_job));
    connect(m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus st) {
              m_exitCode = (st == QProcess::NormalExit) ? code : -1;
              msiFinished();
            });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
      m_exitCode = -1;
      msiFinished();
    });
    m_proc->start();
    startLogWatch();
  }

  // ---- 阶段进度：增量读 verbose log，按出现顺序取最后的标记 ----

  void startLogWatch() {
    m_logPos = 0;
    m_logFile.setFileName(MsiLogPath(m_msiPath));
    m_watcher = new QFileSystemWatcher(this);
    // log 文件由 msiexec 创建，先盯目录等它出现
    m_watcher->addPath(QFileInfo(m_logFile).absolutePath());
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { readLogDelta(); });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { readLogDelta(); });
    // msiexec 独占写时 watcher 事件可能吞掉，低频轮询兜底
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, [this] { readLogDelta(); });
    m_pollTimer->start(250);
  }

  void stopLogWatch() {
    if (m_pollTimer) { m_pollTimer->stop(); m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_watcher) { m_watcher->deleteLater(); m_watcher = nullptr; }
    if (m_logFile.isOpen())
      m_logFile.close();
  }

  void readLogDelta() {
    if (!m_logFile.isOpen()) {
      if (!m_logFile.exists())
        return;
      if (!m_logFile.open(QIODevice::ReadOnly))
        return;
      m_logPos = 0;
    }
    const qint64 size = m_logFile.size();
    if (size < m_logPos) {  // 文件被重建（重试场景）
      m_logFile.seek(0);
      m_logPos = 0;
    }
    if (m_logFile.pos() != m_logPos)
      m_logFile.seek(m_logPos);
    const QByteArray chunk = m_logFile.readAll();
    m_logPos = m_logFile.pos();
    if (chunk.isEmpty())
      return;

    static const struct { const char* a; const wchar_t* t; } steps[] = {
        {"StopServer", L"正在停止输入法服务"},
        {"ClearPendingDelete", L"正在准备文件"},
        {"InstallValidate", L"正在校验安装"},
        {"InstallFiles", L"正在复制文件"},
        {"WriteRegistryValues", L"正在写入注册表"},
        {"RegisterTSF", L"正在注册输入法"},
        {"FirstDeploy", L"正在部署输入方案"},
        {"StartServer", L"正在启动服务"},
        {"RemoveFiles", L"正在移除文件"},
        {"UnregisterTSF", L"正在注销输入法"},
        {"Cleanup", L"正在清理残留"},
    };
    int bestPos = -1;
    const char* bestKey = nullptr;
    const wchar_t* bestText = nullptr;
    for (const auto& s : steps) {
      const int p = chunk.lastIndexOf(s.a);
      if (p > bestPos) {
        bestPos = p;
        bestKey = s.a;
        bestText = s.t;
      }
    }
    if (bestKey) {
      setActionText(QString::fromWCharArray(bestText));
      // 按部署序列换算阶段进度,蚌壳随之张开(真实进度,非假百分比)
      static const char* kOrder[] = {"StopServer",  "ClearPendingDelete", "InstallValidate",
                                     "InstallFiles", "WriteRegistryValues", "RegisterTSF",
                                     "FirstDeploy",  "StartServer",         "RemoveFiles",
                                     "UnregisterTSF", "Cleanup"};
      const int N = sizeof(kOrder) / sizeof(kOrder[0]);
      for (int i = 0; i < N; ++i) {
        if (!strcmp(bestKey, kOrder[i])) {
          m_scene->setProgress(i / (qreal)(N - 1));
          break;
        }
      }
    }
  }

  void msiFinished() {
    stopLogWatch();
    m_running = false;
    const int r = m_exitCode;
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    switchPage(finishPage_);

    // 1641 = ERROR_SUCCESS_REBOOT_INITIATED、3010 = ERROR_SUCCESS_REBOOT_REQUIRED：
    // 都是"成功了，只是要重启才彻底生效"，不是失败。原先只认 0，会把它们显示成
    // "操作失败 (代码 0x00000669)"，用户看到的是装好了却报错。
    const bool rebootPending = (r == 1641 || r == 3010);
    const bool ok = (r == 0 || rebootPending);
    if (ok) {
      QString title, detail;
      switch (m_job.op) {
        case MsiJob::Uninstall:
          title = QStringLiteral(u"卸载完成");
          detail = QStringLiteral(u"蚌壳拼音已卸载（用户词库保留）。\n建议注销一次，输入指示器中的残留图标即会消失。");
          break;
        case MsiJob::Repair:
          title = QStringLiteral(u"修复完成");
          detail = QStringLiteral(u"蚌壳拼音已恢复就绪。");
          break;
        case MsiJob::Install:
          if (m_job.wasInstalled) {
            title = QStringLiteral(u"升级完成");
            detail = QStringLiteral(u"已更新到 %1，按 Win+空格 即可继续使用。").arg(m_pkgVersion);
          } else {
            title = QStringLiteral(u"安装完成");
            detail = QStringLiteral(u"蚌壳拼音已就绪，按 Win+空格 切换到蚌壳拼音开始使用。");
          }
          if (rebootPending) {
            // 输入法的 dll 被各个正在运行的程序加载着，替换不了的那一份要等本次登录
            // 结束才落位。直说，并给一个按钮让用户自己决定什么时候注销——不替他决定。
            detail += QStringLiteral(
                u"\n部分文件正被运行中的程序占用，注销一次即可完成更新。");
          }
          break;
      }
      m_finishTitle->setText(title);
      m_finishDetail->setText(detail);
    } else {
      m_finishTitle->setText(QStringLiteral(u"操作失败 (代码 0x%1)").arg((uint)r, 8, 16, QChar('0')));
      m_finishDetail->setText(QStringLiteral(u"详细日志：") +
                              QDir::toNativeSeparators(MsiLogPath(m_msiPath)));
    }
    m_check->start(ok);
    m_launchCheck->setVisible(ok && m_job.op != MsiJob::Uninstall);
    // 注销与否由用户点，不代他决定：装/卸载后有文件要等登录结束才落位时，把这个按钮
    // 摆出来并说明原因
    if (ok)
      m_logoffBtn->setText(rebootPending
                               ? QStringLiteral(u"立即注销（让更新生效）")
                               : QStringLiteral(u"立即注销（清除输入法残留）"));
    m_logoffBtn->setVisible(ok && (m_job.op == MsiJob::Uninstall || rebootPending));
    m_openLogBtn->setVisible(!ok && !m_msiPath.isEmpty());
  }

  void refreshState() {
    m_msiPath = FindMsi();
    m_pkgVersion = MsiPackageVersion(m_msiPath);
    m_productCode = InstalledProductCode();
    const bool installed = !m_productCode.isEmpty();
    const bool haveMsi = !m_msiPath.isEmpty();

    m_msiMissing->setVisible(!haveMsi);
    freshGroup_->setVisible(haveMsi && !installed);
    installedGroup_->setVisible(haveMsi && installed);

    if (haveMsi)
      m_pkgLabel->setText(QStringLiteral(u"安装包版本 %1").arg(
          m_pkgVersion.isEmpty() ? QStringLiteral(u"未知") : m_pkgVersion));

    if (installed) {
      const QString cur = InstalledVersion(m_productCode);
      m_installedLabel->setText(QStringLiteral(u"已安装版本 %1").arg(cur));
      // 升级与重装都走 /i：MajorUpgrade 自动按 UpgradeCode 接管旧版本
      m_primaryBtn->setText(ParseVersion(m_pkgVersion) > ParseVersion(cur)
                                ? QStringLiteral(u"升级到 %1").arg(m_pkgVersion)
                                : QStringLiteral(u"重新安装"));
    }
  }

  QPushButton* accentButton(const QString& text) {
    auto* b = new QPushButton(text);
    b->setObjectName("accent");
    b->setMinimumHeight(44);
    b->setCursor(Qt::PointingHandCursor);
    return b;
  }

  void buildWelcomePage() {
    welcomePage_ = new QFrame();
    welcomePage_->setObjectName("page");
    auto* box = new QVBoxLayout(welcomePage_);
    box->setContentsMargins(36, 8, 36, 28);
    box->setSpacing(12);

    auto* logo = new QLabel(QStringLiteral(u"蚌"), welcomePage_);
    logo->setObjectName("logo");
    logo->setAlignment(Qt::AlignCenter);
    box->addSpacing(10);
    box->addWidget(logo, 0, Qt::AlignHCenter);
    box->addWidget(new QLabel(QStringLiteral(u"蚌壳拼音输入法"), welcomePage_), 0, Qt::AlignHCenter);
    auto* sub = new QLabel(QStringLiteral(u"Rime 内核 · Windows 原生 TSF · 蚌壳出品"), welcomePage_);
    sub->setObjectName("sub");
    sub->setAlignment(Qt::AlignCenter);
    box->addWidget(sub, 0, Qt::AlignHCenter);
    m_pkgLabel = new QLabel(welcomePage_);
    m_pkgLabel->setObjectName("sub");
    m_pkgLabel->setAlignment(Qt::AlignCenter);
    box->addWidget(m_pkgLabel, 0, Qt::AlignHCenter);
    box->addSpacing(8);

    m_msiMissing = new QLabel(
        QStringLiteral(u"未找到 BangkeSetup-*.msi。\n请将本程序与安装包放在同一目录后重新运行。"), welcomePage_);
    m_msiMissing->setObjectName("warn");
    m_msiMissing->setAlignment(Qt::AlignCenter);
    m_msiMissing->setWordWrap(true);
    box->addWidget(m_msiMissing);

    freshGroup_ = new QFrame(welcomePage_);
    auto* fresh = new QVBoxLayout(freshGroup_);
    fresh->setContentsMargins(0, 0, 0, 0);
    fresh->setSpacing(10);
    auto* pathNote = new QLabel(QStringLiteral(u"将安装到 C:\\Program Files\\Bangke Pinyin"), freshGroup_);
    pathNote->setObjectName("sub");
    pathNote->setAlignment(Qt::AlignCenter);
    fresh->addWidget(pathNote);
    auto* installBtn = accentButton(QStringLiteral(u"立即安装"));
    connect(installBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    fresh->addWidget(installBtn);
    box->addWidget(freshGroup_);

    installedGroup_ = new QFrame(welcomePage_);
    auto* inst = new QVBoxLayout(installedGroup_);
    inst->setContentsMargins(0, 0, 0, 0);
    inst->setSpacing(10);
    m_installedLabel = new QLabel(installedGroup_);
    m_installedLabel->setAlignment(Qt::AlignCenter);
    inst->addWidget(m_installedLabel);
    m_primaryBtn = accentButton(QStringLiteral(u"重新安装"));
    connect(m_primaryBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    inst->addWidget(m_primaryBtn);
    auto* btnRow = new QHBoxLayout();
    auto* repairBtn = new QPushButton(QStringLiteral(u"修复安装"));
    auto* uninstallBtn = new QPushButton(QStringLiteral(u"卸载"));
    uninstallBtn->setObjectName("danger");
    for (auto* b : {repairBtn, uninstallBtn}) {
      b->setMinimumHeight(36);
      b->setCursor(Qt::PointingHandCursor);
    }
    connect(repairBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Repair); });
    connect(uninstallBtn, &QPushButton::clicked, this, [this] {
      if (QMessageBox::question(this, QStringLiteral(u"卸载"),
                                QStringLiteral(u"确定卸载蚌壳拼音吗？用户词库会保留。")) == QMessageBox::Yes)
        startJob(MsiJob::Uninstall);
    });
    btnRow->addWidget(repairBtn, 1);
    btnRow->addWidget(uninstallBtn, 1);
    inst->addLayout(btnRow);
    box->addWidget(installedGroup_);
    box->addStretch();

    m_stack->addWidget(welcomePage_);
  }

  void buildProgressPage() {
    progressPage_ = new QFrame();
    progressPage_->setObjectName("page");
    auto* box = new QVBoxLayout(progressPage_);
    box->setContentsMargins(48, 48, 48, 48);
    box->setSpacing(12);
    m_scene = new BeachScene(progressPage_);
    m_progressTitle = new QLabel(progressPage_);
    m_progressTitle->setAlignment(Qt::AlignCenter);
    m_actionLabel = new QLabel(QStringLiteral(u"正在准备…"), progressPage_);
    m_actionLabel->setAlignment(Qt::AlignCenter);
    m_bar = new BusyBar(progressPage_);
    m_progressHint = new QLabel(progressPage_);
    m_progressHint->setObjectName("sub");
    m_progressHint->setAlignment(Qt::AlignCenter);
    box->addStretch();
    box->addWidget(m_scene, 0, Qt::AlignHCenter);
    box->addSpacing(10);
    box->addWidget(m_progressTitle);
    box->addWidget(m_actionLabel);
    box->addWidget(m_bar);
    box->addSpacing(4);
    box->addWidget(m_progressHint);
    box->addStretch();
    m_stack->addWidget(progressPage_);
  }

  void buildFinishPage() {
    finishPage_ = new QFrame();
    finishPage_->setObjectName("page");
    auto* box = new QVBoxLayout(finishPage_);
    box->setContentsMargins(48, 44, 48, 32);
    box->setSpacing(12);
    m_check = new CheckMark(finishPage_);
    m_finishTitle = new QLabel(finishPage_);
    m_finishTitle->setObjectName("finishTitle");
    m_finishTitle->setAlignment(Qt::AlignCenter);
    m_finishDetail = new QLabel(finishPage_);
    m_finishDetail->setObjectName("sub");
    m_finishDetail->setAlignment(Qt::AlignCenter);
    m_finishDetail->setWordWrap(true);
    m_launchCheck = new QCheckBox(QStringLiteral(u"立即启动输入法服务"), finishPage_);
    m_launchCheck->setChecked(true);

    m_logoffBtn = new QPushButton(QStringLiteral(u"立即注销（清除输入法残留）"));
    m_logoffBtn->setMinimumHeight(36);
    m_logoffBtn->setVisible(false);
    connect(m_logoffBtn, &QPushButton::clicked, this, [] {
      QProcess::startDetached(QStringLiteral("shutdown"), {QStringLiteral("/l")});
    });

    m_openLogBtn = new QPushButton(QStringLiteral(u"打开日志文件"));
    m_openLogBtn->setMinimumHeight(36);
    m_openLogBtn->setVisible(false);
    connect(m_openLogBtn, &QPushButton::clicked, this, [this] {
      const QString log = QDir::toNativeSeparators(MsiLogPath(m_msiPath));
      QProcess::startDetached(QStringLiteral("explorer"),
                              {QStringLiteral("/select,"), log});
    });

    auto* doneBtn = accentButton(QStringLiteral(u"完成"));
    connect(doneBtn, &QPushButton::clicked, this, [this] {
      if (m_launchCheck->isChecked() && m_launchCheck->isVisible()) {
        // explorer 拉起 = 用户会话、非提升，避免服务继承管理员上下文
        QProcess::startDetached(
            QStringLiteral("explorer"),
            {QDir::toNativeSeparators(QStringLiteral(u"C:\\Program Files\\Bangke Pinyin\\BangkeServer.exe"))});
      }
      close();
    });
    box->addStretch();
    box->addWidget(m_check, 0, Qt::AlignHCenter);
    box->addSpacing(4);
    box->addWidget(m_finishTitle);
    box->addWidget(m_finishDetail);
    box->addWidget(m_launchCheck, 0, Qt::AlignHCenter);
    box->addSpacing(8);
    box->addWidget(m_logoffBtn);
    box->addWidget(m_openLogBtn);
    box->addSpacing(8);
    box->addWidget(doneBtn);
    box->addStretch();
    m_stack->addWidget(finishPage_);
  }

  QString m_msiPath, m_productCode, m_pkgVersion;
  QStackedWidget* m_stack = nullptr;
  QFrame *welcomePage_ = nullptr, *progressPage_ = nullptr, *finishPage_ = nullptr;
  QFrame *freshGroup_ = nullptr, *installedGroup_ = nullptr;
  QLabel *m_installedLabel = nullptr, *m_pkgLabel = nullptr, *m_msiMissing = nullptr,
         *m_progressTitle = nullptr, *m_actionLabel = nullptr, *m_progressHint = nullptr,
         *m_finishTitle = nullptr, *m_finishDetail = nullptr;
  BusyBar* m_bar = nullptr;
  BeachScene* m_scene = nullptr;
  CheckMark* m_check = nullptr;
  bool m_entered = false;
  QCheckBox* m_launchCheck = nullptr;
  QPushButton* m_primaryBtn = nullptr;
  QPushButton *m_logoffBtn = nullptr, *m_openLogBtn = nullptr;
  CloseButton* m_closeBtn = nullptr;
  MsiJob m_job;
  bool m_running = false;
  QProcess* m_proc = nullptr;
  QTimer* m_pollTimer = nullptr;
  QFileSystemWatcher* m_watcher = nullptr;
  QFile m_logFile;
  qint64 m_logPos = 0;
  int m_exitCode = -1;
  QPoint m_dragPos;
};

// 启动诊断:%TEMP%\bkinstaller.log(引导壳拉起失败定位用)
static void BootLog(const char* msg) {
  wchar_t tmp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring path = std::wstring(tmp) + L"bkinstaller.log";
  HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return;
  SetFilePointer(f, 0, NULL, FILE_END);
  char line[160];
  snprintf(line, sizeof(line), "%lu %s\r\n", GetTickCount(), msg);
  DWORD w = 0;
  WriteFile(f, line, (DWORD)strlen(line), &w, NULL);
  CloseHandle(f);
}

int main(int argc, char* argv[]) {
  BootLog("main enter");
  QApplication app(argc, argv);
  BootLog("QApplication up");
  QApplication::setApplicationName(QStringLiteral(u"蚌壳拼音安装器"));

  app.setStyleSheet(QStringLiteral(uR"(
    QWidget { background: transparent; color: #e8ecf1; font-family: "Microsoft YaHei UI"; font-size: 14px; }
    #panel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #131b26, stop:1 #0d141d);
             border-radius: 14px; border: 1px solid #243244; }
    #title { font-size: 15px; color: #9fb6cd; }
    #logo { min-width: 92px; min-height: 92px; max-width: 92px; max-height: 92px;
            background: qradialgradient(cx:0.5, cy:0.35, radius:1.1, stop:0 #35618f, stop:1 #16233a);
            border-radius: 24px; color: #eaf2fb; font-size: 52px; font-weight: 600;
            border: 1px solid #3d5a82; }
    #sub { color: #7e93a8; font-size: 12px; }
    #warn { color: #e0a75e; font-size: 13px; }
    QPushButton { background: #1b2736; border: 1px solid #2c3d52; border-radius: 8px; color: #dfe7ef; padding: 0 18px; }
    QPushButton:hover { border-color: #4a9df0; color: #ffffff; }
    QPushButton:disabled { color: #55637a; }
    #accent { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2f7cd6, stop:1 #4aa8f0);
              border: none; color: white; font-size: 15px; font-weight: 600; border-radius: 8px; }
    #accent:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #3a8ae8, stop:1 #5cb5f7); }
    #danger { color: #e08080; }
    #danger:hover { border-color: #e05656; color: #ff9c9c; }
    QProgressBar { background: #0b1119; border: none; border-radius: 3px; }
    QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2f7cd6, stop:1 #4aa8f0); border-radius: 3px; }
    QCheckBox { color: #9fb6cd; }
    #finishTitle { font-size: 22px; font-weight: 600; color: #eaf2fb; }
  )"));

  MainWindow w;
  w.show();
  w.raise();
  w.activateWindow();  // 提权链路不自动给前台,需自取
  BootLog("window shown, entering exec");
  const int ret = app.exec();
  BootLog("exec returned");
  return ret;
}

#include "main.moc"
