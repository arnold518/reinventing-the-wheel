"""Actual MkDocs file contract: immutable trace links and cross-device fallback."""
import errno,importlib.util,os,tempfile,sys
from pathlib import Path
from unittest.mock import patch
try:
    from mkdocs.structure.files import File,Files
except ModuleNotFoundError:
    print('SKIP: MkDocs is required for the site asset check');raise SystemExit(77)
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('mkdocs_recordings',root/'tools/mkdocs_recordings.py')
hook=importlib.util.module_from_spec(spec);spec.loader.exec_module(hook)
with tempfile.TemporaryDirectory() as tmp:
    source,destination=Path(tmp)/'docs',Path(tmp)/'site'
    name='assets/rv32im/gameplay-demos/visual-ticks/snake/tick-0.bin.gz'
    path=source/name;path.parent.mkdir(parents=True);path.write_bytes(b'closed-native-chunk')
    other=source/'other.bin.gz';other.write_bytes(b'ordinary')
    files=hook.on_files(Files([File(name,str(source),str(destination),True),File('other.bin.gz',str(source),str(destination),True)]))
    trace=files.get_file_from_path(name);trace.copy_file();trace.copy_file(dirty=True)
    assert os.path.samefile(path,destination/name)
    assert type(files.get_file_from_path('other.bin.gz')) is File
    (destination/name).unlink()
    with patch.object(hook.os,'link',side_effect=OSError(errno.EXDEV,'different device')):
        trace.copy_file()
    assert (destination/name).read_bytes()==path.read_bytes() and not os.path.samefile(path,destination/name)
    assert not list(destination.rglob('*.link-*'))
print('PASS immutable trace hardlinks, ordinary-file preservation and cross-filesystem copies')
