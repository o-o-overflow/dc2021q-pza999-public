/*
 * PZA 999 emulation
 *
 * Defcon Ctf Quals 2021
 */

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "net/net.h"

#define TYPE_PZA999 "pza999"

enum {
    DEBUG_RXINFO, DEBUG_TXINFO
};

#define DBGBIT(x) (1<<DEBUG_##x)
//static int debugflags = DBGBIT(RXINFO) | DBGBIT(TXINFO);
//static int debugflags = 0;

#define DBGOUT(what, fmt, ...) do { \
    if (debugflags & DBGBIT(what)) \
        fprintf(stderr, "PZA: " fmt, ## __VA_ARGS__); \
    } while (0)

#define PZA999(obj) \
    OBJECT_CHECK(PZA999State, (obj), TYPE_PZA999)

#define IOPORT_SIZE 0x40
#define TX_MMIO_SIZE 0x1000
#define RX_MMIO_SIZE 0x1000

// IRQ enablement bits
#define RXIRQ 1

// Descriptor status bits
#define RXPOPD 1
#define TXPOPD 2
#define RXEOPK 4

#define MAC_ADDR1 0
#define MAC_ADDR3 1
#define LNKSTATUS 2 // link status
#define IRSN      3 // interrupt reason
#define IMSK      4 // interrupt mask
#define CTRL_LAST 5
#define NCTRLREGS CTRL_LAST

#define TXDAL 0 // Tx Desc Address Lo
#define TXDAH 1 // Tx Desc Address Hi
#define TXCNT 2 // Tx Desc Length
#define TXHDP 3 // Tx Head Pointer
#define TXTLP 4 // Tx Tail Pointer
#define TXLST 5 // last entry, not a valid entry
#define NTXREG TXLST

#define RXDAL 0 // Rx Desc Address Lo
#define RXDAH 1 // Rx Desc Address Hi
#define RXCNT 2 // Rx Desc Length
#define RXHDP 3 // Rx Head Pointer
#define RXTLP 4 // Rx Tail Pointer
#define RXLST 5
#define NRXREG RXLST

struct descriptor {
    dma_addr_t base;
    uint32_t len;
    uint8_t flags;
    uint8_t pad[3];
};

typedef struct PZA999State_st {
    PCIDevice parent;

    NICState *nic;
    NICConf conf;

    MemoryRegion io;
    MemoryRegion tx_mmio;
    MemoryRegion rx_mmio;

    unsigned char tx_scratch[0x10000];

    uint32_t ctrl_reg[NCTRLREGS];
    uint32_t tx_reg[NTXREG];
    uint32_t rx_reg[NRXREG];
} PZA999State;


static uint32_t rx_readreg(PZA999State *s, int index)
{
    return s->rx_reg[index];
}

static uint32_t tx_readreg(PZA999State *s, int index)
{
    return s->tx_reg[index];
}

static void rx_writereg(PZA999State *s, int index, uint32_t val)
{
    s->rx_reg[index] = val;
}

static void tx_writereg(PZA999State *s, int index, uint32_t val)
{
    s->tx_reg[index] = val;
}

static dma_addr_t rx_desc_base(PZA999State *s)
{
    uint64_t h = s->rx_reg[RXDAH];

    return h | s->rx_reg[RXDAL];
}

static dma_addr_t tx_desc_base(PZA999State *s)
{
    uint64_t h = s->tx_reg[TXDAH];

    return h | s->tx_reg[TXDAL];
}

static void pza999_instance_init(Object *obj)
{
    PZA999State *p = PZA999(obj);

    memset(p->tx_reg, 0, NRXREG * sizeof(uint32_t));
    memset(p->rx_reg, 0, NTXREG * sizeof(uint32_t));
}


static Property pza999_properties[] = {
    DEFINE_NIC_PROPERTIES(PZA999State, conf),
    DEFINE_PROP_END_OF_LIST(),
};

typedef struct PZA999Info {
    const char *name;
    uint16_t device_id;
    uint8_t revision;
} PZA999Info;

static void pza999_set_link_status(NetClientState *nc)
{
    PZA999State *s = qemu_get_nic_opaque(nc);

    if (nc->link_down) { }
    else { }

    (void)s;
}

/* Host IO */
static void do_xmit(PZA999State *s) {
    PCIDevice *d = PCI_DEVICE(s);
    NetClientState *nc = qemu_get_queue(s->nic);
    dma_addr_t base;

    struct descriptor current;
    while (s->tx_reg[TXHDP] != s->tx_reg[TXTLP]) {
        base = tx_desc_base(s) + \
            s->tx_reg[TXHDP] * sizeof(struct descriptor);

        pci_dma_read(d, base, &current, sizeof(struct descriptor));

        size_t db = current.base;
        size_t dl = current.len;

        if (current.flags & TXPOPD) {
            // bad packet
            if (dl > sizeof(s->tx_scratch)) {
                return;
            }
            pci_dma_read(d, db, s->tx_scratch, dl);

            s->tx_reg[TXHDP]++;

            if (s->tx_reg[TXHDP] >= s->tx_reg[TXCNT])
                s->tx_reg[TXHDP] = 0;

            // remove flag
            current.flags = 0;
            pci_dma_write(d, base, &current, sizeof(struct descriptor));

            // send off packet
            qemu_send_packet(nc, s->tx_scratch, dl);

            // send tx interrupt

        } else {
            //printf("XMIT: Descriptors invalid HDP %d TLP %d CNT %d\n", s->tx_reg[TXHDP], s->tx_reg[TXTLP], s->tx_reg[TXCNT]);
            // no valid descriptors?
        }
    }
}


static bool pza999_can_receive(NetClientState *nc)
{
    PZA999State *s = qemu_get_nic_opaque(nc);

    // link status must be up
    // rx interrupts must be enabled
    // and we must be the bus master
    if (s->ctrl_reg[LNKSTATUS] == 0 ||
        //(s->ctrl_reg[IMSK] & RXIRQ) ||
        !(s->parent.config[PCI_COMMAND] & PCI_COMMAND_MASTER)) {
        return false;
    }

    // did the cpu give us some descriptors?
    return s->rx_reg[RXHDP] != s->rx_reg[RXTLP];
}

static ssize_t pza999_receive_iov(NetClientState *nc, const struct iovec *iov, int iovcnt)
{
    PZA999State *s = qemu_get_nic_opaque(nc);
    // needed for DMA
    PCIDevice *d = PCI_DEVICE(s);
    size_t size = iov_size(iov, iovcnt);
    size_t left = size;
    size_t iov_ofs = 0;
    size_t sent = 0;
    dma_addr_t base;

    // iterate over whole 'size' fitting in each iov into a descriptor
    struct descriptor current;
    while(sent < size) {
        base = rx_desc_base(s) + sizeof(struct descriptor) * s->rx_reg[RXHDP];
        pci_dma_read(d, base, &current, sizeof(struct descriptor));

        // TODO CTF-REMOVE if we run into a descriptor we've populated already that's an issue
        if (current.flags & RXPOPD) {
        }

        size_t db = current.base;
        size_t dl = current.len;
        size_t to_copy = 0;
        size_t working_size = left;
        // total size being requested for push is greater than current descriptor
        if (working_size > dl) {
            working_size = dl;
        }

        //DBGOUT(RXINFO, "RX %lx <- %lu\n", db, working_size);

        // now each iov we can fit
        size_t to_dma = working_size;
        do {
            // BUT the current iov may still be smaller
            // CTF-REMOVE interesting bug here, what we mess up this to_copy size calculation?
            to_copy = MIN(to_dma, iov->iov_len - iov_ofs);
            pci_dma_write(d, db, iov->iov_base + iov_ofs, to_copy);
            db += to_copy;
            iov_ofs += to_copy;
            to_dma -= to_copy;
            // if the entire iov was copied but still more left in working size
            if (iov_ofs == iov->iov_len) {
                iov++;
                iov_ofs = 0;
            }
        } while (to_dma);

        // DONE, those bytes are sent, record it and move onto the next descriptor
        sent += working_size;
        left -= working_size;

        // mark descriptor as populated
        current.flags |= RXPOPD;

        // if we've accounted for everything mark the packet as final
        if (sent >= size) {
            current.flags |= RXEOPK;
        }

        current.len = working_size;

        // TODO add an out length to the descriptor!
        pci_dma_write(d, base, &current, sizeof(struct descriptor));

        // handle wrap around
        s->rx_reg[RXHDP]++;
        if (s->rx_reg[RXHDP] == s->rx_reg[RXCNT]) {
            s->rx_reg[RXHDP] = 0;
        }
    }

    // now we interrupt telling the CPU we added packets
    s->ctrl_reg[IRSN] |= RXIRQ;
    bool to_interrupt = !(s->ctrl_reg[IMSK] & s->ctrl_reg[IRSN]);
    pci_set_irq(d, to_interrupt);

    return size;
}

static ssize_t pza999_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    const struct iovec iov = {
        .iov_base = (uint8_t *)buf,
        .iov_len = size
    };

    return pza999_receive_iov(nc, &iov, 1);
}

/* Device IO */
static uint64_t pza999_rx_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PZA999State *s = opaque;
    unsigned int index = (addr & 0x1ffff) >> 2;

    if (index < NRXREG) {
        return rx_readreg(s, index);
    }

    return 0;
}

/* configuration of the rx descriptor list and other rx attributes */
static void pza999_rx_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                 unsigned size)
{
    PZA999State *s = opaque;
    unsigned int index = (addr & 0x1ffff) >> 2;

    if (index < NRXREG) {
        rx_writereg(s, index, val);
    }

    return;
}

static const MemoryRegionOps pza999_rx_mmio_ops = {
    .read = pza999_rx_mmio_read,
    .write = pza999_rx_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t pza999_tx_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PZA999State *s = opaque;
    unsigned int index = (addr & 0x1ffff) >> 2;

    if (index < NTXREG) {
        return tx_readreg(s, index);
    }

    return 0;
}

static void pza999_tx_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                 unsigned size)
{
    PZA999State *s = opaque;
    unsigned int index = (addr & 0x1ffff) >> 2;

    if (index < NTXREG) {
        tx_writereg(s, index, val);
    }

    switch(index) {
    case TXTLP:
        // if the tail pointer no longer equals the head pointer..
        // we transmit
        if (s->tx_reg[TXTLP] != s->tx_reg[TXHDP]) {
            do_xmit(s);
        }
    }

    return;
}

static const MemoryRegionOps pza999_tx_mmio_ops = {
    .read = pza999_tx_mmio_read,
    .write = pza999_tx_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static uint64_t pza999_io_read(void *opaque, hwaddr addr, unsigned size)
{
    PZA999State *s = opaque;
    int shift = 0;
    uint64_t result = 0;
    uint32_t index = addr >> 2;

    uint32_t mask = 0xffffffff;
    if (size == 2) {
        mask = 0xffff;
    } else if (size != 4) {
        // error only sizes of 2 and 4 are supported
        return 0;
    }

    if (addr & 2) {
        shift = 16;
    } else if (addr & 1) {
        // error only offsets of 4 and 2 supported
        return 0;
    }

    if (index < NCTRLREGS) {
        result = (s->ctrl_reg[index] >> shift) & mask;
    }

    return result;
}

static void pza999_io_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    PZA999State *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);
    uint32_t index = addr >> 2;

    if (index < NCTRLREGS) {
        s->ctrl_reg[index] = (uint32_t)val;
    }

    /* special logic */
    int irq_level = 0;
    switch(index) {
    case IRSN:
        irq_level = (s->ctrl_reg[IMSK] & s->ctrl_reg[IRSN]);
        pci_set_irq(d, irq_level);
        break;
    }
}

static const MemoryRegionOps pza999_io_ops = {
    .read = pza999_io_read,
    .write = pza999_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* PCI */

static void pza999_io_setup(PZA999State *d)
{
    memory_region_init_io(&d->io, OBJECT(d), &pza999_io_ops, d, "pza999-io", IOPORT_SIZE);
    memory_region_init_io(&d->tx_mmio, OBJECT(d), &pza999_tx_mmio_ops, d, "pza999-tx-mmio", TX_MMIO_SIZE);
    memory_region_init_io(&d->rx_mmio, OBJECT(d), &pza999_rx_mmio_ops, d, "pza999-rx-mmio", RX_MMIO_SIZE);
}

static void pci_pza999_uninit(PCIDevice *pci_dev)
{
    PZA999State *d = PZA999(pci_dev);
    qemu_del_nic(d->nic);
    (void)d;
}

static NetClientInfo net_pza999_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = pza999_can_receive,
    .receive = pza999_receive,
    .receive_iov = pza999_receive_iov,
    .link_status_changed = pza999_set_link_status,
};

static void pza999_write_config(PCIDevice *pci_dev, uint32_t address,
                                uint32_t val, int len)
{
    PZA999State *s = PZA999(pci_dev);

    (void)s;
    pci_default_write_config(pci_dev, address, val, len);

    /* TODO: flush packets here on PCI_COMMAND_MASTER */
}

static void pci_pza999_realize(PCIDevice *pci_dev, Error **errp)
{
    DeviceState *dev = DEVICE(pci_dev);
    PZA999State *d = PZA999(pci_dev);
    uint8_t *pci_conf;
    uint8_t *macaddr;

    // initialize pci config space
    pci_conf = pci_dev->config;

    pci_dev->config_write = pza999_write_config;

    pci_conf[PCI_CACHE_LINE_SIZE] = 0x10;
    pci_conf[PCI_INTERRUPT_PIN] = 1;

    pza999_io_setup(d);

    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io);
    pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->tx_mmio);
    pci_register_bar(pci_dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->rx_mmio);

    qemu_macaddr_default_if_unset(&d->conf.macaddr);
    macaddr = d->conf.macaddr.a;

    d->nic = qemu_new_nic(&net_pza999_info, &d->conf,
                          object_get_typename(OBJECT(d)), dev->id, d);

    qemu_format_nic_info_str(qemu_get_queue(d->nic), macaddr);

    memset(d->ctrl_reg, 0, NCTRLREGS * sizeof(uint32_t));
    memset(d->tx_reg, 0, NTXREG * sizeof(uint32_t));
    memset(d->rx_reg, 0, NRXREG * sizeof(uint32_t));

    /* populate the mac addrs */
    memcpy(d->ctrl_reg, d->conf.macaddr.a, 6);
}

static void pza999_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = pci_pza999_realize;
    k->exit = pci_pza999_uninit;
    k->vendor_id = 0x505a;
    k->device_id = 0x0999;
    k->revision   = 0x00;
    k->class_id  = PCI_CLASS_NETWORK_ETHERNET;
    // expose PZA999 as a networking device (allows it to be used as 'model' in -nic)
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    dc->desc = "PZA Gigabit Ethernet";
    device_class_set_props(dc, pza999_properties);
}

static const TypeInfo pza999_info = {
    .name          = TYPE_PZA999,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PZA999State),
    .instance_init = pza999_instance_init,
    .class_init = pza999_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE},
        { },
    },
};

static void pza999_register_types(void)
{
    type_register_static(&pza999_info);
}

type_init(pza999_register_types)
