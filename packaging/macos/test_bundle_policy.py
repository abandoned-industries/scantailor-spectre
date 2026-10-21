#!/usr/bin/env python3
"""Release regression: packaging must never lower deployment metadata."""
import pathlib,unittest
class PackagingPolicy(unittest.TestCase):
    def test_no_minimum_falsification(self):
        script=pathlib.Path(__file__).with_name('fix-bundle-libs.sh').read_text()
        self.assertNotIn('-set-build-version',script)
        self.assertNotIn('retarget_newer_dylibs',script)
if __name__=='__main__': unittest.main()
