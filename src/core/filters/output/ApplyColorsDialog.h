// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_APPLYCOLORSDIALOG_H_
#define SCANTAILOR_OUTPUT_APPLYCOLORSDIALOG_H_

#include <QButtonGroup>
#include <QDialog>
#include <memory>
#include <set>

#include "ColorParams.h"
#include "PageId.h"
#include "PageSequence.h"
#include "Settings.h"
#include "ui_ApplyColorsDialog.h"

class PageSelectionAccessor;

namespace output {
class ApplyColorsDialog : public QDialog, private Ui::ApplyColorsDialog {
  Q_OBJECT
 public:
  ApplyColorsDialog(QWidget* parent,
                    const PageId& pageId,
                    const PageSelectionAccessor& pageSelectionAccessor,
                    ColorMode colorMode = BLACK_AND_WHITE,
                    std::shared_ptr<Settings> settings = nullptr);

  ~ApplyColorsDialog() override;

 signals:

  void accepted(const std::set<PageId>& pages);

 private slots:

  void onSubmit();

 private:
  QString colorModeLabel() const;
  std::set<PageId> filterPagesByColorMode(const std::set<PageId>& pages) const;

  PageSequence m_pages;
  std::set<PageId> m_selectedPages;
  PageId m_curPage;
  QButtonGroup* m_scopeGroup;
  ColorMode m_colorMode;
  std::shared_ptr<Settings> m_settings;
};
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_APPLYCOLORSDIALOG_H_
