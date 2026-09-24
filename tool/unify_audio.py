"""asset/score を MP3、asset/sound/se を PCM WAV に統一する。

score: 48 kHz / Stereo / CBR 160 kbps (libmp3lame)
se:    PCM 16-bit / 48 kHz / Mono

譜面 JSON の "music" 拡張子だけ .mp3 に直す。
C++ / ヘッダのパスは書き換えない。
"""

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCORE_DIR = ROOT / "asset" / "score"
SE_DIR = ROOT / "asset" / "sound" / "se"
SOURCE_EXTS = {".wav", ".mp3", ".ogg"}


def require_ffmpeg():
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        print("ffmpeg が PATH にありません。インストールしてから再実行してください。", file=sys.stderr)
        sys.exit(1)
    return ffmpeg


def iter_audio(directory):
    if not directory.is_dir():
        print(f"ディレクトリがありません: {directory}", file=sys.stderr)
        return
    for path in sorted(directory.iterdir()):
        if path.is_file() and path.suffix.lower() in SOURCE_EXTS:
            yield path


def convert(ffmpeg, src, dst, codec_args):
    dst.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(delete=False, dir=dst.parent, suffix=dst.suffix) as tmp:
        tmp_path = Path(tmp.name)
    cmd = [ffmpeg, "-y", "-i", str(src), *codec_args, str(tmp_path)]
    try:
        result = subprocess.run(cmd, capture_output=True)
        if result.returncode != 0:
            err = result.stderr.decode("utf-8", errors="replace")
            print(f"変換失敗: {src}\n{err}", file=sys.stderr)
            tmp_path.unlink(missing_ok=True)
            return False
        if src.resolve() == dst.resolve():
            src.unlink()
        elif src.exists() and src.suffix.lower() != dst.suffix.lower():
            src.unlink()
        tmp_path.replace(dst)
        print(f"{src.name} -> {dst.name}")
        return True
    except Exception:
        tmp_path.unlink(missing_ok=True)
        raise


def unify_score(ffmpeg):
    args = ["-ar", "48000", "-ac", "2", "-c:a", "libmp3lame", "-b:a", "160k"]
    ok = True
    for src in iter_audio(SCORE_DIR):
        dst = src.with_suffix(".mp3")
        if not convert(ffmpeg, src, dst, args):
            ok = False
    return ok


def unify_se(ffmpeg):
    args = ["-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le"]
    ok = True
    for src in iter_audio(SE_DIR):
        dst = src.with_suffix(".wav")
        if not convert(ffmpeg, src, dst, args):
            ok = False
    return ok


def update_score_json():
    if not SCORE_DIR.is_dir():
        return
    for path in sorted(SCORE_DIR.glob("*.json")):
        text = path.read_text(encoding="utf-8")
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            print(f"JSON 読み込み失敗: {path} ({exc})", file=sys.stderr)
            continue
        music = data.get("music")
        if not isinstance(music, str):
            continue
        music_path = Path(music)
        if music_path.suffix.lower() not in SOURCE_EXTS:
            continue
        new_music = music_path.with_suffix(".mp3").as_posix()
        if new_music == music.replace("\\", "/"):
            continue
        updated = text.replace(f'"{music}"', f'"{new_music}"', 1)
        path.write_text(updated, encoding="utf-8", newline="\n")
        print(f"{path.name}: music {music} -> {new_music}")


def main():
    ffmpeg = require_ffmpeg()
    score_ok = unify_score(ffmpeg)
    se_ok = unify_se(ffmpeg)
    update_score_json()
    if not score_ok or not se_ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
