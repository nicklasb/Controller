

#ifdef __cplusplus
extern "C" {
#endif

/* 16 Bit SPP Service UUID */
#define GATT_SPP_SVC_UUID                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define GATT_SPP_CHR_UUID                                  0xABF1

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
void print_addr(const void *addr);
int gatt_svr_register(void);
void ble_store_config_init(void);

#ifdef __cplusplus
}
#endif
