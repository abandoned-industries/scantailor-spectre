// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_OPTIONSWIDGET_H_
#define SCANTAILOR_EXPORT_OPTIONSWIDGET_H_

#include <memory>

#include "FilterOptionsWidget.h"
#include "PageId.h"
#include "PageSelectionAccessor.h"
#include "Settings.h"
#include "ui_OptionsWidget.h"

namespace output {
class Settings;
}

namespace export_ {
class OptionsWidget : public FilterOptionsWidget, private Ui::ExportOptionsWidget {
  Q_OBJECT

 public:
  OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor);

  ~OptionsWidget() override;

  void setOutputSettings(std::shared_ptr<output::Settings> outputSettings);

  void preUpdateUI(const PageInfo& pageInfo);
  void postUpdateUI(const PageId& pageId);

 signals:
  void exportToPdfRequested();

 private slots:
  void noDpiLimitChanged(bool checked);
  void maxDpiChanged(int value);
  void compressGrayscaleChanged(bool checked);
  void qualityChanged(int index);
  void exportToPdfClicked();

 private:
  void updateDisplay();
  void populateQualityCombo();

  std::shared_ptr<Settings> m_settings;
  std::shared_ptr<output::Settings> m_outputSettings;
  PageSelectionAccessor m_pageSelectionAccessor;
  PageId m_pageId;
};
}  // namespace export_

#endif  // SCANTAILOR_EXPORT_OPTIONSWIDGET_H_
