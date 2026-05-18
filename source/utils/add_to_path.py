# source/utils/add_to_path.py
import os
import sys
import platform
from pathlib import Path

def get_script_path():
    if len(sys.argv) > 1:
        script_path = Path(sys.argv[1]).resolve()
    else:
        script_path = Path(__file__).resolve()
    return script_path

def add_to_path_windows(script_path):
    try:
        import winreg
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment", 0, winreg.KEY_READ | winreg.KEY_WRITE)
        try:
            current_path, _ = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            current_path = ""
        script_dir = str(script_path.parent)
        if script_dir not in current_path.split(os.pathsep):
            new_path = current_path + os.pathsep + script_dir if current_path else script_dir
            winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
            winreg.CloseKey(key)
            import ctypes
            HWND_BROADCAST = 0xFFFF
            WM_SETTINGCHANGE = 0x001A
            ctypes.windll.user32.SendMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment")
    except Exception:
        pass

def add_to_path_unix(script_path, shell_config):
    script_dir = str(script_path.parent)
    home_dir = Path.home()
    config_path = home_dir / shell_config
    export_line = f'\nexport PATH="{script_dir}:$PATH"\n'
    if config_path.exists():
        with open(config_path, 'r') as f:
            content = f.read()
            if script_dir in content:
                return
    try:
        with open(config_path, 'a') as f:
            f.write(f"\n# Added by apex PATH installer\n{export_line}")
    except Exception:
        pass

def add_to_path_macos(script_path):
    shell = os.environ.get('SHELL', '')
    if 'zsh' in shell:
        add_to_path_unix(script_path, '.zshrc')
    elif 'bash' in shell:
        add_to_path_unix(script_path, '.bash_profile')
    else:
        add_to_path_unix(script_path, '.zshrc')
        add_to_path_unix(script_path, '.bash_profile')

def add_to_path_linux(script_path):
    shell = os.environ.get('SHELL', '')
    if 'zsh' in shell:
        add_to_path_unix(script_path, '.zshrc')
    elif 'bash' in shell:
        add_to_path_unix(script_path, '.bashrc')
    elif 'fish' in shell:
        fish_config = Path.home() / '.config/fish/config.fish'
        script_dir = str(script_path.parent)
        fish_line = f'\nset PATH {script_dir} $PATH\n'
        try:
            with open(fish_config, 'a') as f:
                f.write(fish_line)
        except Exception:
            pass
    else:
        add_to_path_unix(script_path, '.bashrc')

def path():
    script_path = get_script_path()
    if not script_path.exists():
        sys.exit(1)
    system = platform.system()
    if system == "Windows":
        add_to_path_windows(script_path)
    elif system == "Darwin":
        add_to_path_macos(script_path)
    elif system == "Linux":
        add_to_path_linux(script_path)
    else:
        sys.exit(1)

if __name__ == "__main__":
    path()