# Modem emulation on Debian 12 (qico, PeerHayes, mgetty, tty0tty)

This is the Linux equivalent of a Hayes modem on a null-modem cable.
qico talks AT commands to a serial device. PeerHayes is that modem.
tty0tty is the cable. mgetty picks up the phone and starts qico.

The phone line between two modems is TCP. A dial string such as
`ATDT123123` is a number in the PeerHayes config, and that number is
`host:port` of the other PeerHayes.

Paths used by the units:

| Role | Path |
|------|------|
| qico | `/home/map/ftn/usr/bin/qico` |
| qico config | `/home/map/ftn/usr/etc/qico/qico.conf` |
| spool | `/home/map/ftn/fido` |
| peerhayesd | `/home/map/ftn/usr/bin/peerhayesd` |
| PeerHayes config | `/home/map/ftn/usr/etc/qico/peerhayes.conf` |

For the emulation, enable `qico-modem-emulation.service`. It starts tty0tty and
PeerHayes. For a hardware modem, enable `qico-mgetty-serial.service`
instead.

`peerhayesd` is built from this repository. The systemd units and the
mgetty snippets named below are in this repository, directory
`systemd/`. The `cp` commands are run from the repository root.

## How the pieces sit

```text
qico or mgetty                         peerhayesd
on /dev/tnt0  <---- tty0tty ---->  on /dev/tnt1
   DTE              null-modem          modem
                                      |
                                      TCP  (the telephone network)
                                      |
                                 other peerhayesd
```

tty0tty is a kernel module. It creates four pairs:

| Pair | Use on one machine |
|------|--------------------|
| `/dev/tnt0` ↔ `/dev/tnt1` | answer: mgetty/qico on `tnt0`, peerhayesd on `tnt1` |
| `/dev/tnt2` ↔ `/dev/tnt3` | dial: qico on `tnt2`, peerhayesd on `tnt3` |
| `/dev/tnt4` ↔ `/dev/tnt5` | spare |
| `/dev/tnt6` ↔ `/dev/tnt7` | spare |

TX and RX are crossed. RTS is CTS on the other end. DTR on one end is
both DSR and DCD on the other. `TIOCMGET` / `TIOCMSET` work, so qico's
carrier check is a real modem line. A PTY from socat only copies bytes.
On a PTY those ioctls return `ENOTTY`, qico clears `CLOCAL`, and the
read blocks. Do not put a preload between qico and the port.

The `pts/` tree in the tty0tty source is that byte-only PTY pair. Do
not build or use it for this setup.

PeerHayes raises DTR only while the call is up (`&C1` behaviour).
Before answer, and after `ATH`, DCD and DSR are both off. mgetty
therefore logs `DSR is off` at startup. That is the cable, not a
broken modem.

## What a call does

1. The caller's qico sends `ATDT123123` to its PeerHayes.
2. That PeerHayes opens TCP to the number's `host:port` and speaks
   VMCP/1 until CONNECT. After CONNECT the socket is a raw byte pipe.
3. The answering PeerHayes raises DTR (so the other end sees DCD) and
   sends `RING`.
4. mgetty, which already holds `/dev/tnt0`, sends `ATA`.
5. Both modems print `CONNECT 57600`. qico now requires DCD. Dropping
   DTR drops DCD and ends the session.
6. mgetty sends `EMSI_REQ` and waits for `EMSI_INQ` (`fido-send-emsi`).
   `login.config` matches `/FIDO/` and execs `qico -a emsi` as `map`.
7. `qico -a emsi` does not open `port` and does not send `ATA` or
   `EMSI_REQ` again. Its stdin is the line mgetty already answered.
8. With `protorder H` the session is ordinary Hydra (not Hydra
   4k/8k/16k). qico exits 8 (`S_HOLDR`) when the session completed and
   the systems exchanged hold requests. That exit is success.

## Packages

```bash
sudo apt-get install build-essential cmake git \
    linux-headers-$(uname -r) mgetty
```

`linux-headers-$(uname -r)` must match `uname -r`. Debian's mgetty
1.2.1 is built with FIDO. Without that, `fido-send-emsi` is ignored
and the `/FIDO/` line in `login.config` never runs.

`map` has to be able to open the PeerHayes end of the pair:

```bash
sudo usermod -aG dialout map
```

## 1. tty0tty

```bash
git clone --depth 1 https://github.com/lcgamboa/tty0tty.git
cd tty0tty/module
make -C /lib/modules/$(uname -r)/build M="$PWD" modules
sudo make -C /lib/modules/$(uname -r)/build M="$PWD" modules_install
sudo depmod -a
sudo modprobe tty0tty
ls -l /dev/tnt0 /dev/tnt1
```

Rebuild and reinstall the module after every kernel upgrade, then
`modprobe` again. The module is unsigned. Loading it needs a kernel
that does not require signed modules.

`qico-tty0tty.service` is this `modprobe` plus a check that `tnt0` and
`tnt1` exist. The device nodes are expected in group `dialout`.

## 2. PeerHayes

From the root of this repository:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo install -m 755 build/peerhayesd /home/map/ftn/usr/bin/peerhayesd
```

Answering modem, `/home/map/ftn/usr/etc/qico/peerhayes.conf`
(`systemd/peerhayes-vmodem.conf` is this file):

```ini
[modem]
name = node-910
serial = /dev/tnt1
baudrate = 57600
modem_speed = 57600
auto_answer = 0

[network]
listen = 0.0.0.0:5000
```

`auto_answer` stays 0. mgetty sends `ATA`. `S0=1` would answer before
mgetty and skip the Fido login. There is no `carrier_file`. Carrier
is DTR on `tnt1`, which tty0tty presents as DCD on `tnt0`.

`listen` is the TCP port the other modem dials. Open it in the
firewall if the caller is on another host.

## 3. qico

On the answering node the polling daemon must not also open `/dev/tnt0`.
mgetty owns that device. Inbound mail arrives through mgetty. Outbound
modem calls need the line free, or a second pair (`tnt2` / `tnt3`)
with a second peerhayesd.

Settings that matter for this modem (the rest of `qico.conf` is the
normal node config):

```text
protorder       H
port            /dev/tnt0:57600
modemcheckdsr   no
modemconnect    CONNECT
modemok         OK
modemerror      ERROR
modemerror      NO CARRIER
modemerror      NO ANSWER
modembusy       BUSY
modemnodial     NO DIALTONE
modemringing    RINGING
modemreset      ATZ|
modemalive      AT|
modemhangup     ~~+++~~ATH0|
dialprefix      ATDT
dialsuffix      |
lockdir         /var/lock
phone           123123
speed           57600
```

`protorder H` is classic Hydra. Do not build qico with Hydra 4k/8k/16k
for this line.

`modemcheckdsr no` is required on tty0tty. DSR is off until the peer
raises DTR, which is too late for a check that runs before dial.

`lockdir` must be the directory where mgetty writes `LCK..tnt0`. On
Debian that is `/var/lock`. A private lock directory makes an outbound
qico dial a line mgetty is already holding.

`qico -a emsi` ignores `port` and uses stdin. The `port` line is still
what an outbound `qico -c` opens, and it is the speed the node
publishes.

## 4. mgetty

Copy `systemd/mgetty.login.config` over `/etc/mgetty/login.config`:

```text
/FIDO/	map	map	/home/map/ftn/usr/bin/qico -I /home/map/ftn/usr/etc/qico/qico.conf -a emsi
*	-	-	/usr/sbin/nologin
```

The tabs are significant. `/FIDO/` is mgetty's Fido match. The name it
logs is `**EMSI_INQC816`. The command runs as `map`. Anything else
gets `nologin`, so a stray caller is not dropped into a shell.

Paste `systemd/mgetty.port-tnt0` into `/etc/mgetty/mgetty.config`.
Replace an existing `port tnt0` block and leave every other port
alone:

```text
port tnt0
  speed 57600
  data-only y
  toggle-dtr y
  need-dsr n
  ignore-carrier n
  fido-send-emsi y
  issue-file /dev/null
  init-chat "" AT OK ATZ OK
```

`init-chat "" AT OK ATZ OK` is the whole modem setup. PeerHayes
answers `AT` and `ATZ` with `OK`. `ATZ` restores its defaults
(`S0=0`, verbal result codes, quiet off). Do not put `ATS0=0Q0V1` or
any other Hayes dialect in this string.

`need-dsr n` because DSR is tied to the peer's DTR and is off while
mgetty sends `AT`. `ignore-carrier n` so DCD is honoured after
CONNECT. `toggle-dtr y` lets mgetty drop a modem that stayed online.
`issue-file /dev/null` skips the login banner. `data-only y` skips
the fax probe.

Speed here, in PeerHayes (`baudrate` and `modem_speed`), and in
qico's `port` should be the same value. 57600 is what this node uses.

## Units

The emulation is three units. Enable `qico-modem-emulation.service`. It starts
the other two. A hardware modem is the fourth unit, on its own.

```text
emulation:
  qico-modem-emulation.service     mgetty /dev/tnt0
    starts qico-tty0tty.service      modprobe tty0tty
    starts qico-peerhayes.service    peerhayesd on /dev/tnt1

hardware modem:
  qico-mgetty-serial.service   mgetty /dev/ttyUSB0
```

| Unit | What it is | How it behaves |
|------|------------|----------------|
| `qico-tty0tty.service` | `modprobe tty0tty` | Oneshot. Creates `/dev/tnt*`. Stays active after `modprobe` returns. Pulled in by the emulation mgetty unit. |
| `qico-peerhayes.service` | `peerhayesd` as `map` | Opens `/dev/tnt1`, listens on TCP 5000. Restarts if the process dies. Stops when the tty0tty unit stops. |
| `qico-modem-emulation.service` | mgetty on `/dev/tnt0` | The unit to enable for emulation. Starts the module and PeerHayes first. mgetty is root and setuid's to `map`. After each call mgetty exits and `Restart=always` runs it again. Conflicts with a getty on `tnt0`. |
| `qico-mgetty-serial.service` | mgetty on `/dev/ttyUSB0` | The unit to enable for a hardware modem. Does not load tty0tty and does not start PeerHayes. Same restart behaviour. |

mgetty holds the device and the lock `LCK..<device>` in `/var/lock`
the whole time it waits for `RING`. An outbound qico cannot dial that
same device until mgetty has exited. One line is either answering
(mgetty running) or originating (mgetty stopped, then `qico -c`).

Enable either `qico-modem-emulation.service` or `qico-mgetty-serial.service`.
Not both. `login.config` is the same file for both.

## Run with the emulation

Install the three emulation units once:

```bash
sudo cp systemd/qico-tty0tty.service \
        systemd/qico-peerhayes.service \
        systemd/qico-modem-emulation.service \
        /etc/systemd/system/
sudo systemctl daemon-reload
```

Leave the hardware unit disabled. Then:

```bash
sudo systemctl disable --now qico-mgetty-serial.service
sudo systemctl enable --now qico-modem-emulation.service
```

`enable --now` on `qico-modem-emulation.service` is the whole start. systemd
brings the units up in this order:

1. `qico-tty0tty.service` loads the module and checks that `/dev/tnt0`
   and `/dev/tnt1` exist.
2. `qico-peerhayes.service` starts `peerhayesd` on `/dev/tnt1`.
3. `qico-modem-emulation.service` runs `mgetty /dev/tnt0`.

Check:

```bash
systemctl status qico-modem-emulation.service qico-peerhayes.service qico-tty0tty.service
journalctl -u qico-modem-emulation -u qico-peerhayes -f
ls -l /dev/tnt0 /dev/tnt1
ss -ltnp | grep 5000
```

mgetty's log is `/var/log/mgetty/`. `DSR is off` before the first `AT`
is expected. `TIOCMGET` must not return `ENOTTY`. PeerHayes must be
listening before a caller dials.

Stop the emulation:

```bash
sudo systemctl disable --now qico-modem-emulation.service
```

That stops mgetty, PeerHayes, and unloads nothing by itself: the
tty0tty unit stops because mgetty is bound to it. `modprobe -r tty0tty`
removes the devices after both ends have closed them.

qico.conf for this mode uses `port /dev/tnt0:57600` and
`modemcheckdsr no`. The mgetty port block is `systemd/mgetty.port-tnt0`
(`need-dsr n`).

## Run with a real modem

Do not load tty0tty and do not start peerhayesd. The modem is the
serial device, mgetty is the answer program, and qico is still what
`login.config` execs.

```bash
sudo systemctl disable --now qico-modem-emulation.service
sudo modprobe -r tty0tty
```

Point qico at the UART. `modemcheckdsr yes` matches a modem that
raises DSR while it is powered on:

```text
port            /dev/ttyUSB0:57600
modemcheckdsr   yes
lockdir         /var/lock
```

Paste `systemd/mgetty.port-ttyUSB0` into `/etc/mgetty/mgetty.config`
(`need-dsr y`). If the node is not `ttyUSB0`, change the name in that
block and in `qico-mgetty-serial.service` together. `login.config`
stays the `/FIDO/` line from the emulation setup.

```bash
sudo cp systemd/qico-mgetty-serial.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now qico-mgetty-serial.service
```

What comes up is only mgetty on `/dev/ttyUSB0`. It sends
`"" AT OK ATZ OK`, then waits for `RING`. On answer it runs the same
`qico -a emsi`.

```bash
systemctl status qico-mgetty-serial.service
journalctl -u qico-mgetty-serial -f
```

If the log stays on `DSR is off` and never sends `AT`, the cable does
not wire DSR. Set `need-dsr n` for that port. `ignore-carrier n`
stays: after CONNECT, loss of DCD ends the call.

Stop:

```bash
sudo systemctl disable --now qico-mgetty-serial.service
```

## Calling side

The caller can be another host, or the second tty0tty pair on the
same machine. Its qico uses `/dev/tnt2` (or a real serial device on
another host) and the same modem strings, including
`modemcheckdsr no` and `protorder H`. Its PeerHayes uses the other
end of that pair:

```ini
[modem]
name = point-910-2
serial = /dev/tnt3
baudrate = 57600
modem_speed = 57600
auto_answer = 0

[network]
listen = 0.0.0.0:5001

[dial]
123123 = 192.168.1.10:5000
```

`123123` must be the number qico dials (`phone` of the node, or the
nodelist phone). The value is `host:port` of the answering
PeerHayes. One outbound poll:

```bash
/home/map/ftn/usr/bin/qico -I /home/map/ftn/usr/etc/qico/qico.conf -cI 2:203/910
```

During the call, qico logs `CD is On` and the line status includes
DSR and DCD. After `ATH`, the next check is `CD is Off`. A session
that moved mail both ways logs `Hydra: Bi-directional mode session`
and `successful [ok]`. Exit code 8 is `S_HOLDR`, not a failure.
`DCD line still active` means carrier was still reported after hangup.
On tty0tty that warning means DTR was left up.

## A real modem

Start and stop are in "Run with a real modem" above. The hardware path
is `qico-mgetty-serial.service` on `/dev/ttyUSB0` (or `ttyS0` /
`ttyACM0`). tty0tty and peerhayesd stay off. `need-dsr y` is for a
modem that holds DSR while powered; the tty0tty port cannot use that.

## What not to use

- socat PTY pairs, or tty0tty's `pts/` directory. No modem lines.
- An `LD_PRELOAD` that invents `TIOCM_CAR`. The carrier line is DTR.
- `carrier_file` in the PeerHayes config. Nothing on this setup reads it.
- `ATS0=0Q0V1` (or any longer string) as mgetty's init chat.
- Hydra 4k/8k/16k. `protorder H` is the ordinary Hydra.
