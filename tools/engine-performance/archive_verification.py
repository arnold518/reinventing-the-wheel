#!/usr/bin/env python3
"""Losslessly archive completed private verification ELFs, retaining hashes."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import stat
import experiment as e


def digest_gzip(path):
    h=hashlib.sha256()
    with gzip.open(path,'rb') as stream:
        for block in iter(lambda:stream.read(65536),b''):h.update(block)
    return h.hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing')
    p.add_argument('--restore',help='Restore one relative path from the archive index; keep its compressed copy')
    a=p.parse_args();root=a.root.resolve();index=root/'verification-archives.json'
    records=json.loads(index.read_text()) if index.exists() else {}
    def save():
        temporary=index.with_suffix('.json.tmp');e.save(temporary,records);temporary.replace(index)
    if a.restore:
        if a.restore not in records:raise ValueError('Path is not in the archive index')
        item=records[a.restore];path=root/a.restore;compressed=root/item['compressed']
        if not path.resolve().is_relative_to(root):raise ValueError('Archive path escapes root')
        if path.exists():
            if e.sha(path)!=item['sha256']:raise RuntimeError('Existing path differs; refusing overwrite')
            return
        if os.statvfs(root).f_bavail*os.statvfs(root).f_frsize<item['bytes']+1024**3:
            raise RuntimeError('Restoring requires 1 GiB headroom beyond the file size')
        temporary=path.with_suffix(path.suffix+'.restore-tmp');h=hashlib.sha256()
        with gzip.open(compressed,'rb') as src,temporary.open('wb') as dst:
            for block in iter(lambda:src.read(65536),b''):dst.write(block);h.update(block)
        if h.hexdigest()!=item['sha256']:raise RuntimeError('Restoration hash differs; original path remains absent')
        temporary.chmod(item['mode']);os.utime(temporary,ns=(item['atimeNs'],item['mtimeNs']));temporary.replace(path)
        item['status']='restored';save();print('RESTORED',a.restore);return
    active=set()
    for process in Path('/proc').iterdir():
        if process.name.isdigit():
            try:active.add((process/'exe').resolve(strict=True))
            except OSError:pass
    count=0;released=0
    for path in sorted(root.rglob('verify-*')):
        if path.is_symlink() or not path.is_file() or not os.access(path,os.X_OK) or path.resolve() in active:continue
        with path.open('rb') as src:
            if src.read(4)!=b'\x7fELF':continue
        before=path.stat();key=str(path.relative_to(root));compressed=path.with_name(path.name+'.gz')
        if compressed.exists():
            original=e.sha(path)
            if digest_gzip(compressed)!=original:raise RuntimeError('Existing archive differs: '+key)
        else:
            temporary=compressed.with_suffix('.gz.tmp');h=hashlib.sha256()
            with path.open('rb') as src,temporary.open('wb') as raw:
                with gzip.GzipFile(filename='',mode='wb',fileobj=raw,compresslevel=1,mtime=int(before.st_mtime)) as dst:
                    for block in iter(lambda:src.read(65536),b''):h.update(block);dst.write(block)
            original=h.hexdigest()
            if digest_gzip(temporary)!=original:raise RuntimeError('Archive roundtrip failed: '+key)
            temporary.replace(compressed)
        if path.stat().st_mtime_ns!=before.st_mtime_ns or path.stat().st_size!=before.st_size:
            raise RuntimeError('Verification executable changed while archiving: '+key)
        item=dict(compressed=str(compressed.relative_to(root)),sha256=original,compressedSha256=e.sha(compressed),
            bytes=before.st_size,compressedBytes=compressed.stat().st_size,mode=stat.S_IMODE(before.st_mode),
            atimeNs=before.st_atime_ns,mtimeNs=before.st_mtime_ns,status='compressed-verified')
        records[key]=item;save() # Commit a verified recoverable copy before unlinking.
        path.unlink();item['status']='archived';save();count+=1;released+=item['bytes']-item['compressedBytes']
    print(json.dumps(dict(archivedFiles=count,releasedBytes=released,index=str(index))))


if __name__=='__main__':main()
