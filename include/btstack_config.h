#ifndef PICOCALC_FXAI_BTSTACK_CONFIG_H
#define PICOCALC_FXAI_BTSTACK_CONFIG_H
// BTstack configuration, used only when the firmware is built with
// -DFXAI_ENABLE_BT=ON (pico_btstack_ble + pico_btstack_cyw43). Adapted from
// raspberrypi/pico-examples bluetooth/config/btstack_config_common.h
// (BSD-3-Clause, Copyright 2020 Raspberry Pi (Trading) Ltd.), reduced to an LE
// peripheral. BTstack keeps its link keys in the last two flash sectors, which
// the firmware layout always reserves (see docs/PICO_PLUS_2W.md).

#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP // required by the SDK hci_dump_embedded_stdout backend
#ifdef ENABLE_BLE
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#endif

#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_PAYLOAD_SIZE (255 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 2
#define MAX_NR_L2CAP_SERVICES 2
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 4
#define MAX_NR_LE_DEVICE_DB_ENTRIES 4

// Limit ACL buffers to avoid overrunning the bus shared with Wi-Fi.
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define HCI_HOST_ACL_PACKET_LEN (255 + 4)
#define HCI_HOST_ACL_PACKET_NUM 3
#define HCI_HOST_SCO_PACKET_LEN 120
#define HCI_HOST_SCO_PACKET_NUM 3

#define NVM_NUM_DEVICE_DB_ENTRIES 4
#define NVM_NUM_LINK_KEYS 4
#define MAX_ATT_DB_SIZE 512

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 1000
#define ENABLE_SOFTWARE_AES128
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS

#endif
