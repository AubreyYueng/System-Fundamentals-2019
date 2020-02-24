#include "debug.h"
#include "server.h"
#include "trader.h"
#include "exchange.h"
#include "csapp.h"
#include "errno.h"

#include <stdio.h>

void *brs_client_service(void *arg) {
    pthread_t t = Pthread_self();
    int fd = *(int *)arg;
    debug("%ld---fd of %d into brs_client_service", t, fd);

    /* free, detach, register */
    free(arg);
    Pthread_detach(t);
    creg_register(client_registry, fd);

    BRS_PACKET_HEADER hdr ;
    hdr.type = 0 ;

    void **payloadp = Malloc(sizeof(void *));
    BRS_STATUS_INFO *status = Malloc(sizeof(BRS_STATUS_INFO));
    BRS_FUNDS_INFO *funds = Malloc(sizeof(BRS_FUNDS_INFO));
    BRS_ESCROW_INFO *escrow = Malloc(sizeof(BRS_ESCROW_INFO));
    BRS_ORDER_INFO *order = Malloc(sizeof(BRS_ORDER_INFO));
    BRS_CANCEL_INFO *cancel = Malloc(sizeof(BRS_CANCEL_INFO));
    quantity_t* canceled_quantity = Malloc(sizeof(int));
    TRADER* trader = NULL;
    
    while(1) {
        /* Retrieve package sent by client */
        if (proto_recv_packet(fd, &hdr, payloadp) != 0)
        {
            continue;
        }
        status->orderid = 0;
        status->quantity = 0;

        uint8_t type = hdr.type;
        if (trader == NULL && type != BRS_LOGIN_PKT) {
            debug("Login required");
            continue;
        }

        if (type == BRS_LOGIN_PKT) {
            /* Check if already logged in */
            if (trader != NULL) {
                debug("%ld---%s", t, "Already logged in, login package no longer accepted");
                continue;
            } 
            debug("%ld---%s", t, "Login package accepted");

            /* Sendback ACK or NACK depended on login result */
            trader = trader_login(fd, *payloadp);
            if (trader == NULL) {
                trader_send_nack(trader);
                continue;
            }
            trader_send_ack(trader, NULL);
        } else if (type == BRS_STATUS_PKT) {
            debug("Start to send status");
            trader_send_ack(trader, status);
        } else if (type == BRS_DEPOSIT_PKT) {
            funds = (BRS_FUNDS_INFO*)*payloadp;
            trader_increase_balance(trader, funds->amount);
            trader_send_ack(trader, status);
        } else if (type == BRS_WITHDRAW_PKT) {
            funds = (BRS_FUNDS_INFO*)*payloadp;
            if (trader_decrease_balance(trader, funds->amount) == 0)
                trader_send_ack(trader, status);
            else
                trader_send_nack(trader);
        } else if (type == BRS_ESCROW_PKT) {
            escrow = (BRS_ESCROW_INFO*)*payloadp;
            trader_increase_inventory(trader, escrow->quantity);
            trader_send_ack(trader, status);
        } else if (type == BRS_RELEASE_PKT) {
            escrow = (BRS_ESCROW_INFO*)*payloadp;
            if (trader_decrease_inventory(trader, escrow->quantity) == 0)
                trader_send_ack(trader, status);
            else
                trader_send_nack(trader);
        } else if (type == BRS_BUY_PKT) {
            order = (BRS_ORDER_INFO*)*payloadp;
            if (trader_decrease_balance(trader, order->price*order->quantity) == 0) {
                orderid_t order_id = exchange_post_buy(exchange, trader, order->quantity, order->price);
                if (order_id != 0) {
                    status->orderid = order_id;
                    trader_send_ack(trader, status);
                    continue;
                }
            }
            trader_send_nack(trader);        
        } else if (type == BRS_SELL_PKT) {
            order = (BRS_ORDER_INFO*)*payloadp;
            if (trader_decrease_inventory(trader, order->quantity) == 0) {
                orderid_t order_id = exchange_post_sell(exchange, trader, order->quantity, order->price);
                if (order_id != 0) {
                    status->orderid = order_id;
                    trader_send_ack(trader, status);
                    continue;
                }
            }
            trader_send_nack(trader);
        } else if (type == BRS_CANCEL_PKT) {
            cancel = (BRS_CANCEL_INFO*)*payloadp;
            if (exchange_cancel(exchange, trader, cancel->order, canceled_quantity) == 0) {
                // TODO: credited back for sell or buy order
                status->orderid = cancel->order;
                status->quantity = *canceled_quantity;
                trader_send_ack(trader, status);
                continue;
            }
            trader_send_nack(trader);
        }
    }
        // TODO: Don't we need logout?
        // TODO: when logout, unregister, close fd, break
        Free(payloadp);
        Free(status);
        Free(funds);
        Free(escrow);
        Free(order);
        Free(cancel);
        Free(canceled_quantity);
        close(fd);
        creg_unregister(client_registry, fd);
        return NULL ;
}