// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license found in LICENSE.

import CoreGraphics
import Foundation
import Vision

private final class DocumentRecognitionResult: @unchecked Sendable {
  var lines: [[String: Any]] = []
  var error: Error?
}

/// Small Objective-C bridge for Vision's Swift-only document recognizer.
///
/// The library containing this class targets macOS 26 and is loaded dynamically.
/// The main application retains its macOS 15 deployment target and its existing
/// VNRecognizeTextRequest fallback.
@objc(SpectreDocumentOCRBridge)
public final class SpectreDocumentOCRBridge: NSObject {
  // Vision's document model already exploits the Neural Engine internally.
  // On macOS 26.0, overlapping these requests in one process crashes inside
  // Vision/TextRecognition teardown on long books (reproduced with unbounded
  // and four-request limits). Keep this request type serial until Apple fixes
  // that lifetime bug; the outer page pipeline continues to run concurrently.
  private static let requestSlots = DispatchSemaphore(value: 1)

  @objc(recognizeImage:languageCode:usesLanguageCorrection:error:)
  public static func recognize(
    image: CGImage,
    languageCode: String?,
    usesLanguageCorrection: Bool
  ) throws -> [[String: Any]] {
    let semaphore = DispatchSemaphore(value: 0)
    let result = DocumentRecognitionResult()

    requestSlots.wait()
    Task.detached {
      defer {
        requestSlots.signal()
        semaphore.signal()
      }
      do {
        var request = RecognizeDocumentsRequest()
        if let languageCode, !languageCode.isEmpty {
          let language = Locale.Language(identifier: languageCode)
          if request.supportedRecognitionLanguages.contains(language) {
            request.textRecognitionOptions.recognitionLanguages = [language]
          } else {
            request.textRecognitionOptions.automaticallyDetectLanguage = true
          }
        } else {
          request.textRecognitionOptions.automaticallyDetectLanguage = true
        }
        request.textRecognitionOptions.useLanguageCorrection = usesLanguageCorrection

        let observations = try await request.perform(on: image)
        if let document = observations.first?.document {
          result.lines = document.text.lines.compactMap { line -> [String: Any]? in
            guard let candidate = line.topCandidates(1).first else {
              return nil
            }
            let box = line.boundingBox
            return [
              "text": candidate.string,
              "confidence": candidate.confidence,
              "x": box.origin.x,
              "y": box.origin.y,
              "width": box.width,
              "height": box.height
            ]
          }
        }
      } catch {
        result.error = error
      }
    }

    semaphore.wait()
    if let error = result.error {
      throw error
    }
    return result.lines
  }
}
