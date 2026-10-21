#!/usr/bin/env python3
"""Fail-closed Mach-O closure repair/audit. Never changes deployment minima.

repair APP --prefix PINNED_QT --prefix PINNED_DEPS
 audit APP --report report.json [--require-resources]
Only arm64 releases are supported. Run repair on a disposable staging copy.
"""
import argparse,hashlib,json,os,pathlib,plistlib,re,shutil,subprocess,sys
MAGICS={bytes.fromhex(x) for x in ('feedface','cefaedfe','feedfacf','cffaedfe','cafebabe','bebafeca','cafebabf','bfbafeca')}
def run(*args):
    return subprocess.check_output([str(x) for x in args],stderr=subprocess.STDOUT,text=True)
def machos(root):
    for p in sorted(root.rglob('*')):
        if p.is_file() and not p.is_symlink():
            with p.open('rb') as f:
                if f.read(4) in MAGICS: yield p

def inspect(p):
    result={}
    for arch in run('lipo','-archs',p).strip().split():
        text=run('otool','-arch',arch,'-l',p)
        info={'minimum':[],'dependencies':[],'ids':[],'rpaths':[],'raw':text}
        for block in re.split(r'Load command \d+\n',text)[1:]:
            cm=re.search(r'\bcmd (LC_\w+)',block)
            if not cm: continue
            cmd=cm[1]
            if cmd in ('LC_BUILD_VERSION','LC_VERSION_MIN_MACOSX'):
                if cmd=='LC_BUILD_VERSION':
                    platform=re.search(r'platform (\S+)',block)
                    if not platform or platform[1] not in ('1','MACOS','macos'): raise ValueError('non-macOS platform '+str(p))
                m=re.search(r'\b(?:minos|version) (\d+(?:\.\d+)*)',block)
                if m: info['minimum'].append(m[1])
            if cmd in ('LC_ID_DYLIB','LC_LOAD_DYLIB','LC_LOAD_WEAK_DYLIB','LC_REEXPORT_DYLIB','LC_LOAD_UPWARD_DYLIB'):
                name=re.search(r'\bname (.*?) \(offset',block)[1]
                info['ids' if cmd=='LC_ID_DYLIB' else 'dependencies'].append(name)
            if cmd=='LC_RPATH': info['rpaths'].append(re.search(r'\bpath (.*?) \(offset',block)[1])
        result[arch]=info
    return result

def version(s): return tuple((list(map(int,s.split('.')))+[0,0,0])[:3])
def system(s): return s.startswith(('/System/Library/','/usr/lib/'))
def inside(p,root): return p.resolve().is_relative_to(root.resolve())
def executable(p,app):
    for parent in p.parents:
        if parent.suffix=='.app':
            pl=plistlib.loads((parent/'Contents/Info.plist').read_bytes())
            return parent/'Contents/MacOS'/pl['CFBundleExecutable']
    raise ValueError(str(p))
def expand(s,p,exe):
    if s.startswith('@loader_path/'): return p.parent/s[len('@loader_path/'):]
    if s.startswith('@executable_path/'): return exe.parent/s[len('@executable_path/'):]
    if s.startswith('/'): return pathlib.Path(s)
    return None

def audit(app,resources=False):
    errors=[]; records=[]
    pl=plistlib.loads((app/'Contents/Info.plist').read_bytes())
    if pl.get('LSMinimumSystemVersion')!='15.0': errors.append('plist minimum must be exactly 15.0')
    for p in app.rglob('*'):
        if 'virtualkeyboard' in p.name.lower(): errors.append('prohibited plugin '+str(p.relative_to(app)))
        if p.is_symlink() and (not p.exists() or not inside(p,app)): errors.append('broken/external symlink '+str(p.relative_to(app)))
    for p in machos(app):
        arches=inspect(p); exe=executable(p,app)
        if set(arches)!={'arm64'}: errors.append('not arm64-only '+str(p.relative_to(app)))
        for arch,info in arches.items():
            if not info['minimum'] or any(version(m)>version('15.0') for m in info['minimum']): errors.append('incompatible minimum '+str(p.relative_to(app))+': '+str(info['minimum']))
            for ident in info['ids']:
                if not ident.startswith('@rpath/'): errors.append('nonportable ID '+ident)
            for rp in info['rpaths']:
                resolved=expand(rp,p,exe)
                if not resolved or not inside(resolved,app): errors.append('external/unresolved rpath '+rp+' in '+str(p.relative_to(app)))
            for dep in info['dependencies']:
                if system(dep): continue
                dest=expand(dep,p,exe)
                if dep.startswith('@rpath/'):
                    candidates=[expand(rp,p,exe) for rp in info['rpaths']]
                    dest=next((q/dep[7:] for q in candidates if q and (q/dep[7:]).exists()),None)
                if not dest or not dest.exists() or not inside(dest,app): errors.append('unresolved/external dependency '+dep+' in '+str(p.relative_to(app)))
                elif arch not in inspect(dest): errors.append('dependency architecture mismatch '+dep)
        records.append({'path':str(p.relative_to(app)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'architectures':arches})
    if not records: errors.append('no Mach-O files')
    if resources:
        for name in ('default.metallib','README.pdf'):
            p=app/'Contents/Resources'/name
            if not p.is_file() or p.stat().st_size==0: errors.append('missing resource '+name)
        if not list((app/'Contents/Resources').glob('*.qm')) and not list((app/'Contents/Resources/translations').glob('*.qm')): errors.append('missing translations')
    return {'app':str(app),'errors':errors,'macho_count':len(records),'files':records}

def repair(app,prefixes):
    fw=app/'Contents/Frameworks'; fw.mkdir(exist_ok=True)
    # Reject too-new input before any mutation; do not disguise incompatibility.
    for p in machos(app):
        data=inspect(p)
        if 'arm64' not in data: raise ValueError('missing arm64 '+str(p))
        if not data['arm64']['minimum'] or any(version(m)>version('15.0') for m in data['arm64']['minimum']): raise ValueError('rebuild incompatible input '+str(p))
    for p in list(app.rglob('*virtualkeyboard*')):
        if p.is_file(): p.unlink()
    done=set()
    while True:
        pending=[p for p in machos(app) if p not in done]
        if not pending: break
        for p in pending:
            data=inspect(p)
            if 'arm64' not in data or not data['arm64']['minimum'] or any(version(m)>version('15.0') for m in data['arm64']['minimum']): raise ValueError('rebuild incompatible dependency '+str(p))
            if len(data)>1:
                thin=p.with_name(p.name+'.thin'); run('lipo',p,'-thin','arm64','-output',thin); thin.replace(p)
            info=data['arm64']; args=[]
            if info['ids']:
                if inside(p,fw): ident=str(p.relative_to(fw))
                elif inside(p,app/'Contents/PlugIns'): ident=str(p.relative_to(app/'Contents'))
                else: raise ValueError('unexpected dylib location '+str(p))
                args+=['-id','@rpath/'+ident]
            for dep in info['dependencies']:
                if system(dep): continue
                match=re.search(r'([^/]+\.framework/.*)',dep)
                rel=match[1] if match else pathlib.Path(dep).name
                target=fw/rel
                local=expand(dep,p,executable(p,app))
                if local is not None and local.exists() and inside(local,app):
                    target=local.resolve()
                if not target.exists():
                    candidates=[pre/'lib'/rel for pre in prefixes]
                    if dep.startswith('/'): candidates.insert(0,pathlib.Path(dep))
                    src=next((q for q in candidates if q.exists() and any(inside(q,pre) for pre in prefixes)),None)
                    if src is None: raise ValueError('dependency absent from approved prefixes: '+dep)
                    if match:
                        name=rel.split('/')[0]; source_fw=next(q for q in src.parents if q.name==name)
                        run('ditto',source_fw,fw/name)
                    else: shutil.copy2(src,target)
                new='@loader_path/'+os.path.relpath(target,p.parent)
                if dep!=new: args+=['-change',dep,new]
            # All dependency loads are explicit loader-relative now. No search
            # paths are necessary, including helper-local ones from macdeployqt.
            for rp in dict.fromkeys(info['rpaths']): args+=['-delete_rpath',rp]
            if args: run('install_name_tool',*args,p)
            done.add(p)
    result=audit(app)
    if result['errors']: raise ValueError('\n'.join(result['errors']))
    return result

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('mode',choices=['audit','repair']); ap.add_argument('app',type=pathlib.Path); ap.add_argument('--prefix',action='append',default=[],type=pathlib.Path); ap.add_argument('--report',type=pathlib.Path); ap.add_argument('--require-resources',action='store_true'); args=ap.parse_args()
    app=args.app.resolve()
    if args.mode=='repair':
        if not args.prefix: ap.error('repair requires explicit trusted --prefix arguments')
        repair(app,[p.resolve() for p in args.prefix])
    result=audit(app,args.require_resources)
    if args.report: args.report.write_text(json.dumps(result,indent=2))
    print(json.dumps({'macho_count':result['macho_count'],'errors':result['errors']},indent=2))
    return bool(result['errors'])
if __name__=='__main__': sys.exit(main())
