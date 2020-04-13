#include <stdint.h>
#include <stdlib.h>
#include "commodities.h"

struct transport_contract {
	uint32_t contract_id;
	uint32_t due_date;
	uint32_t origin, destination, assigned_to;
	int item; /* commodity */
	float qty;
	float shipping_fee;
	float cargo_value;
	char *recipient;
	char *shipper;
	int loaded;
};

void add_transport_contract_shipping_location(uint32_t loc);
void remove_transport_contract_shipping_location(uint32_t loc);
void free_transport_contract(struct transport_contract *tc);
struct transport_contract *create_transport_contract(uint32_t universe_timestamp,
			struct commodity *commodity, int ncommodities);
void delete_transport_contract(struct transport_contract *tc[], int *ncontracts, int i);
struct transport_contract *duplicate_transport_contract(struct transport_contract *tc);

