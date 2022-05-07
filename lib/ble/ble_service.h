/*
 * BLE service handling
 * Loosely based on the ESP-IDF-demo
 */

#ifdef __cplusplus
extern "C" {
#endif

void (*on_ble_data_cb)(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt);


/* 16 Bit SPP Service UUID */
#define GATT_SPP_SVC_UUID                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define GATT_SPP_CHR_UUID                                  0xABF1

uint16_t ble_svc_gatt_read_val_handle, ble_spp_svc_gatt_read_val_handle;

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
void print_addr(const void *addr);
int gatt_svr_register(void);
void ble_store_config_init(void);

#ifdef __cplusplus
}
#endif
