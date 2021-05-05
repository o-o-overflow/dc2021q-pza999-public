# Automatically generated by configure - do not modify

all:
prefix=/usr/local
bindir=${prefix}/bin
libdir=${prefix}/lib
libexecdir=${prefix}/libexec
includedir=${prefix}/include
mandir=${prefix}/share/man
sysconfdir=${prefix}/etc
qemu_confdir=${prefix}/etc/qemu
qemu_datadir=${prefix}/share/qemu
qemu_firmwarepath=${prefix}/share/qemu-firmware
qemu_docdir=${prefix}/share/doc/qemu
qemu_moddir=${prefix}/lib/qemu
qemu_localstatedir=${prefix}/var
qemu_helperdir=${prefix}/libexec
qemu_localedir=${prefix}/share/locale
qemu_icondir=${prefix}/share/icons
qemu_desktopdir=${prefix}/share/applications
libs_cpu=-L$(BUILD_DIR)/capstone -lcapstone 
libs_softmmu=-lpixman-1 -lutil  
GIT=git
GIT_SUBMODULES=ui/keycodemapdb tests/fp/berkeley-testfloat-3 tests/fp/berkeley-softfloat-3 capstone slirp
GIT_UPDATE=yes
ARCH=x86_64
CONFIG_MINIKCONF_MODE=--defconfig
STRIP=strip
CONFIG_POSIX=y
CONFIG_LINUX=y
CONFIG_TOOLS=y
CONFIG_SLIRP=y
CONFIG_SMBD_COMMAND="/usr/sbin/smbd"
SLIRP_CFLAGS=-I$(SRC_PATH)/slirp/src -I$(BUILD_DIR)/slirp/src
SLIRP_LIBS=-L$(BUILD_DIR)/slirp -lslirp
config-host.h: slirp/all
CONFIG_L2TPV3=y
CONFIG_AUDIO_DRIVERS= oss
CONFIG_AUDIO_OSS=m
ALSA_LIBS=
PULSE_LIBS=
COREAUDIO_LIBS=
DSOUND_LIBS=
OSS_LIBS=
CONFIG_BDRV_RW_WHITELIST=
CONFIG_BDRV_RO_WHITELIST=
CONFIG_VNC=y
VERSION=5.0.1
PKGVERSION=
SRC_PATH=/home/nick/Projects/qemu
TARGET_DIRS=x86_64-softmmu
CONFIG_ICONV=y
ICONV_CFLAGS=-I/usr/local/include
ICONV_LIBS=-L/usr/local/lib 
CONFIG_CURSES=m
CURSES_CFLAGS=-DNCURSES_WIDECHAR -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
CURSES_LIBS=-Wl,-Bsymbolic-functions -lncursesw -ltinfo
CONFIG_PIPE2=y
CONFIG_ACCEPT4=y
CONFIG_SPLICE=y
CONFIG_EVENTFD=y
CONFIG_MEMFD=y
CONFIG_USBFS=y
CONFIG_FALLOCATE=y
CONFIG_FALLOCATE_PUNCH_HOLE=y
CONFIG_FALLOCATE_ZERO_RANGE=y
CONFIG_POSIX_FALLOCATE=y
CONFIG_SYNC_FILE_RANGE=y
CONFIG_FIEMAP=y
CONFIG_DUP3=y
CONFIG_PPOLL=y
CONFIG_PRCTL_PR_SET_TIMERSLACK=y
CONFIG_EPOLL=y
CONFIG_EPOLL_CREATE1=y
CONFIG_SENDFILE=y
CONFIG_TIMERFD=y
CONFIG_SETNS=y
CONFIG_CLOCK_ADJTIME=y
CONFIG_SYNCFS=y
CONFIG_INOTIFY=y
CONFIG_INOTIFY1=y
CONFIG_SEM_TIMEDWAIT=y
HAVE_STRCHRNUL=y
HAVE_STRUCT_STAT_ST_ATIM=y
CONFIG_BYTESWAP_H=y
CONFIG_GIO=y
GIO_CFLAGS=-pthread -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -pthread -I/usr/include/gio-unix-2.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
GIO_LIBS=-lgio-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0
GDBUS_CODEGEN=/usr/bin/gdbus-codegen
CONFIG_TLS_PRIORITY="NORMAL"
CONFIG_QEMU_PRIVATE_XTS=y
HAVE_IFADDRS_H=y
HAVE_FSXATTR=y
HAVE_COPY_FILE_RANGE=y
CONFIG_ATTR=y
CONFIG_VHOST_SCSI=y
CONFIG_VHOST_NET=y
CONFIG_VHOST_NET_USER=y
CONFIG_VHOST_CRYPTO=y
CONFIG_VHOST_VSOCK=y
CONFIG_VHOST_KERNEL=y
CONFIG_VHOST_USER=y
CONFIG_VHOST_USER_FS=y
INSTALL_BLOBS=yes
CONFIG_IOVEC=y
CONFIG_PREADV=y
CONFIG_SIGNALFD=y
CONFIG_TCG=y
CONFIG_FDATASYNC=y
CONFIG_MADVISE=y
CONFIG_POSIX_MADVISE=y
CONFIG_POSIX_MEMALIGN=y
CONFIG_MALLOC_TRIM=y
CONFIG_AVX2_OPT=y
CONFIG_QOM_CAST_DEBUG=y
CONFIG_COROUTINE_BACKEND=ucontext
CONFIG_COROUTINE_POOL=1
CONFIG_OPEN_BY_HANDLE=y
CONFIG_LINUX_MAGIC_H=y
CONFIG_PRAGMA_DIAGNOSTIC_AVAILABLE=y
CONFIG_HAS_ENVIRON=y
CONFIG_CPUID_H=y
CONFIG_INT128=y
CONFIG_CMPXCHG128=y
CONFIG_ATOMIC64=y
CONFIG_ATTRIBUTE_ALIAS=y
CONFIG_GETAUXVAL=y
CONFIG_LIVE_BLOCK_MIGRATION=y
CONFIG_TPM=y
TRACE_BACKENDS=log
CONFIG_TRACE_LOG=y
CONFIG_TRACE_FILE=trace
CONFIG_RTNETLINK=y
CONFIG_REPLICATION=y
CONFIG_AF_VSOCK=y
CONFIG_SYSMACROS=y
CONFIG_STATIC_ASSERT=y
HAVE_UTMPX=y
CONFIG_GETRANDOM=y
CONFIG_IVSHMEM=y
CONFIG_CAPSTONE=y
CONFIG_THREAD_SETNAME_BYTHREAD=y
CONFIG_PTHREAD_SETNAME_NP_W_TID=y
CONFIG_BOCHS=y
CONFIG_CLOOP=y
CONFIG_DMG=y
CONFIG_QCOW1=y
CONFIG_VDI=y
CONFIG_VVFAT=y
CONFIG_QED=y
CONFIG_PARALLELS=y
CONFIG_SHEEPDOG=y
HAVE_GDB_BIN=/usr/bin/gdb
TOOLS=qemu-ga$(EXESUF) ivshmem-client$(EXESUF) ivshmem-server$(EXESUF) qemu-nbd$(EXESUF) qemu-storage-daemon$(EXESUF) qemu-img$(EXESUF) qemu-io$(EXESUF) qemu-edid$(EXESUF)  scsi/qemu-pr-helper$(EXESUF)
ROMS=optionrom
MAKE=make
INSTALL=install
INSTALL_DIR=install -d -m 0755
INSTALL_DATA=install -c -m 0644
INSTALL_PROG=install -c -m 0755
INSTALL_LIB=install -c -m 0644
PYTHON=/usr/bin/python3 -B
SPHINX_BUILD=
SPHINX_WERROR=-W
GENISOIMAGE=/usr/bin/genisoimage
CC=cc
HOST_CC=cc
CXX=
OBJCC=cc
AR=ar
ARFLAGS=rv
AS=as
CCAS=cc
CPP=cc -E
OBJCOPY=objcopy
LD=ld
RANLIB=ranlib
NM=nm
PKG_CONFIG=pkg-config
WINDRES=windres
CFLAGS=-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -g 
CFLAGS_NOPIE=-fno-pie
QEMU_CFLAGS=-I/usr/include/pixman-1  -Werror  -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -fPIE -DPIE -m64 -mcx16 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -Wstrict-prototypes -Wredundant-decls -Wall -Wundef -Wwrite-strings -Wmissing-prototypes -fno-strict-aliasing -fno-common -fwrapv -std=gnu99  -Wexpansion-to-defined -Wendif-labels -Wno-shift-negative-value -Wno-missing-include-dirs -Wempty-body -Wnested-externs -Wformat-security -Wformat-y2k -Winit-self -Wignored-qualifiers -Wold-style-declaration -Wold-style-definition -Wtype-limits -fstack-protector-strong -I$(SRC_PATH)/capstone/include
QEMU_CXXFLAGS=
QEMU_INCLUDES=-iquote $(SRC_PATH)/tcg/i386 -isystem $(SRC_PATH)/linux-headers -isystem /home/nick/Projects/qemu/bin/debug/nic/linux-headers -iquote . -iquote $(SRC_PATH) -iquote $(SRC_PATH)/accel/tcg -iquote $(SRC_PATH)/include -iquote $(SRC_PATH)/disas/libvixl
QEMU_LDFLAGS= -Wl,--warn-common -Wl,-z,relro -Wl,-z,now -pie -m64  -fstack-protector-strong
LDFLAGS_NOPIE=-no-pie
LD_REL_FLAGS=-r
LD_I386_EMULATION=elf_i386
LIBS+=-lm -lz -lgthread-2.0 -pthread -lglib-2.0  -lrt
LIBS_TOOLS+=-lutil 
PTHREAD_LIB=
EXESUF=
DSOSUF=.so
LDFLAGS_SHARED=-shared
LIBS_QGA+=-lm -lgthread-2.0 -pthread -lglib-2.0  -lrt
TASN1_LIBS=
TASN1_CFLAGS=
POD2MAN=pod2man --utf8
DECOMPRESS_EDK2_BLOBS=y
x86_64-softmmu/config-devices.mak: CONFIG_XEN=n
x86_64-softmmu/config-devices.mak: CONFIG_KVM=y
PIXMAN_CFLAGS=-I/usr/include/pixman-1
PIXMAN_LIBS=-lpixman-1
config-host.h: capstone/all
LIBCAPSTONE=libcapstone.a
