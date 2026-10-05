"""Reviewed public source allowlist, excluding local history and private materials."""
from pathlib import Path

TOP_LEVEL = ('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md',
             'CONTRIBUTING.md', '.gitignore', 'platformio.ini', 'Build-Keyboard.ps1',
             'Test-Keyboard.ps1', 'Package-Keyboard.ps1')
DIRECTORIES = ('src', 'include', 'test', 'tools', 'docs', 'assets', 'release', 'LICENSES', '.github')
EXTENSIONS = {'.h', '.cpp', '.py', '.ps1', '.md', '.json', '.txt', '.svg', '.yml', '.yaml'}

def public_files(root: Path):
    result = [root / name for name in TOP_LEVEL]
    for directory in DIRECTORIES:
        parent = root / directory
        if not parent.exists():
            continue
        if parent.is_symlink():
            raise ValueError('Symlinks are not allowed in public export.')
        for path in parent.rglob('*'):
            if path.is_symlink():
                raise ValueError('Symlinks are not allowed in public export.')
            if path.is_file() and path.suffix in EXTENSIONS and '__pycache__' not in path.parts:
                result.append(path)
    for path in result:
        if not path.is_file() or path.is_symlink():
            raise ValueError('Missing/invalid public file: ' + path.name)
    return sorted(result, key=lambda path: path.relative_to(root).as_posix())
