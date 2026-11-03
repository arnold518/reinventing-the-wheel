"""Keep immutable native trace chunks single-copy on same-filesystem builds."""
import errno
import os
from pathlib import Path
import re
from mkdocs.structure.files import File

TRACE = re.compile(r"assets/rv32im/(?:gameplay-demos|long-gameplays)/[^/]+-ticks/[^/]+/tick-[0-9]+\.bin\.gz$")

class RecordingChunkFile(File):
    def copy_file(self, dirty=False):
        if dirty and not self.is_modified():
            return
        source,destination=Path(self.abs_src_path),Path(self.abs_dest_path)
        destination.parent.mkdir(parents=True,exist_ok=True)
        if destination.exists() and os.path.samefile(source,destination):
            return
        temporary=destination.with_name(destination.name+f'.link-{os.getpid()}')
        try:
            os.link(source,temporary)
        except OSError as error:
            if error.errno not in (errno.EXDEV,errno.EPERM,errno.EOPNOTSUPP):
                raise
            # A different filesystem must receive a real standalone copy.
            return super().copy_file(dirty=dirty)
        try:
            os.replace(temporary,destination)
        finally:
            temporary.unlink(missing_ok=True)

def on_files(files,**_kwargs):
    for file in list(files):
        if TRACE.fullmatch(file.src_uri) and file.abs_src_path:
            replacement=RecordingChunkFile(file.src_uri,file.src_dir,file.dest_dir,file.use_directory_urls,
                dest_uri=file.dest_uri,inclusion=file.inclusion)
            files.remove(file);files.append(replacement)
    return files
