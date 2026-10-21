// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "SplitModeDialog.h"

#include <core/IconProvider.h>

#include <cassert>
#include <iostream>

#include "ApplyLayoutType.h"
#include "PageSelectionAccessor.h"
#include "SplitModeDialogMode.h"

namespace page_split {
SplitModeDialog::SplitModeDialog(QWidget* const parent,
                                 const PageId& curPage,
                                 const PageSelectionAccessor& pageSelectionAccessor,
                                 const LayoutType layoutType,
                                 const PageLayout::Type autoDetectedLayoutType,
                                 const AutoManualMode splitLineMode)
    : QDialog(parent),
      m_pages(pageSelectionAccessor.allPages()),
      m_selectedPages(pageSelectionAccessor.selectedPages()),
      m_curPage(curPage),
      m_scopeGroup(new QButtonGroup(this)),
      m_layoutType(layoutType),
      m_autoDetectedLayoutType(autoDetectedLayoutType) {
  setupUi(this);
  m_scopeGroup->addButton(thisPageRB);
  m_scopeGroup->addButton(allPagesRB);
  m_scopeGroup->addButton(thisPageAndFollowersRB);
  m_scopeGroup->addButton(thisEveryOtherRB);
  m_scopeGroup->addButton(everyOtherRB);
  m_scopeGroup->addButton(selectedPagesRB);
  m_scopeGroup->addButton(everyOtherSelectedRB);
  if (m_selectedPages.size() <= 1) {
    selectedPagesRB->setEnabled(false);
    selectedPagesHint->setEnabled(false);
    everyOtherSelectedRB->setEnabled(false);
    everyOtherSelectedHint->setEnabled(false);
  }

  if (splitModeDialogOpensManual(m_layoutType, splitLineMode)) {
    modeManual->setChecked(true);
    manualModeSelected();
  } else {
    modeAuto->setChecked(true);
    autoDetectionSelected();
  }

  connect(modeAuto, SIGNAL(pressed()), this, SLOT(autoDetectionSelected()));
  connect(modeManual, SIGNAL(pressed()), this, SLOT(manualModeSelected()));
  connect(buttonBox, SIGNAL(accepted()), this, SLOT(onSubmit()));
}

SplitModeDialog::~SplitModeDialog() = default;

void SplitModeDialog::autoDetectionSelected() {
  layoutTypeLabel->setPixmap(iconFor(AUTO_LAYOUT_TYPE).pixmap(32, 32));
}

void SplitModeDialog::manualModeSelected() {
  layoutTypeLabel->setPixmap(iconFor(combinedLayoutType()).pixmap(32, 32));
}

void SplitModeDialog::onSubmit() {
  LayoutType layoutType = AUTO_LAYOUT_TYPE;
  if (modeManual->isChecked()) {
    layoutType = combinedLayoutType();
  }

  LayoutTypeScope scope;
  if (thisPageRB->isChecked()) {
    scope = LayoutTypeScope::THIS_PAGE;
  } else if (allPagesRB->isChecked()) {
    scope = LayoutTypeScope::ALL_PAGES;
  } else if (thisPageAndFollowersRB->isChecked()) {
    scope = LayoutTypeScope::THIS_PAGE_AND_FOLLOWERS;
  } else if (selectedPagesRB->isChecked()) {
    scope = LayoutTypeScope::SELECTED_PAGES;
  } else if (everyOtherRB->isChecked()) {
    scope = LayoutTypeScope::EVERY_OTHER;
  } else if (thisEveryOtherRB->isChecked()) {
    scope = LayoutTypeScope::THIS_PAGE_AND_FOLLOWING_EVERY_OTHER;
  } else if (everyOtherSelectedRB->isChecked()) {
    scope = LayoutTypeScope::EVERY_OTHER_SELECTED;
  } else {
    return;
  }

  const std::set<PageId> pages = pagesForLayoutTypeScope(m_pages, m_selectedPages, m_curPage, scope);
  emit accepted(pages, layoutType);
  // We assume the default connection from accepted() to accept()
  // was removed.
  accept();
}  // SplitModeDialog::onSubmit

LayoutType SplitModeDialog::combinedLayoutType() const {
  if (m_layoutType != AUTO_LAYOUT_TYPE) {
    return m_layoutType;
  }

  switch (m_autoDetectedLayoutType) {
    case PageLayout::SINGLE_PAGE_UNCUT:
      return SINGLE_PAGE_UNCUT;
    case PageLayout::SINGLE_PAGE_CUT:
      return PAGE_PLUS_OFFCUT;
    case PageLayout::TWO_PAGES:
      return TWO_PAGES;
  }

  assert(!"Unreachable");
  return AUTO_LAYOUT_TYPE;
}

QIcon SplitModeDialog::iconFor(const LayoutType layoutType) {
  QIcon icon;
  switch (layoutType) {
    case AUTO_LAYOUT_TYPE:
      icon = IconProvider::getInstance().getIcon("layout_type_auto");
      break;
    case SINGLE_PAGE_UNCUT:
      icon = IconProvider::getInstance().getIcon("single_page_uncut");
      break;
    case PAGE_PLUS_OFFCUT:
      icon = IconProvider::getInstance().getIcon("right_page_plus_offcut");
      break;
    case TWO_PAGES:
      icon = IconProvider::getInstance().getIcon("two_pages");
      break;
  }
  return icon;
}
}  // namespace page_split
