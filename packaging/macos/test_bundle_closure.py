#!/usr/bin/env python3
import pathlib,plistlib,subprocess,tempfile,unittest
from bundle_closure import audit,repair
class ClosureTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.root=pathlib.Path(self.tmp.name); self.app=self.root/'Test.app'
        (self.app/'Contents/MacOS').mkdir(parents=True)
        (self.app/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable':'test','LSMinimumSystemVersion':'15.0'}))
        self.src=self.root/'test.c'; self.src.write_text('int main(void) {return 0;}')
    def tearDown(self): self.tmp.cleanup()
    def compile(self,minimum):
        subprocess.run(['xcrun','clang','-arch','arm64','-mmacosx-version-min='+minimum,str(self.src),'-o',str(self.app/'Contents/MacOS/test')],check=True)
    def test_accepts_genuine_15_binary(self):
        self.compile('15.0'); self.assertEqual(audit(self.app)['errors'],[])
    def test_rejects_26_without_modifying_bytes(self):
        self.compile('26.0'); p=self.app/'Contents/MacOS/test'; before=p.read_bytes()
        self.assertTrue(any('minimum' in s for s in audit(self.app)['errors']))
        with self.assertRaises(ValueError): repair(self.app,[self.root])
        self.assertEqual(p.read_bytes(),before)
    def test_plugin_and_resource_gates(self):
        self.compile('15.0'); (self.app/'Contents/libqtvirtualkeyboardplugin.dylib').touch()
        errors=audit(self.app,True)['errors']
        self.assertTrue(any('prohibited' in e for e in errors)); self.assertTrue(any('default.metallib' in e for e in errors))
    def test_repairs_plugin_id_and_transitive_closure(self):
        self.compile('15.0')
        prefix=self.root/'prefix'; (prefix/'lib').mkdir(parents=True)
        lib=prefix/'lib/libfixture.dylib'; src=self.root/'fixture.c'; src.write_text('int fixture(void){return 3;}')
        subprocess.run(['xcrun','clang','-arch','arm64','-mmacosx-version-min=15.0','-dynamiclib','-Wl,-headerpad_max_install_names',str(src),'-install_name',str(lib),'-o',str(lib)],check=True)
        plugin=self.app/'Contents/PlugIns/example/plugin.dylib'; plugin.parent.mkdir(parents=True)
        src.write_text('extern int fixture(void); int plugin(void){return fixture();}')
        subprocess.run(['xcrun','clang','-arch','arm64','-mmacosx-version-min=15.0','-dynamiclib','-Wl,-headerpad_max_install_names',str(src),str(lib),'-o',str(plugin)],check=True)
        self.assertEqual(repair(self.app,[prefix])['errors'],[])
        self.assertTrue((self.app/'Contents/Frameworks/libfixture.dylib').exists())
    def test_preserves_existing_loader_relative_framework_dependency(self):
        self.compile('15.0')
        fw=self.app/'Contents/Frameworks/Special.framework/Versions/A'; (fw/'Helpers').mkdir(parents=True)
        src=self.root/'lib.c'; src.write_text('int fixture(void){return 3;}')
        subprocess.run(['xcrun','clang','-arch','arm64','-mmacosx-version-min=15.0','-dynamiclib','-Wl,-headerpad_max_install_names',str(src),'-install_name','@loader_path/../Special','-o',str(fw/'Special')],check=True)
        src.write_text('extern int fixture(void); int helper(void){return fixture();}')
        subprocess.run(['xcrun','clang','-arch','arm64','-mmacosx-version-min=15.0','-dynamiclib','-Wl,-headerpad_max_install_names',str(src),str(fw/'Special'),'-o',str(fw/'Helpers/helper.dylib')],check=True)
        self.assertEqual(repair(self.app,[self.root])['errors'],[])
    def test_rejects_external_rpath(self):
        self.compile('15.0'); subprocess.run(['install_name_tool','-add_rpath','/opt/homebrew/lib',str(self.app/'Contents/MacOS/test')],check=True)
        self.assertTrue(any('rpath' in e for e in audit(self.app)['errors']))
if __name__=='__main__': unittest.main()
