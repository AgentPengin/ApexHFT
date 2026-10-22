#pragma once
#include <cstdint>

enum class Side : uint8_t { BUY, SELL };

enum class OrderType : uint8_t { 
    LIMIT,
    MARKET, 
    CANCEL
};

enum class OrderStatus : uint8_t { 
    NEW, 
    PARTIALLY_FILLED, 
    FILLED, 
    CANCELED, 
    REJECTED,
    EXPIRED
};

struct Order {
    uint64_t orderId;
    uint64_t price;
    uint64_t timestamp; 
    uint64_t clientOrderId;

    uint32_t clientId;
    uint32_t quantity;
    uint32_t executedQuantity;
    uint32_t remainingQuantity;

    Side side;
    OrderType type;
    OrderStatus status;
    uint8_t timeInForce; 

    uint8_t padding[12];
};

static_assert(sizeof(Order) == 64);
static_assert(alignof(Order) == 8);
