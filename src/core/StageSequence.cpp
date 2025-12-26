// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StageSequence.h"

#include "ProjectPages.h"

StageSequence::StageSequence(const std::shared_ptr<ProjectPages>& pages,
                             const PageSelectionAccessor& pageSelectionAccessor)
    : m_fixOrientationFilter(std::make_shared<fix_orientation::Filter>(pageSelectionAccessor)),
      m_pageSplitFilter(std::make_shared<page_split::Filter>(pages, pageSelectionAccessor)),
      m_deskewFilter(std::make_shared<deskew::Filter>(pageSelectionAccessor)),
      m_selectContentFilter(std::make_shared<select_content::Filter>(pageSelectionAccessor)),
      m_pageLayoutFilter(std::make_shared<page_layout::Filter>(pages, pageSelectionAccessor)),
      m_finalizeFilter(std::make_shared<finalize::Filter>(pages, pageSelectionAccessor)),
      m_outputFilter(std::make_shared<output::Filter>(pages, pageSelectionAccessor)),
      m_ocrFilter(std::make_shared<ocr::Filter>(pages, pageSelectionAccessor)),
      m_exportFilter(std::make_shared<export_::Filter>(pages, pageSelectionAccessor)) {
  // Connect finalize filter to output settings so user changes in finalize propagate to output
  m_finalizeFilter->setOutputSettings(m_outputFilter->settings());
  // Connect output filter to finalize settings so user changes in output propagate back to finalize
  m_outputFilter->setFinalizeSettings(m_finalizeFilter->settings());
  // Connect export filter to output settings for PDF export
  m_exportFilter->setOutputSettings(m_outputFilter->settings());

  m_fixOrientationFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_fixOrientationFilter);

  m_pageSplitFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_pageSplitFilter);

  m_deskewFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_deskewFilter);

  m_selectContentFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_selectContentFilter);

  m_pageLayoutFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_pageLayoutFilter);

  m_finalizeFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_finalizeFilter);

  m_outputFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_outputFilter);

  m_ocrFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_ocrFilter);

  m_exportFilterIdx = static_cast<int>(m_filters.size());
  m_filters.emplace_back(m_exportFilter);
}

void StageSequence::performRelinking(const AbstractRelinker& relinker) {
  for (FilterPtr& filter : m_filters) {
    filter->performRelinking(relinker);
  }
}

int StageSequence::findFilter(const FilterPtr& filter) const {
  int idx = 0;
  for (const FilterPtr& f : m_filters) {
    if (f == filter) {
      return idx;
    }
    ++idx;
  }
  return -1;
}
