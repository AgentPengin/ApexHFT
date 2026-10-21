#pragma once

enum class Side {
  BUY, 
  SELL  
};

enum class OrderType {
    LIMIT,
    MARKET, 
    CANCEL
};

struct Order {
    uint64_t orderId;
    uint32_t quantity;
    uint32_t clientId;
    uint64_t price;
    Side side;
    OrderType type;
    uint64_t timestamp;
};

