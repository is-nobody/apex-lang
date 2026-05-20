import os
import sys
import platform
from pathlib import Path

def get_own_executable_path():
    # when bundled with pyinstaller, sys.executable points to the actual .exe
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).resolve()
    else:
        # otherwise we are just a script, so use argv[0] which is how we were invoked
        return Path(sys.argv[0]).resolve()

def force_add_to_path_current_session(bin_dir):
    bin_path = str(bin_dir)
    current_path = os.environ.get('PATH', '')
    
    # prepend so our version is found first, avoiding conflicts with other installs
    if bin_path not in current_path.split(os.pathsep):
        os.environ['PATH'] = f"{bin_path}{os.pathsep}{current_path}"
        # putenv ensures child processes we spawn also see the new path
        os.putenv('PATH', os.environ['PATH'])

def add_to_path_windows(bin_dir):
    try:
        import winreg
        
        bin_path = str(bin_dir)
        
        # user-level registry key avoids uac/admin prompts completely
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            "Environment",
            0,
            winreg.KEY_READ | winreg.KEY_WRITE
        )
        
        try:
            current_path, _ = winreg.QueryValueEx(key, "Path")
        except FileNotFoundError:
            current_path = ""
        
        # clean any old broken apex paths so they don't point to deleted folders
        paths = [p for p in current_path.split(os.pathsep) if 'apex' not in p.lower()]
        # prepend so this install takes priority over other directories on path
        if bin_path not in paths:
            paths.insert(0, bin_path)
        
        new_path = os.pathsep.join(paths)
        # reg_expand_sz allows %variables% in the path to be expanded automatically
        winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new_path)
        winreg.CloseKey(key)
        
        # broadcast WM_SETTINGCHANGE so open explorer/command windows pick up the new path
        import ctypes
        HWND_BROADCAST = 0xFFFF
        WM_SETTINGCHANGE = 0x001A
        
        ctypes.windll.user32.SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            "Environment",
            0x0002,
            5000,
            None
        )
        
        force_add_to_path_current_session(bin_dir)
        return True
    except:
        # fallback: if registry write fails (e.g. locked by policy), at least fix this session
        force_add_to_path_current_session(bin_dir)
        return True

def add_to_path_macos(bin_dir):
    bin_path = str(bin_dir)
    home = Path.home()
    
    # default to zsh on modern macos, but respect the user's actual shell choice
    shell = os.environ.get('SHELL', '/bin/zsh')
    
    if 'zsh' in shell:
        configs = ['.zshrc']
    elif 'bash' in shell:
        # .bash_profile is sourced for login shells, .bashrc for interactive non-login
        configs = ['.bash_profile', '.bashrc']
    else:
        configs = ['.profile']
    
    for config in configs:
        config_path = home / config
        
        content = ""
        if config_path.exists():
            with open(config_path, 'r') as f:
                content = f.read()
        
        # strip old apex entries so path doesn't grow forever on repeated runs
        lines = [l for l in content.split('\n') if 'apex' not in l.lower()]
        
        # append at end; later entries in shell rc files override earlier ones when prepending
        lines.append(f'\n# Apex PATH\nexport PATH="{bin_path}:$PATH"')
        content = '\n'.join(lines)
        
        with open(config_path, 'w') as f:
            f.write(content)
    
    force_add_to_path_current_session(bin_dir)
    return True

def add_to_path_linux(bin_dir):
    bin_path = str(bin_dir)
    home = Path.home()
    
    # different shells use different rc files; detect from SHELL env var
    shell = os.environ.get('SHELL', '/bin/bash')
    
    if 'zsh' in shell:
        configs = ['.zshrc']
    elif 'bash' in shell:
        configs = ['.bashrc']
    elif 'fish' in shell:
        # fish uses a completely different syntax, stored in ~/.config/fish/config.fish
        configs = ['.config/fish/config.fish']
    else:
        # generic fallback for sh, dash, etc.
        configs = ['.profile', '.bashrc']
    
    for config in configs:
        config_path = home / config
        
        # fish config lives in a subdirectory that might not exist yet
        config_path.parent.mkdir(parents=True, exist_ok=True)
        
        content = ""
        if config_path.exists():
            with open(config_path, 'r') as f:
                content = f.read()
        
        # purge old apex lines to avoid duplicates if the install location changes
        lines = [l for l in content.split('\n') if 'apex' not in l.lower()]
        
        # fish uses "set -gx" instead of "export", otherwise it's a syntax error
        if 'fish' in str(config_path):
            lines.append(f'\n# Apex PATH\nset -gx PATH {bin_path} $PATH')
        else:
            lines.append(f'\n# Apex PATH\nexport PATH="{bin_path}:$PATH"')
        
        content = '\n'.join(lines)
        
        with open(config_path, 'w') as f:
            f.write(content)
    
    # ~/.local/bin is the user-local executable directory, standard on freedesktop distros
    local_bin = home / '.local' / 'bin'
    local_bin.mkdir(parents=True, exist_ok=True)
    
    # make sure ~/.local/bin itself is on the current path before we try to symlink into it
    if str(local_bin) not in os.environ.get('PATH', ''):
        os.environ['PATH'] = f"{local_bin}{os.pathsep}{os.environ.get('PATH', '')}"
    
    # symlink lets users just type "apex" without adding our bin_dir to path at all
    local_apex = local_bin / 'apex'
    executable = get_own_executable_path()
    
    try:
        if local_apex.exists() or local_apex.is_symlink():
            local_apex.unlink()
        local_apex.symlink_to(executable)
    except:
        pass  # if the user's filesystem doesn't support symlinks, the path export is enough
    
    force_add_to_path_current_session(bin_dir)
    
    # ensure ~/.local/bin stays on path for future sessions even if the user's distro doesn't include it
    local_bin_path = str(local_bin)
    for config in configs:
        config_path = home / config
        if config_path.exists():
            with open(config_path, 'r') as f:
                content = f.read()
            if local_bin_path not in content:
                with open(config_path, 'a') as f:
                    f.write(f'\nexport PATH="{local_bin_path}:$PATH"\n')
    
    return True

def path():
    executable = get_own_executable_path()
    # the directory containing the executable is what we want users to have on their path
    bin_dir = executable.parent
    
    system = platform.system()
    
    if system == "Windows":
        add_to_path_windows(bin_dir)
    elif system == "Darwin":
        add_to_path_macos(bin_dir)
    elif system == "Linux":
        add_to_path_linux(bin_dir)
    else:
        # unknown os: at least fix the current process so the command works right now
        force_add_to_path_current_session(bin_dir)
    
    # double-ensure the current session sees the directory, regardless of platform
    force_add_to_path_current_session(bin_dir)

if __name__ == "__main__":
    path()