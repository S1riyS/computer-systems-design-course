"""COM-порт: termios на macOS/Linux и Win32 API на Windows."""

import os
import threading

BAUD = 57600


class SerialPort:
    """Минимальный COM-порт: termios на macOS/Linux и Win32 API на Windows."""

    def __init__(self, path, baud=BAUD):
        self._fd = None
        self._handle = None
        self._kernel32 = None
        self._lock = threading.Lock()
        if os.name == "nt":
            self._open_windows(path, baud)
        else:
            self._open_posix(path, baud)

    def _open_posix(self, path, baud):
        import fcntl
        import termios

        speed = getattr(termios, "B%d" % baud, None)
        if speed is None:
            raise OSError("скорость %d не поддерживается" % baud)
        fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            attrs = termios.tcgetattr(fd)
            cflag = attrs[2] & ~termios.CSIZE
            cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD
            for name in ("PARENB", "PARODD", "CSTOPB", "CRTSCTS", "HUPCL"):
                cflag &= ~getattr(termios, name, 0)
            attrs[0] = 0
            attrs[1] = 0
            attrs[2] = cflag
            attrs[3] = 0
            attrs[4] = speed
            attrs[5] = speed
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            termios.tcflush(fd, termios.TCIOFLUSH)
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
        except Exception:
            os.close(fd)
            raise
        self._fd = fd

    def _open_windows(self, path, baud):
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        name = path
        suffix = path[3:] if path.upper().startswith("COM") else ""
        if suffix.isdigit() and int(suffix) >= 10:
            name = "\\\\.\\" + path

        class DCB(ctypes.Structure):
            _fields_ = [
                ("DCBlength", wintypes.DWORD),
                ("BaudRate", wintypes.DWORD),
                ("flags", wintypes.DWORD),
                ("wReserved", wintypes.WORD),
                ("XonLim", wintypes.WORD),
                ("XoffLim", wintypes.WORD),
                ("ByteSize", wintypes.BYTE),
                ("Parity", wintypes.BYTE),
                ("StopBits", wintypes.BYTE),
                ("XonChar", ctypes.c_char),
                ("XoffChar", ctypes.c_char),
                ("ErrorChar", ctypes.c_char),
                ("EofChar", ctypes.c_char),
                ("EvtChar", ctypes.c_char),
                ("wReserved1", wintypes.WORD),
            ]

        class COMMTIMEOUTS(ctypes.Structure):
            _fields_ = [
                ("ReadIntervalTimeout", wintypes.DWORD),
                ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
                ("ReadTotalTimeoutConstant", wintypes.DWORD),
                ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
                ("WriteTotalTimeoutConstant", wintypes.DWORD),
            ]

        kernel32.CreateFileW.argtypes = [
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
            wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        ]
        kernel32.CreateFileW.restype = ctypes.c_void_p
        kernel32.GetCommState.argtypes = [ctypes.c_void_p, ctypes.POINTER(DCB)]
        kernel32.GetCommState.restype = wintypes.BOOL
        kernel32.SetCommState.argtypes = [ctypes.c_void_p, ctypes.POINTER(DCB)]
        kernel32.SetCommState.restype = wintypes.BOOL
        kernel32.SetCommTimeouts.argtypes = [ctypes.c_void_p, ctypes.POINTER(COMMTIMEOUTS)]
        kernel32.SetCommTimeouts.restype = wintypes.BOOL
        kernel32.PurgeComm.argtypes = [ctypes.c_void_p, wintypes.DWORD]
        kernel32.PurgeComm.restype = wintypes.BOOL
        kernel32.ReadFile.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
        ]
        kernel32.ReadFile.restype = wintypes.BOOL
        kernel32.WriteFile.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
        ]
        kernel32.WriteFile.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        kernel32.CloseHandle.restype = wintypes.BOOL

        handle = kernel32.CreateFileW(
            name, 0x80000000 | 0x40000000, 0, None, 3, 0, None,
        )
        if not handle or handle == ctypes.c_void_p(-1).value:
            raise OSError(ctypes.get_last_error(), "не удалось открыть %s" % path)

        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not kernel32.GetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "GetCommState")
        dcb.BaudRate = baud
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        # fBinary, DTR enable, RTS enable, без аппаратного и программного потока.
        dcb.flags = 1 | (1 << 4) | (1 << 12)
        if not kernel32.SetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "SetCommState")
        timeouts = COMMTIMEOUTS(50, 0, 50, 0, 200)
        if not kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "SetCommTimeouts")
        kernel32.PurgeComm(handle, 0x000F)
        self._kernel32 = kernel32
        self._handle = handle

    def read(self, size, timeout=0.05):
        with self._lock:
            if self._fd is not None:
                import select
                ready, _, _ = select.select([self._fd], [], [], timeout)
                if not ready:
                    return b""
                return os.read(self._fd, size)
            if self._handle is None:
                raise OSError("порт уже закрыт")

            import ctypes
            from ctypes import wintypes

            buffer = ctypes.create_string_buffer(size)
            got = wintypes.DWORD(0)
            ok = self._kernel32.ReadFile(
                self._handle, buffer, size, ctypes.byref(got), None,
            )
            if not ok:
                raise OSError(ctypes.get_last_error(), "ReadFile")
            return buffer.raw[:got.value]

    def write(self, data):
        with self._lock:
            if self._fd is not None:
                view = memoryview(data)
                while view:
                    written = os.write(self._fd, view)
                    view = view[written:]
                return
            if self._handle is None:
                raise OSError("порт уже закрыт")

            import ctypes
            from ctypes import wintypes

            buffer = ctypes.create_string_buffer(bytes(data))
            got = wintypes.DWORD(0)
            ok = self._kernel32.WriteFile(
                self._handle, buffer, len(data), ctypes.byref(got), None,
            )
            if not ok or got.value != len(data):
                raise OSError(ctypes.get_last_error(), "WriteFile")

    def close(self):
        with self._lock:
            fd = self._fd
            self._fd = None
            if fd is not None:
                os.close(fd)
            handle = self._handle
            self._handle = None
            if handle is not None and self._kernel32 is not None:
                self._kernel32.CloseHandle(handle)
