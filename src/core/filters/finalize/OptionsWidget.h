// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_OPTIONSWIDGET_H_
#define SCANTAILOR_FINALIZE_OPTIONSWIDGET_H_

#include <memory>
#include <set>

#include "FilterOptionsWidget.h"
#include "PageId.h"
#include "PageSelectionAccessor.h"
#include "Settings.h"
#include "ui_OptionsWidget.h"

namespace output {
class Settings;
}

namespace finalize {
class OptionsWidget : public FilterOptionsWidget, private Ui::FinalizeOptionsWidget {
  Q_OBJECT

 public:
  OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor);

  ~OptionsWidget() override;

  void setOutputSettings(std::shared_ptr<output::Settings> outputSettings);

  void preUpdateUI(const PageInfo& pageInfo);
  void postUpdateUI(const PageId& pageId);

 signals:
  void invalidateThumbnail(const PageId& pageId);
  void reloadRequested();
  void invalidateAllThumbnails();
  void outputDirectoryChanged(const QString& newDir);
  void outputFormatSettingChanged(int format);         // OutputFormat enum value
  void tiffCompressionSettingChanged(int compression); // TiffCompression enum value
  void jpegQualitySettingChanged(int quality);

 private slots:
  void colorModeChanged(int index);
  void thresholdChanged(int value);
  void applyToClicked();
  void applyToConfirmed(const std::set<PageId>& pages);
  void clearCacheClicked();
  void clearAllCacheClicked();
  void autoWhiteBalanceChanged(bool checked);
  void formatChanged(int index);
  void compressionChanged(int index);
  void jpegQualityChanged(int value);

 private:
  void updateDisplay();
  void updateOutputUI();
  void updateFormatOptions();
  void updateSelectionIndicator();
  void applyColorModeToSelectedPages(ColorMode mode);

  std::shared_ptr<Settings> m_settings;
  std::shared_ptr<output::Settings> m_outputSettings;
  PageSelectionAccessor m_pageSelectionAccessor;
  PageId m_pageId;
};
}  // namespace finalize

#endif  // SCANTAILOR_FINALIZE_OPTIONSWIDGET_H_
