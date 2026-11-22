#pragma once
#include <stdint.h>

void battery_service_init(void);
void battery_service_on_db_cache_applied(uint16_t conn_handle);
