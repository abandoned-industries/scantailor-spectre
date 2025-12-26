// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_OPTIONSWIDGET_H_
#define SCANTAILOR_OCR_OPTIONSWIDGET_H_

#include <memory>

#include "FilterOptionsWidget.h"
#include "PageId.h"
#include "PageSelectionAccessor.h"
#include "Settings.h"
#include "ui_OcrOptionsWidget.h"

namespace ocr {
class OptionsWidget : public FilterOptionsWidget, private Ui::OcrOptionsWidget {
  Q_OBJECT

 public:
  OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor);

  ~OptionsWidget() override;

  void preUpdateUI(const PageInfo& pageInfo);
  void postUpdateUI(const PageId& pageId);

 private slots:
  void enableOcrChanged(bool checked);
  void languageChanged(int index);
  void accurateChanged(bool checked);
  void clearResultClicked();
  void clearAllClicked();

 private:
  void updateDisplay();
  void populateLanguageCombo();

  std::shared_ptr<Settings> m_settings;
  PageSelectionAccessor m_pageSelectionAccessor;
  PageId m_pageId;
};
}  // namespace ocr

#endif  // SCANTAILOR_OCR_OPTIONSWIDGET_H_
