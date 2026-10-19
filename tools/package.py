#!/usr/bin/env python3
"""Package only release files and source; exclude local tools and test outputs."""
from pathlib import Path
import hashlib
import shutil
import zipfile

ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT.parent/'deliverables'
OUT.mkdir(exist_ok=True)
name='App_Title_v0.2.8.elf'
elf=OUT/name
shutil.copyfile(ROOT/'dist'/name,elf)
files={name:elf.read_bytes()}
for doc in ['README.md','README_RU.md','CHANGELOG.md','VALIDATION_v0.2.8_20261019.md','LICENSE','LICENSE-HDE64.txt','THIRD_PARTY_NOTICES.md']:
    files[doc]=(ROOT/doc).read_bytes()
for doc in ['Makefile','BUILDING.md','README.md','README_RU.md','CHANGELOG.md','.gitignore','VALIDATION_v0.2.8_20261019.md','DEPENDENCIES.json','LICENSE','LICENSE-HDE64.txt','THIRD_PARTY_NOTICES.md']:
    files['source/'+doc]=(ROOT/doc).read_bytes()
for directory in ['src','include','assets','tools','tests','vendor']:
    for path in sorted((ROOT/directory).rglob('*')):
        if path.is_file() and '__pycache__' not in path.parts and path.suffix!='.pyc':
            files['source/'+path.relative_to(ROOT).as_posix()]=path.read_bytes()
files['SHA256SUMS.txt']=''.join(hashlib.sha256(data).hexdigest()+'  '+path+'\n' for path,data in sorted(files.items())).encode()
archive=OUT/'App_Title_v0.2.8.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for path,data in sorted(files.items()):z.writestr(path,data)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert z.read(name)==elf.read_bytes()
    for line in z.read('SHA256SUMS.txt').decode().splitlines():
        digest,path=line.split('  ',1)
        assert hashlib.sha256(z.read(path)).hexdigest()==digest
print(f'ELF: {elf.name}, {elf.stat().st_size} bytes, SHA256 {hashlib.sha256(elf.read_bytes()).hexdigest()}')
print(f'ZIP: {archive.name}, {archive.stat().st_size} bytes, {len(files)} entries; CRC and every packaged SHA256 verified')
