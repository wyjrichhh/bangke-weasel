// 蚌壳拼音安装器前端：UI 由本程序承担，MSI 以 msiexec /qn 子进程执行。
// 子进程隔离保证 UI 崩溃不影响安装事务（2026-09-06 一次 UI 层 UAF 崩溃验证了这一点）。
// 阶段进度来自对 verbose log 的增量读取，不做假百分比。
#include <QApplication>
#include <QPalette>
#include <QTextBrowser>
#include <QDialog>
#include <QScreen>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QEnterEvent>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
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
#include <QFont>
#include <QFontDatabase>
#include <cmath>
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include "InstallPolicy.h"
#include <cstring>

static const wchar_t* kUpgradeCode = L"{8F1D4B33-9C2A-4E6D-B0F7-3A5C8E21D940}";

static QString MsiPackageProperty(const QString& path, const QString& property = QStringLiteral("ProductVersion"));
static std::optional<bangke::installer::Version> ParseVersion(const QString& v);

static bool InstallerUsesDarkTheme() {
  DWORD light = 1, size = sizeof(light);
  RegGetValueW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
  return light == 0;
}

static void ApplyInstallerTheme(bool dark) {
  const QString ground = dark ? "#11151A" : "#F7F5F0";
  const QString surface = dark ? "#20262E" : "#FFFFFF";
  const QString field = dark ? "#171C23" : "#F0EDE6";
  const QString ink = dark ? "#EDF1F5" : "#302C26";
  const QString muted = dark ? "#ADB8C7" : "#6B6257";
  const QString accent = dark ? "#94C8BC" : "#785330";
  const QString soft = dark ? "#2B403E" : "#EEE2D3";
  const QString line = dark ? "#343D48" : "#DCD5CB";
  const QString on = dark ? "#102722" : "#FFFFFF";
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(ground));
  palette.setColor(QPalette::WindowText, QColor(ink));
  palette.setColor(QPalette::Text, QColor(ink));
  palette.setColor(QPalette::Base, QColor(field));
  palette.setColor(QPalette::Button, QColor(surface));
  palette.setColor(QPalette::ButtonText, QColor(ink));
  palette.setColor(QPalette::Highlight, QColor(accent));
  palette.setColor(QPalette::HighlightedText, QColor(on));
  palette.setColor(QPalette::Mid, QColor(line));
  palette.setColor(QPalette::PlaceholderText, QColor(muted));
  qApp->setPalette(palette);
  QString css = QStringLiteral(R"(
    QWidget { background: transparent; color: @ink; font-family: "Microsoft YaHei UI"; font-size: 13px; }
    #panel { background: @ground; border: 1px solid @line; border-radius: 14px; }
    #title, #sub { color: @muted; font-size: 12px; }
    #brandRail { border-right: 1px solid @line; background: qradialgradient(cx:1, cy:0, radius:1.2, stop:0 @soft, stop:1 @field); }
    #brandName { font-size: 20px; font-weight: 600; }
    #brandHeadline { font-size: 28px; font-weight: 600; }
    #brandPreview { background: @ground; border: 1px solid @line; border-radius: 10px; }
    #selectedCandidate { background: @accent; color: @on; border-radius: 6px; padding: 6px; }
    #heroTitle, #finishTitle { font-size: 25px; font-weight: 600; }
    #facts { background: @surface; border: 1px solid @line; border-radius: 10px; }
    #warn { color: @accent; }
    QPushButton { background: @field; border: 1px solid @line; border-radius: 8px; padding: 0 18px; min-height: 40px; }
    QPushButton:hover, QPushButton:focus { border-color: @accent; background: @soft; }
    QPushButton:pressed { background: @surface; }
    QPushButton:disabled { color: @muted; }
    #accent { background: @accent; color: @on; border: 1px solid @accent; font-size: 14px; font-weight: 600; }
    #accent:hover { background: @accent; border: 2px solid @on; }
    #accent:disabled { background: @soft; color: @muted; border-color: @line; }
    #danger { color: @muted; }
    #danger:hover { color: @ink; }
    #textLink { background: transparent; border: none; padding: 0 4px; min-height: 24px; color: @muted; font-size: 12px; }
    #textLink:hover { color: @accent; }
    QCheckBox { color: @muted; spacing: 8px; }
    QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid @muted; background: @field; }
    QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
    QMessageBox, QDialog { background: @ground; }
    QTextBrowser { background: @field; border: 1px solid @line; border-radius: 8px; padding: 12px; }
  )");
  for (const auto& entry : {qMakePair(QString("@ground"), ground), qMakePair(QString("@surface"), surface),
      qMakePair(QString("@field"), field), qMakePair(QString("@ink"), ink),
      qMakePair(QString("@muted"), muted), qMakePair(QString("@accent"), accent),
      qMakePair(QString("@soft"), soft), qMakePair(QString("@line"), line), qMakePair(QString("@on"), on)})
    css.replace(entry.first, entry.second);
  qApp->setStyleSheet(css);
}

static QString FindMsi() {
  // 引导壳会把本次解包的 MSI 文件名作为第一个参数传入 —— 参数是
  // 唯一真相。兜底(直接运行安装器)按 ProductVersion 数值挑最大:
  // 文件名字典序会把 0.4.9 排在 0.4.13 后面,必挑错
  const auto args = QCoreApplication::arguments();
  if (args.size() > 1) {
    const QFileInfo byArg(args.at(1));
    if (byArg.exists() && byArg.suffix().compare(QLatin1String("msi"),
                                                 Qt::CaseInsensitive) == 0)
      return byArg.absoluteFilePath();
  }
  QDir dir(QApplication::applicationDirPath());
  const auto msis = dir.entryList({QStringLiteral("BangkeSetup-*.msi")},
                                  QDir::Files);
  QString best;
  std::optional<bangke::installer::Version> bestVer;
  for (const auto& name : msis) {
    const QString path = dir.absoluteFilePath(name);
    const auto ver = ParseVersion(MsiPackageProperty(path));
    if (ver > bestVer) {
      bestVer = ver;
      best = path;
    }
  }
  return best;
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
static QString MsiPackageProperty(const QString& path, const QString& property) {
  MSIHANDLE db = 0;
  if (MsiOpenDatabaseW(path.toStdWString().c_str(), MSIDBOPEN_READONLY, &db) != ERROR_SUCCESS)
    return QString();
  QString result;
  MSIHANDLE view = 0;
  const auto query = QStringLiteral("SELECT `Value` FROM `Property` WHERE `Property` = '%1'").arg(property).toStdWString();
  if (MsiDatabaseOpenViewW(db, query.c_str(),
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

static std::optional<bangke::installer::Version> ParseVersion(const QString& v) {
  return bangke::installer::ParseVersion(v.toStdString());
}

static QString InstalledRoot() {
  QSettings registry(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Bangke"), QSettings::NativeFormat);
  return registry.value(QStringLiteral("BangkeRoot"),
      QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("Bangke Pinyin"))).toString();
}

// ---------------- MSI 执行 ----------------

struct MsiJob {
  enum Op { Install, Repair, Uninstall } op;
  QString msiPath, productCode;
  bool wasInstalled = false;  // 操作开始前产品是否已装（区分安装/升级文案）

};

static QString MsiLogPath(const QString& msi) {
  // The bootstrap removes its payload directory on exit; diagnostics must survive.
  const QString directory = QDir::tempPath() + QStringLiteral("/BangkeSetupLogs");
  QDir().mkpath(directory);
  return directory + QLatin1Char('/') + QFileInfo(msi).completeBaseName() +
      QStringLiteral("-%1.log").arg(QCoreApplication::applicationPid());
}

static QStringList BuildArgs(const MsiJob& job) {
  const QString log = QDir::toNativeSeparators(MsiLogPath(job.msiPath));
  // Uninstall uses the installed ProductCode, even if the payload is newer.
  // Repair is selected only for the same ProductCode and recaches this MSI;
  // a different payload goes through /i and the normal MajorUpgrade chain.
  const QString installed = job.productCode.isEmpty()
                                ? QDir::toNativeSeparators(job.msiPath)
                                : job.productCode;
  QStringList args;
  switch (job.op) {
    case MsiJob::Install:
      args << "/i" << QDir::toNativeSeparators(job.msiPath);
      break;
    case MsiJob::Repair:
      // Recache from this payload: the previous bootstrap directory is gone.
      args << "/fvomus" << QDir::toNativeSeparators(job.msiPath);
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
    m_animation = a;
  }

 protected:
  void showEvent(QShowEvent* event) override { QWidget::showEvent(event); m_animation->start(); }
  void hideEvent(QHideEvent* event) override { m_animation->stop(); QWidget::hideEvent(event); }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Mid));
    p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
    if (m_value <= 0.0)
      return;
    const QRectF done(0, 0, width() * m_value, height());
    p.setBrush(palette().color(QPalette::Highlight));
    p.drawRoundedRect(done, height() / 2.0, height() / 2.0);
    // 扫光只叠在已完成的那段上:部署阶段之间进度会停(FirstDeploy 可能几分钟),
    // 完全静止看着像卡死,给它一点持续的动作
    p.setClipRect(done);
    const qreal chunk = width() * 0.36;
    const qreal x = -chunk + (width() + chunk * 2) * m_phase;
    QLinearGradient g(x, 0, x + chunk, 0);
    g.setColorAt(0.0, QColor(255, 255, 255, 0));
    g.setColorAt(0.5, QColor(255, 255, 255, 110));
    g.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(g);
    p.drawRect(done);
  }

 public:
  void setValue(qreal v) {
    v = qBound(0.0, v, 1.0);
    if (m_value == v)
      return;
    m_value = v;
    update();
  }

 private:
  QVariantAnimation* m_animation = nullptr;
  qreal m_phase = 0.0;
  qreal m_value = 0.0;
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
    const QColor c = m_ok ? palette().color(QPalette::Highlight) : QColor(0xE0, 0x56, 0x56);
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

// 几何蚌壳：细线扇骨与珍珠，静态绘制，仅阶段变化时重绘。
class ShellMark : public QWidget {
 public:
  explicit ShellMark(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedSize(360, 152);
    setAttribute(Qt::WA_TransparentForMouseEvents);
  }
  void setProgress(qreal p) { m_progress = qBound(0.0, p, 1.0); update(); }
 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, 106);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    for (int x = 12; x < width(); x += 24)
      for (int y = 12; y < height(); y += 24) p.drawPoint(x, y);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.drawEllipse(QPointF(width() / 2.0, 75), 118, 64);
    p.setPen(QPen(palette().color(QPalette::Highlight), 1.3));
    for (int i = 0; i <= 12; ++i) {
      const qreal angle = 3.141592653589793 * i / 12;
      const QPointF edge(center.x() + 82 * std::cos(angle),
                         center.y() - 72 * std::sin(angle));
      QPainterPath rib;
      rib.moveTo(center);
      rib.quadTo(QPointF((center.x() + edge.x()) / 2,
                         center.y() - 18 - 34 * std::sin(angle)), edge);
      p.drawPath(rib);
    }
    p.drawArc(QRectF(center.x() - 82, center.y() - 72, 164, 144), 0, 180 * 16);
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Highlight));
    const qreal radius = 3 + m_progress * 3;
    p.drawEllipse(QPointF(center.x(), center.y() - 28), radius, radius);
  }
 private:
  qreal m_progress = 0;
};

// 自绘关闭按钮，不依赖字体中的特殊符号。
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
    // 悬停时显示关闭按钮背景。
    const bool hot = underMouse() || isDown();
    p.setBrush(hot ? QColor(0xE0, 0x56, 0x56) : Qt::transparent);
    p.drawRoundedRect(rect(), 8, 8);
    QPen pen(hot ? QColor(0xFF, 0xFF, 0xFF) : palette().color(QPalette::PlaceholderText), 1.7);
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
    resize(996, 634);
    setMinimumSize(936, 614);
    setWindowTitle(QStringLiteral(u"蚌壳拼音 · 安装"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QFrame(this);
    panel->setObjectName("panel");
    // 投影边距独立于内容区，避免边缘被裁切。
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(44);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 24, 26, 60));
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
    title->setContentsMargins(28, 18, 18, 12);
    title->addWidget(titleText);
    title->addStretch();
    title->addWidget(m_closeBtn);
    box->addLayout(title);

    auto* content = new QHBoxLayout();
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    auto* rail = new QFrame(panel);
    rail->setObjectName("brandRail");
    rail->setFixedWidth(318);
    auto* brand = new QVBoxLayout(rail);
    brand->setContentsMargins(30, 32, 30, 30);
    brand->setSpacing(12);
    auto* name = new QLabel(QStringLiteral(u"蚌壳拼音"), rail);
    name->setObjectName("brandName");
    name->setAlignment(Qt::AlignCenter);
    auto* logo = new QLabel(rail);
    logo->setPixmap(QPixmap(QStringLiteral(":/installer/bangke-appicon.png")).scaled(
        40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brand->addWidget(logo, 0, Qt::AlignHCenter);
    brand->addWidget(name);
    brand->addSpacing(28);
    auto* headline = new QLabel(QStringLiteral(u"让想法，\n自然成文。"), rail);
    headline->setObjectName("brandHeadline");
    headline->setAlignment(Qt::AlignCenter);
    brand->addWidget(headline);
    auto* copy = new QLabel(QStringLiteral(u"熟悉的拼音输入\n在本机运行的 AI 联想"), rail);
    copy->setObjectName("sub");
    copy->setAlignment(Qt::AlignCenter);
    brand->addWidget(copy);
    brand->addSpacing(20);
    auto* sample = new QFrame(rail);
    sample->setObjectName("brandPreview");
    auto* preview = new QVBoxLayout(sample);
    preview->setContentsMargins(10, 12, 10, 12);
    auto* typed = new QLabel(QStringLiteral(u"今天，bangke"), sample);
    typed->setAlignment(Qt::AlignCenter);
    preview->addWidget(typed);
    auto* candidates = new QHBoxLayout();
    auto* selected = new QLabel(QStringLiteral(u"1. 蚌壳"), sample);
    selected->setObjectName("selectedCandidate");
    selected->setAlignment(Qt::AlignCenter);
    auto* other = new QLabel(QStringLiteral(u"2. 帮客"), sample);
    other->setAlignment(Qt::AlignCenter);
    candidates->addWidget(selected);
    candidates->addWidget(other);
    preview->addLayout(candidates);
    brand->addWidget(sample);
    brand->addStretch();
    auto* foot = new QLabel(QStringLiteral(u"本地 AI 联想 · Windows 64 位"), rail);
    foot->setObjectName("sub");
    foot->setAlignment(Qt::AlignCenter);
    brand->addWidget(foot);
    content->addWidget(rail);
    m_stack = new QStackedWidget(panel);
    content->addWidget(m_stack, 1);
    box->addLayout(content, 1);
    buildWelcomePage();
    buildProgressPage();
    buildFinishPage();

    refreshState();
    auto* themeTimer = new QTimer(this);
    bool dark = InstallerUsesDarkTheme();
    connect(themeTimer, &QTimer::timeout, this, [dark]() mutable {
      const bool next = InstallerUsesDarkTheme();
      if (next != dark) {
        dark = next;
        ApplyInstallerTheme(dark);
      }
    });
    themeTimer->start(1000);
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
  // 阶段文案直接更新，避免连续日志事件替换动画对象。
  void setActionText(const QString& text) {
    if (m_actionLabel->text() == text)
      return;
    m_actionLabel->setText(text);
  }

  void startJob(MsiJob::Op op) {
    if (m_msiPath.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                           QStringLiteral(u"未找到 BangkeSetup-*.msi，请与安装程序放在同一目录。"));
      return;
    }
    m_job = MsiJob{op, m_msiPath, m_productCode, !m_productCode.isEmpty()};
    m_bar->setValue(0);
    m_scene->setProgress(0);
    m_stage = 0;
    setActionText(QStringLiteral(u"正在准备…"));
    m_progressTitle->setText(op == MsiJob::Uninstall ? QStringLiteral(u"正在卸载 蚌壳拼音")
                                : op == MsiJob::Repair  ? QStringLiteral(u"正在修复 蚌壳拼音")
                                                        : QStringLiteral(u"正在安装 蚌壳拼音"));
    m_progressHint->setText(progressHint());
    m_running = true;
    m_closeBtn->setEnabled(false);
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
    m_logPending.clear();
    m_logEncodingKnown = false;
    m_logUtf16 = false;
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

    m_logPending += chunk;
    if (!m_logEncodingKnown && m_logPending.size() >= 2) {
      m_logUtf16 = m_logPending.startsWith(QByteArray::fromHex("fffe")) || m_logPending.at(1) == '\0';
      m_logEncodingKnown = true;
    }
    const QByteArray newline = m_logUtf16 ? QByteArray::fromHex("0a00") : QByteArray("\n");
    int end = 0;
    while ((end = m_logPending.indexOf(newline)) >= 0) {
      const QByteArray line = m_logPending.left(end);
      m_logPending.remove(0, end + newline.size());
      QString text;
      if (m_logUtf16) {
        for (int i = 0; i + 1 < line.size(); i += 2)
          text += QChar(uchar(line[i]) | (ushort(uchar(line[i + 1])) << 8));
      } else text = QString::fromLocal8Bit(line);
      const std::string actionLine = text.toStdString();
      const auto action = bangke::installer::ActionName(actionLine);
      if (!action) continue;
      const QString key = QString::fromUtf8(action->data(), int(action->size()));
      const QStringList order = m_job.op == MsiJob::Uninstall
          ? QStringList{"InstallValidate", "UnregisterTSF", "Cleanup", "RemoveFiles", "RemoveRegistryValues", "InstallFinalize"}
          : QStringList{"InstallValidate", "InstallFiles", "ClearPendingDelete", "RegisterTSF", "WriteRegistryValues", "FirstDeploy", "StartServer", "InstallFinalize"};
      const QStringList labels = m_job.op == MsiJob::Uninstall
          ? QStringList{QStringLiteral(u"校验安装"), QStringLiteral(u"注销输入法"), QStringLiteral(u"清理残留"), QStringLiteral(u"移除文件"), QStringLiteral(u"清理注册表"), QStringLiteral(u"完成卸载")}
          : QStringList{QStringLiteral(u"校验安装"), QStringLiteral(u"复制文件"), QStringLiteral(u"准备注册"), QStringLiteral(u"注册输入法"), QStringLiteral(u"写入配置"), QStringLiteral(u"部署输入方案（首次可能需要几分钟）"), QStringLiteral(u"启动服务"), QStringLiteral(u"完成安装")};
      const auto accepted = bangke::installer::AcceptStage(m_stage, key.toStdString(), m_job.op == MsiJob::Uninstall);
      if (!accepted) continue;
      const int stage = *accepted;
      m_stage = stage;
      setActionText(labels.at(stage));
      // Only completed stages count; 100% is reserved for a successful exit.
      const qreal progress = stage / qreal(order.size());
      m_scene->setProgress(progress);
      m_bar->setValue(progress);
    }
    if (m_logPending.size() > 65536) m_logPending.clear();
  }

  void msiFinished() {
    if (!m_running) return;
    stopLogWatch();
    m_running = false;
    m_closeBtn->setEnabled(true);
    const int r = m_exitCode;
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    switchPage(finishPage_);

    // 1641 = ERROR_SUCCESS_REBOOT_INITIATED、3010 = ERROR_SUCCESS_REBOOT_REQUIRED：
    // 都是"成功了，只是要重启才彻底生效"，不是失败。原先只认 0，会把它们显示成
    // "操作失败 (代码 0x00000669)"，用户看到的是装好了却报错。
    const bool rebootPending = bangke::installer::RequiresReboot(r);
    const bool ok = bangke::installer::Succeeded(r);
    if (ok) {
      m_bar->setValue(1);
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
                u"\n部分文件需要重启 Windows 才能完成替换，请保存工作后重启。");
          }
          break;
      }
      m_finishTitle->setText(title);
      m_finishDetail->setText(detail);
    } else {
      m_finishTitle->setText(QStringLiteral(u"未能完成操作（0x%1）").arg((uint)r, 8, 16, QChar('0')));
      m_finishDetail->setText(QStringLiteral(u"请查看日志了解原因，再重新运行安装程序。\n") +
                              QDir::toNativeSeparators(MsiLogPath(m_msiPath)));
    }
    m_check->start(ok);
    m_settingsCheck->setVisible(ok && m_job.op != MsiJob::Uninstall);
    // 注销与否由用户点，不代他决定：装/卸载后有文件要等登录结束才落位时，把这个按钮
    // 摆出来并说明原因
    if (ok)
      m_logoffBtn->setText(rebootPending
                               ? QStringLiteral(u"现在重启…")
                               : QStringLiteral(u"注销以完成卸载…"));
    m_logoffBtn->setVisible(ok && (m_job.op == MsiJob::Uninstall || rebootPending));
    m_openLogBtn->setVisible(!ok && !m_msiPath.isEmpty());
  }

  void refreshState() {
    m_msiPath = FindMsi();
    m_pkgVersion = MsiPackageProperty(m_msiPath);
    m_productCode = InstalledProductCode();
    const bool installed = !m_productCode.isEmpty();
    const bool haveMsi = !m_msiPath.isEmpty();

    m_msiMissing->setVisible(!haveMsi);
    freshGroup_->setVisible(haveMsi && !installed);
    installedGroup_->setVisible(haveMsi && installed);

    if (haveMsi)
      m_pkgLabel->setText(QStringLiteral(u"安装版本  %1  ·  Windows 64 位").arg(
          m_pkgVersion.isEmpty() ? QStringLiteral(u"未知") : m_pkgVersion));

    if (installed) {
      const QString cur = InstalledVersion(m_productCode);
      m_installedLabel->setText(QStringLiteral(u"已安装版本 %1").arg(cur));
      const auto comparison = ParseVersion(m_pkgVersion);
      const bool canInstall = comparison && ParseVersion(cur) && comparison >= ParseVersion(cur);
      m_primaryBtn->setEnabled(canInstall);
      m_repairBtn->setEnabled(canInstall);
      m_primaryRepair = comparison == ParseVersion(cur) &&
          MsiPackageProperty(m_msiPath, QStringLiteral("ProductCode")).compare(m_productCode, Qt::CaseInsensitive) == 0;
      m_repairBtn->setVisible(!m_primaryRepair);
      if (comparison > ParseVersion(cur))
        m_primaryBtn->setText(QStringLiteral(u"更新至 %1").arg(m_pkgVersion));
      else if (m_primaryRepair)
        m_primaryBtn->setText(QStringLiteral(u"修复安装"));
      else if (comparison == ParseVersion(cur))
        m_primaryBtn->setText(QStringLiteral(u"重新安装"));
      else
        m_primaryBtn->setText(QStringLiteral(u"已安装更新版本"));
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
    box->setContentsMargins(36, 8, 36, 24);
    box->setSpacing(12);

    box->addSpacing(20);
    auto* heading = new QLabel(QStringLiteral(u"安装蚌壳拼音"), welcomePage_);
    heading->setObjectName("heroTitle");
    heading->setAlignment(Qt::AlignCenter);
    box->addWidget(heading);
    auto* sub = new QLabel(QStringLiteral(u"拼音输入与本地 AI 联想，一次装好。"), welcomePage_);
    sub->setObjectName("sub");
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    box->addWidget(sub);
    box->addSpacing(18);
    m_pkgLabel = new QLabel(welcomePage_);
    m_pkgLabel->setObjectName("facts");
    m_pkgLabel->setMinimumHeight(56);
    m_pkgLabel->setAlignment(Qt::AlignCenter);
    box->addWidget(m_pkgLabel);
    auto* privacy = new QLabel(QStringLiteral(u"内置输入方案与本地模型，无需另行下载。"), welcomePage_);
    privacy->setObjectName("sub");
    privacy->setAlignment(Qt::AlignCenter);
    privacy->setWordWrap(true);
    box->addWidget(privacy);
    box->addStretch();

    m_msiMissing = new QLabel(
        QStringLiteral(u"安装文件不完整。\n请重新下载完整安装包后再试。"), welcomePage_);
    m_msiMissing->setObjectName("warn");
    m_msiMissing->setAlignment(Qt::AlignCenter);
    m_msiMissing->setWordWrap(true);
    box->addWidget(m_msiMissing);

    freshGroup_ = new QFrame(welcomePage_);
    freshGroup_->setObjectName("freshInstall");
    auto* fresh = new QVBoxLayout(freshGroup_);
    fresh->setContentsMargins(0, 0, 0, 0);
    fresh->setSpacing(10);
    auto* pathNote = new QLabel(QStringLiteral(u"安装位置\n%1").arg(QDir::toNativeSeparators(InstalledRoot())), freshGroup_);
    pathNote->setObjectName("sub");
    pathNote->setAlignment(Qt::AlignCenter);
    pathNote->setWordWrap(true);
    pathNote->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fresh->addWidget(pathNote);
    auto* installBtn = accentButton(QStringLiteral(u"开始安装"));
    connect(installBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    fresh->addWidget(installBtn);
    box->addWidget(freshGroup_);

    installedGroup_ = new QFrame(welcomePage_);
    installedGroup_->setObjectName("installedActions");
    auto* inst = new QVBoxLayout(installedGroup_);
    inst->setContentsMargins(0, 0, 0, 0);
    inst->setSpacing(10);
    m_installedLabel = new QLabel(installedGroup_);
    m_installedLabel->setAlignment(Qt::AlignCenter);
    inst->addWidget(m_installedLabel);
    m_primaryBtn = accentButton(QStringLiteral(u"重新安装"));
    connect(m_primaryBtn, &QPushButton::clicked, this, [this] {
      startJob(m_primaryRepair ? MsiJob::Repair : MsiJob::Install);
    });
    inst->addWidget(m_primaryBtn);
    auto* btnRow = new QHBoxLayout();
    auto* repairBtn = new QPushButton(QStringLiteral(u"使用此安装包修复"));
    m_repairBtn = repairBtn;
    auto* uninstallBtn = new QPushButton(QStringLiteral(u"卸载"));
    uninstallBtn->setObjectName("danger");
    for (auto* b : {repairBtn, uninstallBtn}) {
      b->setMinimumHeight(36);
      b->setCursor(Qt::PointingHandCursor);
    }
    connect(repairBtn, &QPushButton::clicked, this, [this] {
      const bool sameProduct = MsiPackageProperty(m_msiPath, QStringLiteral("ProductCode"))
          .compare(m_productCode, Qt::CaseInsensitive) == 0;
      startJob(sameProduct ? MsiJob::Repair : MsiJob::Install);
    });
    connect(uninstallBtn, &QPushButton::clicked, this, [this] {
      if (QMessageBox::question(this, QStringLiteral(u"卸载"),
                                QStringLiteral(u"确定卸载蚌壳拼音吗？用户词库会保留。")) == QMessageBox::Yes)
        startJob(MsiJob::Uninstall);
    });
    btnRow->addWidget(repairBtn, 1);
    btnRow->addWidget(uninstallBtn, 1);
    inst->addLayout(btnRow);
    box->addWidget(installedGroup_);
    auto* license = new QPushButton(QStringLiteral(u"开源许可"), welcomePage_);
    license->setObjectName("textLink");
    connect(license, &QPushButton::clicked, this, [this] {
      QDialog dialog(this);
      dialog.setWindowTitle(QStringLiteral(u"开源许可"));
      dialog.resize(620, 440);
      auto* layout = new QVBoxLayout(&dialog);
      auto* text = new QTextBrowser(&dialog);
      QFile file(QStringLiteral(":/installer/LICENSE.txt"));
      if (file.open(QIODevice::ReadOnly))
        text->setPlainText(QString::fromUtf8(file.readAll()));
      else
        text->setPlainText(QStringLiteral(u"无法读取许可文件，请访问项目网站查看。"));
      layout->addWidget(text);
      auto* done = new QPushButton(QStringLiteral(u"关闭"), &dialog);
      connect(done, &QPushButton::clicked, &dialog, &QDialog::accept);
      layout->addWidget(done, 0, Qt::AlignHCenter);
      dialog.exec();
    });
    box->addWidget(license, 0, Qt::AlignHCenter);

    m_stack->addWidget(welcomePage_);
  }

  void buildProgressPage() {
    progressPage_ = new QFrame();
    progressPage_->setObjectName("page");
    auto* box = new QVBoxLayout(progressPage_);
    box->setContentsMargins(48, 48, 48, 48);
    box->setSpacing(12);
    m_scene = new ShellMark(progressPage_);
    m_progressTitle = new QLabel(progressPage_);
    m_progressTitle->setAlignment(Qt::AlignCenter);
    m_progressTitle->setObjectName("finishTitle");
    m_progressTitle->setWordWrap(true);
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
    m_finishTitle->setWordWrap(true);
    m_finishDetail = new QLabel(finishPage_);
    m_finishDetail->setObjectName("sub");
    m_finishDetail->setAlignment(Qt::AlignCenter);
    m_finishDetail->setWordWrap(true);
    m_settingsCheck = new QCheckBox(QStringLiteral(u"打开蚌壳设置"), finishPage_);
    m_settingsCheck->setChecked(false);

    m_logoffBtn = new QPushButton(QStringLiteral(u"注销以完成卸载…"));
    m_logoffBtn->setMinimumHeight(36);
    m_logoffBtn->setVisible(false);
    connect(m_logoffBtn, &QPushButton::clicked, this, [this] {
      const bool reboot = m_exitCode == 3010 || m_exitCode == 1641;
      if (QMessageBox::question(this, QStringLiteral(u"保存工作后继续"),
          reboot ? QStringLiteral(u"现在重启 Windows？请先保存所有工作。")
                 : QStringLiteral(u"现在注销？请先保存所有工作。")) != QMessageBox::Yes) return;
      QProcess::startDetached(QStringLiteral("shutdown.exe"),
          reboot ? QStringList{"/r", "/t", "0"} : QStringList{"/l"});
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
      if (m_settingsCheck->isChecked() && m_settingsCheck->isVisible()) {
        // explorer 拉起 = 用户会话、非提升，避免服务继承管理员上下文
        QProcess::startDetached(
            QStringLiteral("explorer"),
            {QDir::toNativeSeparators(QDir(InstalledRoot()).filePath(QStringLiteral("BangkeSettings.exe")))});
      }
      close();
    });
    box->addStretch();
    box->addWidget(m_check, 0, Qt::AlignHCenter);
    box->addSpacing(4);
    box->addWidget(m_finishTitle);
    box->addWidget(m_finishDetail);
    box->addWidget(m_settingsCheck, 0, Qt::AlignHCenter);
    box->addSpacing(8);
    box->addWidget(m_logoffBtn);
    box->addWidget(m_openLogBtn);
    box->addSpacing(8);
    box->addWidget(doneBtn);
    box->addStretch();
    m_stack->addWidget(finishPage_);
  }

  QString m_msiPath, m_productCode, m_pkgVersion;
  bool m_primaryRepair = false;
  QStackedWidget* m_stack = nullptr;
  QFrame *welcomePage_ = nullptr, *progressPage_ = nullptr, *finishPage_ = nullptr;
  QFrame *freshGroup_ = nullptr, *installedGroup_ = nullptr;
  QLabel *m_installedLabel = nullptr, *m_pkgLabel = nullptr, *m_msiMissing = nullptr,
         *m_progressTitle = nullptr, *m_actionLabel = nullptr, *m_progressHint = nullptr,
         *m_finishTitle = nullptr, *m_finishDetail = nullptr;
  BusyBar* m_bar = nullptr;
  ShellMark* m_scene = nullptr;
  CheckMark* m_check = nullptr;
  bool m_entered = false;
  QCheckBox* m_settingsCheck = nullptr;
  QPushButton* m_primaryBtn = nullptr;
  QPushButton* m_repairBtn = nullptr;
  QPushButton *m_logoffBtn = nullptr, *m_openLogBtn = nullptr;
  CloseButton* m_closeBtn = nullptr;
  MsiJob m_job;
  bool m_running = false;
  QProcess* m_proc = nullptr;
  QTimer* m_pollTimer = nullptr;
  QFileSystemWatcher* m_watcher = nullptr;
  QFile m_logFile;
  qint64 m_logPos = 0;
  int m_stage = 0;
  QByteArray m_logPending;
  bool m_logEncodingKnown = false, m_logUtf16 = false;
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
  QFontDatabase::setApplicationFallbackFontFamilies(
      QChar::Script_Han, {QStringLiteral("Microsoft YaHei UI")});
  BootLog("QApplication up");
  QApplication::setApplicationName(QStringLiteral(u"蚌壳拼音安装器"));

  app.setStyle("Fusion");
  ApplyInstallerTheme(InstallerUsesDarkTheme());

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
