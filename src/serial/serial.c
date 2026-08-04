/*
 * PeerHayes — Win32/POSIX serial, named pipe, and stdio DTE backends
 *
 * Copyright (c) 2026 Alexey Matrosov <2:203/910>
 * SPDX-License-Identifier: MIT
 */

#include "vmodem/serial.h"
#include "vmodem/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define VMOD_STRICMP _stricmp
#else
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#define VMOD_STRICMP strcasecmp
#endif

struct vmod_serial {
  vmod_serial_kind_t kind;
#ifdef _WIN32
  HANDLE handle;
  int is_stdio;
#else
  int fd;
  int is_stdio;
#endif
};

static int parse_baud(int baud) {
#ifdef _WIN32
  switch (baud) {
  case 300:
    return CBR_300;
  case 1200:
    return CBR_1200;
  case 2400:
    return CBR_2400;
  case 4800:
    return CBR_4800;
  case 9600:
    return CBR_9600;
  case 19200:
    return CBR_19200;
  case 38400:
    return CBR_38400;
  case 57600:
    return CBR_57600;
  case 115200:
    return CBR_115200;
  default:
    return CBR_9600;
  }
#else
  switch (baud) {
  case 300:
    return B300;
  case 1200:
    return B1200;
  case 2400:
    return B2400;
  case 4800:
    return B4800;
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  default:
    return B9600;
  }
#endif
}

#ifdef _WIN32
static void win_path(const char *path, char *out, size_t out_sz) {
  if (VMOD_STRICMP(path, "COM") != 0 &&
      (path[0] == 'C' || path[0] == 'c') &&
      (path[1] == 'O' || path[1] == 'o') &&
      (path[2] == 'M' || path[2] == 'm') && path[3] >= '1' && path[3] <= '9') {
    snprintf(out, out_sz, "\\\\.\\%s", path);
  } else {
    snprintf(out, out_sz, "%s", path);
  }
}

static int configure_com(HANDLE h, int baudrate) {
  DCB dcb;
  COMMTIMEOUTS timeouts;

  memset(&dcb, 0, sizeof(dcb));
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb))
    return -1;
  dcb.BaudRate = (DWORD)parse_baud(baudrate);
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  /* DTR starts off; we raise it for carrier (com0com maps DTR→peer DCD). */
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  if (!SetCommState(h, &dcb))
    return -1;

  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 2000;
  if (!SetCommTimeouts(h, &timeouts))
    return -1;
  return 0;
}
#endif

vmod_status_t vmod_serial_open(vmod_serial_t **out, const char *path,
                               int baudrate) {
  vmod_serial_t *s;

  if (!out || !path)
    return VMOD_ERR_INVAL;
  s = (vmod_serial_t *)calloc(1, sizeof(*s));
  if (!s)
    return VMOD_ERR_NOMEM;

  if (VMOD_STRICMP(path, "stdio") == 0 || strcmp(path, "-") == 0) {
    s->kind = VMOD_SERIAL_STDIO;
    s->is_stdio = 1;
#ifdef _WIN32
    s->handle = INVALID_HANDLE_VALUE;
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
    s->fd = STDIN_FILENO;
    {
      int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
      if (flags >= 0)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
#endif
    *out = s;
    VMOD_LOGI("DTE: stdio");
    return VMOD_OK;
  }

#ifdef _WIN32
  {
    char full[512];
    DWORD access = GENERIC_READ | GENERIC_WRITE;

    win_path(path, full, sizeof(full));
    if (_strnicmp(path, "pipe:", 5) == 0 || strstr(full, "\\\\.\\pipe\\") ||
        strstr(full, "\\\\.\\PIPE\\")) {
      char pipe_path[512];
      int attempt;
      s->kind = VMOD_SERIAL_PIPE;
      if (_strnicmp(path, "pipe:", 5) == 0)
        snprintf(pipe_path, sizeof(pipe_path), "\\\\.\\pipe\\%s", path + 5);
      else
        snprintf(pipe_path, sizeof(pipe_path), "%s", full);

      s->handle = INVALID_HANDLE_VALUE;
      for (attempt = 0; attempt < 50; attempt++) {
        s->handle = CreateFileA(pipe_path, access, 0, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
        if (s->handle != INVALID_HANDLE_VALUE)
          break;
        if (GetLastError() != ERROR_PIPE_BUSY &&
            GetLastError() != ERROR_FILE_NOT_FOUND)
          break;
        Sleep(100);
      }
      if (s->handle == INVALID_HANDLE_VALUE) {
        VMOD_LOGE("pipe open failed: %s (%lu)", pipe_path,
                  (unsigned long)GetLastError());
        free(s);
        return VMOD_ERR_IO;
      }
      {
        DWORD mode = PIPE_READMODE_BYTE;
        SetNamedPipeHandleState(s->handle, &mode, NULL, NULL);
      }
      VMOD_LOGI("DTE: pipe %s", pipe_path);
    } else {
      s->kind = VMOD_SERIAL_COM;
      s->handle = CreateFileA(full, access, 0, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
      if (s->handle == INVALID_HANDLE_VALUE) {
        VMOD_LOGE("COM open failed: %s (%lu)", full,
                  (unsigned long)GetLastError());
        free(s);
        return VMOD_ERR_IO;
      }
      if (configure_com(s->handle, baudrate) != 0) {
        VMOD_LOGE("COM configure failed: %s", full);
        CloseHandle(s->handle);
        free(s);
        return VMOD_ERR_IO;
      }
      VMOD_LOGI("DTE: %s @ %d", full, baudrate);
    }
  }
#else
  s->kind = VMOD_SERIAL_COM;
  s->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (s->fd < 0) {
    VMOD_LOGE("serial open failed: %s", path);
    free(s);
    return VMOD_ERR_IO;
  }
  {
    struct termios tio;
    speed_t spd = (speed_t)parse_baud(baudrate);
    if (tcgetattr(s->fd, &tio) == 0) {
      cfmakeraw(&tio);
      cfsetispeed(&tio, spd);
      cfsetospeed(&tio, spd);
      tio.c_cflag |= (CLOCAL | CREAD);
      tcsetattr(s->fd, TCSANOW, &tio);
    }
  }
  VMOD_LOGI("DTE: %s @ %d", path, baudrate);
#endif

  *out = s;
  return VMOD_OK;
}

void vmod_serial_close(vmod_serial_t *s) {
  if (!s)
    return;
#ifdef _WIN32
  if (!s->is_stdio && s->handle != INVALID_HANDLE_VALUE)
    CloseHandle(s->handle);
#else
  if (!s->is_stdio && s->fd >= 0)
    close(s->fd);
#endif
  free(s);
}

int vmod_serial_read(vmod_serial_t *s, uint8_t *buf, size_t buflen) {
  if (!s || !buf || buflen == 0)
    return 0;

#ifdef _WIN32
  if (s->is_stdio) {
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD n = 0;
    if (WaitForSingleObject(hin, 0) != WAIT_OBJECT_0)
      return 0;
    if (!ReadFile(hin, buf, (DWORD)buflen, &n, NULL))
      return 0;
    return (int)n;
  } else {
    DWORD n = 0;
    if (!ReadFile(s->handle, buf, (DWORD)buflen, &n, NULL)) {
      DWORD err = GetLastError();
      if (err == ERROR_BROKEN_PIPE)
        return -1;
      return 0;
    }
    return (int)n;
  }
#else
  {
    ssize_t n = read(s->is_stdio ? STDIN_FILENO : s->fd, buf, buflen);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return 0;
      return -1;
    }
    return (int)n;
  }
#endif
}

int vmod_serial_write(vmod_serial_t *s, const uint8_t *buf, size_t len) {
  size_t off = 0;
  if (!s || !buf)
    return -1;

#ifdef _WIN32
  if (s->is_stdio) {
    DWORD n = 0;
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    while (off < len) {
      if (!WriteFile(hout, buf + off, (DWORD)(len - off), &n, NULL))
        return -1;
      off += n;
    }
    fflush(stdout);
    return 0;
  }
  while (off < len) {
    DWORD n = 0;
    if (!WriteFile(s->handle, buf + off, (DWORD)(len - off), &n, NULL))
      return -1;
    if (n == 0)
      return -1;
    off += n;
  }
  return 0;
#else
  while (off < len) {
    ssize_t n =
        write(s->is_stdio ? STDOUT_FILENO : s->fd, buf + off, len - off);
    if (n < 0) {
      if (errno == EAGAIN)
        continue;
      return -1;
    }
    off += (size_t)n;
  }
  return 0;
#endif
}

vmod_serial_kind_t vmod_serial_kind(const vmod_serial_t *s) {
  return s ? s->kind : VMOD_SERIAL_COM;
}

void vmod_serial_set_dtr(vmod_serial_t *s, int on) {
  if (!s || s->is_stdio)
    return;
#ifdef _WIN32
  if (s->handle == INVALID_HANDLE_VALUE)
    return;
  EscapeCommFunction(s->handle, on ? SETDTR : CLRDTR);
  VMOD_LOGD("DTR %s", on ? "ON (carrier)" : "OFF");
#else
  if (s->fd < 0)
    return;
  {
    int status = 0;
    if (ioctl(s->fd, TIOCMGET, &status) == 0) {
      if (on)
        status |= TIOCM_DTR;
      else
        status &= ~TIOCM_DTR;
      ioctl(s->fd, TIOCMSET, &status);
    }
  }
#endif
}
