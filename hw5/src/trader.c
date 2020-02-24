#include "debug.h"
#include "server.h"
#include "trader.h"
#include "exchange.h"
#include "protocol.h"
#include "csapp.h"
#include "errno.h"

#include <stdio.h>
#include <string.h>

typedef struct order {
    orderid_t orderid;

} ORDER;

TRADER *trader_ref(TRADER *trader, char *why);
void trader_unref(TRADER *trader, char *why);

typedef struct trader {
    char *name;                // username
    int cli_fd;
    sem_t mutex;
    funds_t balance;               // Trader's account balance
    quantity_t inventory;          // Trader's inventory
    funds_t bid;                   // Current highest bid price
    funds_t ask;                   // Current lowest ask price
    funds_t last;                  // Last trade price
    int ref_cnt;
} TRADER;

sem_t mutex;
TRADER *traders[MAX_TRADERS];
BRS_PACKET_HEADER *ack_pkt = NULL;
BRS_PACKET_HEADER *nack_pkt = NULL;

void init_a_trader(TRADER * trader, char* name, int cli_fd) {
    trader->name = NULL;
    trader->cli_fd = -1;
    Sem_init(&trader->mutex, 0, 1);
    trader->name = name;
    trader->cli_fd = cli_fd;
}

int trader_init(void) {
    Sem_init(&mutex, 0, 1);

    ack_pkt = Malloc(sizeof(BRS_PACKET_HEADER));
    ack_pkt->type = BRS_ACK_PKT;

    nack_pkt = Malloc(sizeof(BRS_PACKET_HEADER));
    nack_pkt->type = BRS_NACK_PKT;
    nack_pkt->size = 0;

    for (int i = 0; i < MAX_TRADERS; i++) {
        traders[i] = NULL;
    }
    return 0;
}

void trader_fini(void) {
    for(int i = 0; i < MAX_TRADERS; i++) {
        if (traders[i] != NULL) free(traders[i]->name);
        free(traders[i]);
    }
    free(ack_pkt);
    free(nack_pkt);
}

TRADER *trader_login(int fd, char *name) {
    P(&mutex);
    TRADER *trader = NULL;
    int found = 0;
    for (int i = 0; i < MAX_TRADERS; i++) {
        if (traders[i] == NULL) {
            trader = traders[i];
            found = 1;
        } else if (traders[i] -> name == NULL) {
            trader = traders[i];
            found = 1;
        } else if (strcmp(traders[i]->name, name)) {
            debug("User of %s has already logged in", name);
            V(&mutex);
            return NULL;
        }
    }

    if (found == 0) 
        debug("Traders over limit");
    else {
        if (trader == NULL) {
            trader = Calloc(1, sizeof(TRADER));
            trader->ref_cnt = 0;
            trader_ref(trader, "newly created trader");
        }
        init_a_trader(trader, name, fd);
        trader_ref(trader, "trader login");
    }
    V(&mutex);
    return trader;
}

void trader_logout(TRADER *trader) {
    P(&mutex);
    trader->name = NULL;
    trader_unref(trader, "logout");
    V(&mutex);
}

TRADER *trader_ref(TRADER *trader, char *why) {
    ++trader->ref_cnt;
    debug("trader_ref is called: %s, current ref_cnt: %d", why, trader->ref_cnt);
    return trader;
}

void trader_unref(TRADER *trader, char *why) {
    --trader->ref_cnt;
    debug("trader_unref is called: %s, current ref_cnt: %d", why, trader->ref_cnt);
    if (trader->ref_cnt == 0) {
        debug("ref_cnt is equal to 0 now, free the trader content and trader itself, set trader NULL");
        free(trader->name);
        free(trader);
        trader = NULL;
    }
}

int trader_send_packet(TRADER *trader, BRS_PACKET_HEADER *pkt, void *data) {
    return proto_send_packet(trader->cli_fd, pkt, data); 
}

int trader_broadcast_packet(BRS_PACKET_HEADER *pkt, void *data) {
    for (int i = 0; i < MAX_TRADERS; i++) {
        trader_send_packet(traders[i], pkt, data);
    }
    return 0;
}

int trader_send_ack(TRADER *trader, BRS_STATUS_INFO *info) {
    debug("into trader_send_ack");
    P(&trader->mutex);
    debug("start to set value for status_info");
    if (info != NULL) {
        info->balance = trader->balance;
        debug("set balance for status_info success");
        info->inventory = trader->inventory;
        info->bid = trader->balance;
        info->ask = trader->inventory;
        info->last = trader->last;
        ack_pkt->size = sizeof(BRS_STATUS_INFO);
    } else
        ack_pkt->size = 0;
    int res = trader_send_packet(trader, ack_pkt, info);

    V(&trader->mutex);
    return res;
}

int trader_send_nack(TRADER *trader) {
    return trader_send_packet(trader, nack_pkt, NULL);
}

void trader_increase_balance(TRADER *trader, funds_t amount) {
    P(&trader->mutex);
    (trader->balance) += amount;
    V(&trader->mutex);
}

int trader_decrease_balance(TRADER *trader, funds_t amount) {
    P(&trader->mutex);
    int res = -1;
    if (trader->balance >= amount) {
        (trader->balance) -= amount;
        res = 0;
    } else 
        debug("Decrease balance failed: original balance of %d < amount of %d", trader->balance, amount);
    V(&trader->mutex);
    return res;
}

void trader_increase_inventory(TRADER *trader, quantity_t quantity) {
    P(&trader->mutex);
    (trader->inventory) += quantity;
    V(&trader->mutex);
}

int trader_decrease_inventory(TRADER *trader, quantity_t quantity) {
    P(&trader->mutex);
    int res = -1;
    if (trader->inventory >= quantity) {
        (trader->inventory) -= quantity;
        res = 0;
    } else 
        debug("Decrease inventory failed: original inventory of %d < quantity of %d", trader->inventory, quantity);
    V(&trader->mutex);
    return res;
}