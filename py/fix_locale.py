"""Ensure zh_CN.GB18030 locale exists for openctp TTS .so"""
import os
import subprocess
from pathlib import Path

_locales_dir = Path("/tmp/locales")
_locale_path = _locales_dir / "zh_CN.GB18030"
if not _locale_path.exists():
    _locales_dir.mkdir(exist_ok=True)
    subprocess.run(
        ["localedef", "-i", "zh_CN", "-f", "GB18030", str(_locale_path)],
        check=True,
    )
os.environ.setdefault("LOCPATH", str(_locales_dir))
