// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PDFIMPORTDIALOG_H_
#define SCANTAILOR_APP_PDFIMPORTDIALOG_H_

#include <QDialog>

class QComboBox;
class QLabel;

/**
 * \brief Dialog shown after selecting a PDF file for import.
 *
 * Displays detected DPI from embedded images and allows user to
 * select the import resolution (300, 400, or 600 DPI).
 */
class PdfImportDialog : public QDialog {
  Q_OBJECT

 public:
  PdfImportDialog(QWidget* parent, const QString& pdfPath, int pageCount, int detectedDpi);

  int selectedDpi() const;

 private:
  QComboBox* m_dpiCombo;
};

#endif  // SCANTAILOR_APP_PDFIMPORTDIALOG_H_
