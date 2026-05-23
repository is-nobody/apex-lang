# source/libraries/os_module.py
import time
import os as _os  # используем псевдоним, чтобы не конфликтовать с именем класса
from source.libraries.builtin_module import BuiltinModule

class OS(BuiltinModule):
    def __init__(self):
        super().__init__("os")
        self._funcs = {
            # Ввод-вывод
            "output": self.output,
            "input": self.input,

            # Файловые операции
            "read": self.read,
            "write": self.write,
            "close": self.close,
            
            # Файловая система
            "exists": self.exists,
            "isfile": self.isfile,
            "isdir": self.isdir,
            "rename": self.rename,
            "rmfile": self.rmfile,
            "mkfile": self.mkfile,
            "listdir": self.listdir,

            # Директории
            "getcwd": self.getcwd,
            "chdir": self.chdir,
            "mkdir": self.mkdir,
            "rmdir": self.rmdir,

            # Метаданные
            "stat": self.stat,

            # Системное
            "exit": self.exit,
            "wait": self.wait,
            "time": self.time,
            "system": self.system,  # добавлен в _funcs
        }
        # Словарь для хранения открытых файлов (ключ — имя файла, значение — file object)
        self._open_files = {}

    # ========== Ввод-вывод ==========
    
    @staticmethod
    def output(value=None):
        if value is None:
            print("none")
        elif isinstance(value, bool):
            print("true" if value else "false")
        else:
            print(value)
    
    @staticmethod
    def input(prompt: str = ""):
        return input(prompt)

    # ========== Файловые операции ==========
    
    def read(self, filename: str):
        # Проверяем, открыт ли файл уже
        if filename in self._open_files:
            file_obj = self._open_files[filename]
            return file_obj.read()
        
        try:
            with open(filename, 'r', encoding='utf-8') as f:
                return f.read()
        except FileNotFoundError:
            return None
        except Exception:
            return None
    
    def write(self, filename: str, content: str) -> bool:
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write(str(content))
            return True
        except Exception:
            return False

    def close(self, filename: str = None):
        if filename is None:
            # Закрываем все открытые файлы
            for f in list(self._open_files.values()):
                try:
                    f.close()
                except Exception:
                    pass
            self._open_files.clear()
        elif filename in self._open_files:
            try:
                self._open_files[filename].close()
            except Exception:
                pass
            del self._open_files[filename]

    # ========== Файловая система ==========
    
    @staticmethod
    def exists(path: str) -> bool:
        return _os.path.exists(path)
    
    @staticmethod
    def isfile(path: str) -> bool:
        return _os.path.isfile(path)
    
    @staticmethod
    def isdir(path: str) -> bool:
        return _os.path.isdir(path)
    
    @staticmethod
    def rename(old_name: str, new_name: str) -> bool:
        try:
            _os.rename(old_name, new_name)
            return True
        except Exception:
            return False
    
    @staticmethod
    def rmfile(path: str) -> bool:
        try:
            _os.remove(path)
            return True
        except FileNotFoundError:
            return False
        except Exception:
            return False
    
    @staticmethod
    def mkfile(filename: str) -> bool:
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                pass
            return True
        except Exception:
            return False
    
    @staticmethod
    def listdir(path: str = ".") -> list:
        try:
            return _os.listdir(path)
        except Exception:
            return []

    # ========== Директории ==========
    
    @staticmethod
    def getcwd() -> str:
        return _os.getcwd()
    
    @staticmethod
    def chdir(path: str) -> bool:
        try:
            _os.chdir(path)
            return True
        except Exception:
            return False
    
    @staticmethod
    def mkdir(path: str) -> bool:
        try:
            _os.mkdir(path)
            return True
        except FileExistsError:
            return False
        except Exception:
            return False
    
    @staticmethod
    def rmdir(path: str) -> bool:
        try:
            _os.rmdir(path)
            return True
        except Exception:
            return False

    # ========== Метаданные ==========

    @staticmethod
    def stat(path: str):
        try:
            st = _os.stat(path)
            from source.core.interpreter import Table
            result = Table()
            result.set("size", st.st_size)
            result.set("mtime", st.st_mtime)
            result.set("ctime", st.st_ctime)
            result.set("isdir", _os.path.isdir(path))
            return result
        except Exception:
            return None

    # ========== Системное ==========
    
    @staticmethod
    def exit(code: int = 0):
        import sys
        sys.exit(code)
    
    @staticmethod
    def wait(seconds: float):
        if seconds < 0:
            seconds = 0
        time.sleep(seconds)
    
    @staticmethod
    def time() -> float:
        return time.time()
    
    @staticmethod
    def system(cmd: str) -> int:
        return _os.system(cmd)