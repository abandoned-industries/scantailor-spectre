// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ApplyColorsDialog.h"

#include "PageSelectionAccessor.h"

namespace output {
ApplyColorsDialog::ApplyColorsDialog(QWidget* parent,
                                     const PageId& curPage,
                                     const PageSelectionAccessor& pageSelectionAccessor,
                                     ColorMode colorMode,
                                     std::shared_ptr<Settings> settings,
                                     std::shared_ptr<finalize::Settings> finalizeSettings)
    : QDialog(parent),
      m_pages(pageSelectionAccessor.allPages()),
      m_selectedPages(pageSelectionAccessor.selectedPages()),
      m_curPage(curPage),
      m_scopeGroup(new QButtonGroup(this)),
      m_colorMode(colorMode),
      m_settings(std::move(settings)),
      m_finalizeSettings(std::move(finalizeSettings)) {
  setupUi(this);
  m_scopeGroup->addButton(thisPageRB);
  m_scopeGroup->addButton(allPagesRB);
  m_scopeGroup->addButton(thisPageAndFollowersRB);
  m_scopeGroup->addButton(selectedPagesRB);

  // Update labels based on color mode
  if (m_settings) {
    const QString modeLabel = colorModeLabel();
    allPagesRB->setText(tr("All %1 pages").arg(modeLabel));
    thisPageAndFollowersRB->setText(tr("This page and following %1 ones").arg(modeLabel));
  }

  if (m_selectedPages.size() <= 1) {
    selectedPagesRB->setEnabled(false);
    selectedPagesHint->setEnabled(false);
  }

  connect(buttonBox, SIGNAL(accepted()), this, SLOT(onSubmit()));
}

ApplyColorsDialog::~ApplyColorsDialog() = default;

QString ApplyColorsDialog::colorModeLabel() const {
  switch (m_colorMode) {
    case BLACK_AND_WHITE:
      return tr("B&W");
    case GRAYSCALE:
      return tr("grayscale");
    case COLOR:
    case COLOR_GRAYSCALE:
      return tr("color");
    case MIXED:
      return tr("mixed");
    default:
      return QString();
  }
}

std::set<PageId> ApplyColorsDialog::filterPagesByColorMode(const std::set<PageId>& pages) const {
  // Use finalize settings if available (preferred - that's where color mode is set)
  // Fall back to output settings if finalize not available
  if (!m_finalizeSettings && !m_settings) {
    return pages;
  }

  std::set<PageId> filtered;
  for (const PageId& pageId : pages) {
    ColorMode pageMode;

    if (m_finalizeSettings) {
      // Get color mode from finalize settings (stage 6)
      finalize::ColorMode finalizeMode = m_finalizeSettings->getColorMode(pageId);
      switch (finalizeMode) {
        case finalize::ColorMode::BlackAndWhite:
          pageMode = BLACK_AND_WHITE;
          break;
        case finalize::ColorMode::Grayscale:
          pageMode = GRAYSCALE;
          break;
        case finalize::ColorMode::Color:
          pageMode = COLOR;
          break;
        default:
          pageMode = BLACK_AND_WHITE;  // Default
          break;
      }
    } else {
      // Fall back to output settings
      const Params params = m_settings->getParams(pageId);
      pageMode = params.colorParams().colorMode();
    }

    // Match similar modes: treat COLOR and COLOR_GRAYSCALE as same
    bool matches = false;
    switch (m_colorMode) {
      case BLACK_AND_WHITE:
        matches = (pageMode == BLACK_AND_WHITE);
        break;
      case GRAYSCALE:
        matches = (pageMode == GRAYSCALE);
        break;
      case COLOR:
      case COLOR_GRAYSCALE:
        matches = (pageMode == COLOR || pageMode == COLOR_GRAYSCALE);
        break;
      case MIXED:
        matches = (pageMode == MIXED);
        break;
      default:
        matches = true;  // Include all if mode is unknown
        break;
    }

    if (matches) {
      filtered.insert(pageId);
    }
  }
  return filtered;
}

void ApplyColorsDialog::onSubmit() {
  std::set<PageId> pages;

  // thisPageRB is intentionally not handled.
  if (allPagesRB->isChecked()) {
    m_pages.selectAll().swap(pages);
    pages = filterPagesByColorMode(pages);
  } else if (thisPageAndFollowersRB->isChecked()) {
    m_pages.selectPagePlusFollowers(m_curPage).swap(pages);
    pages = filterPagesByColorMode(pages);
  } else if (selectedPagesRB->isChecked()) {
    emit accepted(m_selectedPages);
    accept();
    return;
  }

  emit accepted(pages);

  // We assume the default connection from accepted() to accept()
  // was removed.
  accept();
}
}  // namespace output
