"""Package accepted runtime files and overlays; the full Data tree stays external."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--core-archive',type=Path,required=True);p.add_argument('--addon-overlay',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1]
assert not a.output.exists(),'Output already exists'
baseline=json.loads((r/'release/baseline.json').read_text())
assert hashlib.sha256((a.runtime/'Kaentake.dll').read_bytes()).hexdigest()==baseline['accepted_artifacts']['Kaentake.dll'],'Runtime DLL is not the accepted baseline'
assert hashlib.sha256(a.core_archive.read_bytes()).hexdigest()==baseline['assets']['core_archive_sha256'],'Core asset archive mismatch'
for name,expected in baseline['assets']['addon_overlay_files'].items():
 assert hashlib.sha256((a.addon_overlay/name).read_bytes()).hexdigest()==expected,'Addon asset mismatch: '+name
files={}
for f in a.runtime.iterdir():
 if f.is_file() and f.suffix.lower() not in ['.log'] and f.name!='config.ini':files[f.name]=f.read_bytes()
with zipfile.ZipFile(a.core_archive) as z:
 for name in z.namelist():
  if name.startswith('Client/Data/') and not name.endswith('/'):
   target=name[len('Client/'):];assert '..' not in PurePosixPath(target).parts;files[target]=z.read(name)
for f in a.addon_overlay.rglob('*'):
 if f.is_file():files['Data/'+f.relative_to(a.addon_overlay).as_posix()]=f.read_bytes()
files['config.ini']=(r/'config.example.ini').read_bytes();files['BASELINE.json']=(r/'release/baseline.json').read_bytes()
manifest={'repository':'https://github.com/joshhmann/SlumsMS-Client','commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),'files':{n:hashlib.sha256(b).hexdigest() for n,b in files.items()},'full_data_included':False,'apply_to':'complete owned v83 client base with matching Data; configure server host/login port'}
a.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.output,'w',allowZip64=True) as z:
 for n,b in files.items():z.writestr(n,b,compress_type=zipfile.ZIP_DEFLATED)
 z.writestr('manifest.json',json.dumps(manifest,indent=2))
with zipfile.ZipFile(a.output) as z:assert z.testzip() is None
print(json.dumps({'archive':str(a.output),'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'files':len(files)}))
