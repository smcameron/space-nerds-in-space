#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "commodities.h"
#include "infinite-taunt.h"
#include "mtwist.h"
#include "transport_contract.h"

#define MAX_SHIPPING_LOCATIONS 100
static uint32_t shipping_location[MAX_SHIPPING_LOCATIONS] = { 0 };
static int nshipping_locations = 0;

void add_transport_contract_shipping_location(uint32_t loc)
{
	if (nshipping_locations >= MAX_SHIPPING_LOCATIONS)
		return;

	shipping_location[nshipping_locations] = loc;
	nshipping_locations++;
}

void remove_transport_contract_shipping_location(uint32_t loc)
{
	int i, j;
	int count;

	do {
		count = 0;
		for (i = 0; i < nshipping_locations; i++) {
			if (shipping_location[i] == loc) {
				count++;
				break;
			}
		}
		if (count > 0) {
			for (j = i; j < nshipping_locations - 1; j++)
				shipping_location[j] = shipping_location[j + 1];
			nshipping_locations--;
		}
	} while (count > 0);
}

static uint32_t random_shipping_location(struct mtwist_state *mt, uint32_t avoid)
{
	uint32_t candidate;

	do {
		candidate = mtwist_int(mt, nshipping_locations);
	} while (shipping_location[candidate] == avoid);
	return shipping_location[candidate];
}

void free_transport_contract(struct transport_contract *tc)
{
	if (!tc)
		return;
	if (tc->shipper)
		free(tc->shipper);
	if (tc->recipient)
		free(tc->recipient);
	free(tc);
}

struct transport_contract *create_transport_contract(uint32_t universe_timestamp, struct commodity *commodity, int ncommodities)
{
	struct transport_contract *tc;
	static struct mtwist_state *mt = NULL;
	char buffer[100];
	float p;

	if (!mt)
		mt = mtwist_init(87832993);

	if (nshipping_locations < 2)
		return NULL;

	tc = calloc(1, sizeof(*tc));
	tc->contract_id = 10000 + mtwist_int(mt, 10000);
	tc->due_date = universe_timestamp + 5 * 600 + mtwist_int(mt, 5 * 600); /* 5 to 10 minutes */
	tc->origin = random_shipping_location(mt, (uint32_t) -1);
	tc->destination = random_shipping_location(mt, tc->origin);
	tc->assigned_to = (uint32_t) -1;
	character_name(mt, buffer, 99);
	tc->recipient = strdup(buffer);
	character_name(mt, buffer, 99);
	tc->shipper = strdup(buffer);
	tc->item = mtwist_int(mt, ncommodities);
	tc->cargo_value = mtwist_int(mt, 20000.0) + 10000;
	tc->shipping_fee = 0.1 * tc->cargo_value;
	tc->shipping_fee = ((int) (tc->shipping_fee / 100)) * 100;
	tc->loaded = 0;
	p = commodity_calculate_price(&commodity[tc->item], 0.5, 0.5, 0.5);
	tc->qty = tc->cargo_value / p;
	return tc;
}

void delete_transport_contract(struct transport_contract *tc[], int *ncontracts, int n)
{
	int i;

	if (n < 0 || n >= *ncontracts)
		return;
	free_transport_contract(tc[n]);
	for (i = n; i < *ncontracts - 1; i++)
		tc[i] = tc[i + 1];
	tc[*ncontracts - 1] = NULL;
	(*ncontracts)--;
}

struct transport_contract *duplicate_transport_contract(struct transport_contract *tc)
{
	struct transport_contract *nc;

	nc = calloc(1, sizeof(*nc));
	if (!nc)
		return nc;
	nc->contract_id = tc->contract_id;
	nc->due_date = tc->due_date;
	nc->origin = tc->origin;
	nc->destination = tc->destination;
	nc->assigned_to = tc->assigned_to;
	nc->item = tc->item;
	nc->qty = tc->qty;
	nc->shipping_fee = tc->shipping_fee;
	nc->cargo_value = tc->cargo_value;
	nc->loaded = tc->loaded;
	if (tc->recipient)
		nc->recipient = strdup(tc->recipient);
	else
		nc->recipient = NULL;
	if (tc->shipper)
		nc->shipper = strdup(tc->shipper);
	else
		nc->shipper = NULL;
	return nc;
}


#ifdef TEST_TRANSPORT_CONTRACT
#include <stdio.h>
#include "names.h"

char *planet_name[MAX_SHIPPING_LOCATIONS];
struct commodity *commodity;

static void print_transport_contract(struct transport_contract *tc)
{
	printf("----------------------------\n");
	printf("Contract ID: %u\n", tc->contract_id);
	printf("Due Date: %.1f\n", tc->due_date / 1000.0);
	printf("Recipient: %s\n", tc->recipient);
	printf("Destination: %s\n", planet_name[tc->destination]);
	printf("shipper: %s\n", tc->shipper);
	printf("Origin: %s\n", planet_name[tc->origin]);
	printf("Assigned to: %d\n", tc->assigned_to);
	printf("Load: %.1f %s %s\n", tc->qty, commodity[tc->item].unit, commodity[tc->item].name);
	printf("Shipping fee: %.1f\n", tc->shipping_fee);
	printf("Cargo value: %.1f\n", tc->cargo_value);
	printf("Loaded: %d\n", tc->loaded);
}

int main(int argc, char *argv[])
{
	int i, ncommodities;
	struct mtwist_state *mt = mtwist_init(78238423);

	commodity = read_commodities("share/snis/commodities.txt", &ncommodities);
	if (!commodity) {
		fprintf(stderr, "Failed to read share/snis/commodities.txt\n");
		return 1;
	}

	for (i = 0; i < MAX_SHIPPING_LOCATIONS; i++) {
		planet_name[i] = random_name(mt);
		add_transport_contract_shipping_location((uint32_t) i);
	}

	for (i = 0; i < 25; i++) {
		struct transport_contract *tc = create_transport_contract(2020000, commodity, ncommodities);
		print_transport_contract(tc);
		free_transport_contract(tc);
	}
	return 0;
}
#endif
