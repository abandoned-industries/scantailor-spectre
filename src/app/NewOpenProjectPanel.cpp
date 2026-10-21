// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "NewOpenProjectPanel.h"

#include <core/IconProvider.h>

#include <algorithm>
#include <functional>
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "RecentProjects.h"
#include "version.h"

namespace {
class RecentProjectRow : public QWidget {
 public:
  RecentProjectRow(const QString& projectName, const QString& parentFolder,
                   std::function<void()> activated, QWidget* parent = nullptr)
      : QWidget(parent), m_activated(std::move(activated)) {
    setFixedHeight(36);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* nameLabel = new QLabel(projectName, this);
    nameLabel->setObjectName(QStringLiteral("recentProjectName"));
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(nameLabel);

    auto* folderLabel = new QLabel(parentFolder, this);
    folderLabel->setObjectName(QStringLiteral("recentProjectFolder"));
    folderLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(folderLabel);
  }

 protected:
  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      m_activated();
    }
    QWidget::mousePressEvent(event);
  }

 private:
  std::function<void()> m_activated;
};

QWidget* createActionRow(const QString& text, const QString& shortcutHint, const QIcon& icon,
                         QWidget* parent, const std::function<void()>& activated) {
  auto* row = new QWidget(parent);
  row->setFixedHeight(44);

  auto* layout = new QGridLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* button = new QPushButton(icon, text, row);
  button->setObjectName(QStringLiteral("startupActionButton"));
  button->setCursor(Qt::PointingHandCursor);
  button->setIconSize(QSize(16, 16));
  layout->addWidget(button, 0, 0);

  auto* hint = new QLabel(shortcutHint, row);
  hint->setObjectName(QStringLiteral("startupShortcutHint"));
  hint->setAttribute(Qt::WA_TransparentForMouseEvents);
  hint->setContentsMargins(0, 0, 14, 0);
  layout->addWidget(hint, 0, 0, Qt::AlignRight | Qt::AlignVCenter);

  QObject::connect(button, &QPushButton::clicked, row, activated);
  return row;
}
}  // namespace

NewOpenProjectPanel::NewOpenProjectPanel(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("newOpenProjectPanel"));
  setAttribute(Qt::WA_StyledBackground);

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(40, 40, 40, 40);
  mainLayout->setSpacing(12);

  auto* header = new QWidget(this);
  header->setFixedHeight(48);
  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(12);

  auto* appIcon = new QLabel(header);
  appIcon->setFixedSize(48, 48);
  appIcon->setPixmap(QPixmap(QStringLiteral(":/scantailor-spectre2.png"))
                         .scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  headerLayout->addWidget(appIcon);

  auto* titleColumn = new QVBoxLayout();
  titleColumn->setContentsMargins(0, 0, 0, 0);
  titleColumn->setSpacing(0);

  auto* appName = new QLabel(tr("ScanTailor Spectre"), header);
  QFont appNameFont = appName->font();
  appNameFont.setPointSize(appNameFont.pointSize() + 5);
  appNameFont.setWeight(QFont::DemiBold);
  appName->setFont(appNameFont);
  titleColumn->addWidget(appName);

  auto* version = new QLabel(QString::fromUtf8(VERSION), header);
  version->setObjectName(QStringLiteral("startupSecondaryLabel"));
  QFont versionFont = version->font();
  versionFont.setPointSize(std::max(1, versionFont.pointSize() - 2));
  version->setFont(versionFont);
  titleColumn->addWidget(version);
  headerLayout->addLayout(titleColumn);
  headerLayout->addStretch();
  mainLayout->addWidget(header);

  auto& icons = IconProvider::getInstance();
  mainLayout->addWidget(createActionRow(tr("Import PDF"), QStringLiteral("⌘I"), icons.getIcon("file"), this,
                                        [this]() { emit importPdf(); }));
  mainLayout->addWidget(createActionRow(tr("Import Folder"), QString(), icons.getIcon("folder"), this,
                                        [this]() { emit importFolder(); }));
  mainLayout->addWidget(createActionRow(tr("Open Project"), QStringLiteral("⌘O"), icons.getIcon("project"), this,
                                        [this]() { emit openProject(); }));

  auto* recentLabel = new QLabel(tr("Recent Projects"), this);
  recentLabel->setObjectName(QStringLiteral("recentProjectsHeading"));
  mainLayout->addWidget(recentLabel);

  m_recentContainer = new QWidget(this);
  m_recentContainer->setObjectName(QStringLiteral("recentProjectsContainer"));
  m_recentLayout = new QVBoxLayout(m_recentContainer);
  m_recentLayout->setContentsMargins(0, 0, 0, 0);
  m_recentLayout->setSpacing(0);
  m_recentLayout->setAlignment(Qt::AlignTop);

  auto* recentScroll = new QScrollArea(this);
  recentScroll->setObjectName(QStringLiteral("recentProjectsScroll"));
  recentScroll->setFrameShape(QFrame::NoFrame);
  recentScroll->setWidgetResizable(true);
  recentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  recentScroll->setWidget(m_recentContainer);
  mainLayout->addWidget(recentScroll, 1);

  m_noRecentLabel = new QLabel(tr("No recent projects"), m_recentContainer);
  m_noRecentLabel->setObjectName(QStringLiteral("startupSecondaryLabel"));
  m_recentLayout->addWidget(m_noRecentLabel);

  RecentProjects rp;
  rp.read();
  if (!rp.validate()) {
    rp.write();
  }

  if (!rp.isEmpty()) {
    m_noRecentLabel->setVisible(false);
    rp.enumerate([this](const QString& filePath) { addRecentProject(filePath); });
  }
}

void NewOpenProjectPanel::addRecentProject(const QString& filePath) {
  const QFileInfo fileInfo(filePath);
  QString baseName(fileInfo.completeBaseName());
  if (baseName.isEmpty()) {
    baseName = QChar('_');
  }

  const QString parentFolder = fileInfo.dir().dirName();
  auto* row = new RecentProjectRow(baseName, parentFolder,
                                   [this, filePath]() { emit openRecentProject(filePath); }, m_recentContainer);
  row->setToolTip(filePath);
  m_recentLayout->addWidget(row);
}
