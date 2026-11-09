#ifndef PICOCALC_FXAI_LWIPOPTS_H
#define PICOCALC_FXAI_LWIPOPTS_H

// lwIP configuration for pico_cyw43_arch_lwip_threadsafe_background.
// Required by the Pico SDK lwIP integration (lwip/opt.h includes lwipopts.h).
// Values follow the Raspberry Pi pico-examples defaults for Pico W/Pico 2 W.
//
// Memory placement: with MEM_LIBC_MALLOC=0 lwIP uses its own static pools
// (memp/mem in .bss), so every pbuf and protocol control block stays in SRAM.
// This is intentional: pbufs may be touched from the CYW43 IRQ path and must
// not live in PSRAM (shared QSPI bus, unavailable during flash writes).

#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// MEM_LIBC_MALLOC is incompatible with the threadsafe_background architecture.
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    8000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1
#define LWIP_TCP_KEEPALIVE          1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define LWIP_CHKSUM_ALGORITHM       3

#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#endif
