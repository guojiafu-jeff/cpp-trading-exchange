#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace Exchange {

// Domain names for plain integers, so that signatures read like the business.
using OrderId = uint64_t;
using ClientId = uint32_t;
using TickerId = uint32_t;
using Qty = uint32_t;

/// Prices are integers in ticks, never floating point: 1 tick = $0.01, so
/// $100.01 is stored as 10001. A double cannot represent 0.01 exactly.
using Price = int64_t;

constexpr OrderId kInvalidOrderId = 0;
constexpr Price kTicksPerDollar = 100;

enum class Side : uint8_t { BUY, SELL };
enum class OrderType : uint8_t { LIMIT, MARKET };

inline std::string sideToString(Side side) { return side == Side::BUY ? "BUY" : "SELL"; }

inline std::string orderTypeToString(OrderType type) { return type == OrderType::LIMIT ? "LIMIT" : "MARKET"; }

inline std::string priceToString(Price price) {
    return std::format("{}.{:02}", price / kTicksPerDollar, price % kTicksPerDollar);
}


/// One order as the exchange sees it: plain data plus toString().
///
/// prev/next make an Order a node of an intrusive doubly-linked list: all
/// orders resting at the same price form a FIFO queue. The OrderBook owns
/// those links; clients never touch them.
struct Order {
    OrderId order_id = kInvalidOrderId;
    ClientId client_id = 0;
    TickerId ticker_id = 0;
    Side side = Side::BUY;
    OrderType type = OrderType::LIMIT;
    Price price = 0;  // ignored for MARKET orders
    Qty qty = 0;      // remaining (not yet filled) quantity

    Order* prev = nullptr;
    Order* next = nullptr;

    std::string toString() const {
        return std::format("Order[id={} client={} ticker={} {} {} px={} qty={}]", order_id, client_id, ticker_id,
                           sideToString(side), orderTypeToString(type), priceToString(price), qty);
    }
};

}   // namespace Exchange