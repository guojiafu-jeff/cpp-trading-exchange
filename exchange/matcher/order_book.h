#pragma once

#include <cstddef>
#include <format>
#include <functional>  // std::greater
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include "common/mem_pool.h"
#include "exchange/matcher/order.h"


namespace Exchange {

/// Every resting order at one price, kept in arrival order (time priority).
/// head is the oldest order and trades first; tail is the newest.
struct PriceLevel {
    Price price = 0;
    Qty total_qty = 0;  // sum of qty over all orders at this price
    std::size_t num_orders = 0;
    Order* head = nullptr;
    Order* tail = nullptr;
};


/// A limit order book for one ticker.
///
/// Data structures:
///   - orders live in a MemPool<Order>: no heap allocation per order;
///   - orders_by_id_ maps OrderId -> Order* for O(1) cancel;
///   - bids_ / asks_ map price -> PriceLevel, sorted so that begin() is the
///     best price on that side (highest bid, lowest ask);
///   - inside a level, orders form an intrusive FIFO list through
///     Order::prev / Order::next (O(1) append, O(1) unlink).
class OrderBook final {
public:
    OrderBook(TickerId ticker_id, std::size_t max_orders) : ticker_id_(ticker_id), pool_(max_orders) {
        orders_by_id_.reserve(max_orders);
    }

    // The book owns its pool and holds raw pointers into it: a copy would leave
    // two books pointing at the same orders. So we should never copy or move OrderBook.
    OrderBook(const OrderBook&) = delete("an OrderBook owns its orders");
    OrderBook(OrderBook&&) = delete("an OrderBook owns its orders");
    OrderBook& operator=(const OrderBook&) = delete("an OrderBook owns its orders");
    OrderBook& operator=(OrderBook&&) = delete("an OrderBook owns its orders");

    /// Copy `order` into the book as a resting order.
    /// Returns a pointer to the resting copy, or nullptr if the order was
    /// rejected (wrong ticker, zero qty, invalid or duplicate id, market order, pool full).
    const Order* add(const Order& order) {
        if (order.ticker_id != ticker_id_ || order.qty == 0 || order.order_id == kInvalidOrderId) return nullptr;
        if (order.type != OrderType::LIMIT) return nullptr;  // market orders need matching (Part 2)
        if (orders_by_id_.contains(order.order_id)) return nullptr;

        Order* resting = pool_.allocate(order);  // copy-construct into the pool
        if (resting == nullptr) return nullptr;  // pool exhausted
        resting->prev = nullptr;
        resting->next = nullptr;

        if (resting->side == Side::BUY) {
            appendToLevel(bids_[resting->price], resting);
        } else {
            appendToLevel(asks_[resting->price], resting);
        }
        orders_by_id_.emplace(resting->order_id, resting);
        return resting;
    }

    /// Remove a resting order. Returns false if no such order is in the book.
    bool cancel(OrderId order_id) {
        const auto it = orders_by_id_.find(order_id);
        if (it == orders_by_id_.end()) return false;
        Order* order = it->second;
        if (order->side == Side::BUY) {
            removeFromBook(bids_, order);
        } else {
            removeFromBook(asks_, order);
        }
        orders_by_id_.erase(it);
        pool_.deallocate(order);
        return true;
    }

    std::optional<Price> bestBid() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.begin()->first;
    }

    std::optional<Price> bestAsk() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }

    std::size_t numOrders() const { return orders_by_id_.size(); }

    const Order* find(OrderId order_id) const {
        const auto it = orders_by_id_.find(order_id);
        return it == orders_by_id_.end() ? nullptr : it->second;
    }

    /// The price ladder: asks from high to low, a divider, then bids from high
    /// to low, so the best prices meet in the middle.
    std::string toString() const {
        std::string out = std::format("=== OrderBook ticker={} orders={} ===\n", ticker_id_, orders_by_id_.size());
        for (auto it = asks_.rbegin(); it != asks_.rend(); ++it) out += levelToString("ASK", it->second);
        out += "  ---------------------------------\n";
        for (const auto& [price, level] : bids_) out += levelToString("BID", level);
        return out;
    }


private:
    using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;  // highest price first
    using AskLevels = std::map<Price, PriceLevel>;                        // lowest price first

    /// Link `order` at the tail of `level`: it becomes the newest order there.
    static void appendToLevel(PriceLevel& level, Order* order) {
        if (level.num_orders == 0) level.price = order->price;
        order->prev = level.tail;
        order->next = nullptr;
        if (level.tail != nullptr) {
            level.tail->next = order;
        } else {
            level.head = order;
        }
        level.tail = order;
        level.total_qty += order->qty;
        ++level.num_orders;
    }

    /// Unlink `order` from `level`, wherever it sits in the FIFO.
    static void removeFromLevel(PriceLevel& level, Order* order) {
        if (order->prev != nullptr) {
            order->prev->next = order->next;
        } else {
            level.head = order->next;
        }
        if (order->next != nullptr) {
            order->next->prev = order->prev;
        } else {
            level.tail = order->prev;
        }
        level.total_qty -= order->qty;
        --level.num_orders;
        order->prev = nullptr;
        order->next = nullptr;
    }

    /// Unlink `order` from its level in `levels`, dropping the level if it became empty.
    template <typename Levels>
    static void removeFromBook(Levels& levels, Order* order) {
        const auto it = levels.find(order->price);
        removeFromLevel(it->second, order);
        if (it->second.num_orders == 0) levels.erase(it);
    }


    static std::string levelToString(const char* side, const PriceLevel& level) {
        std::string out = std::format("  {} {:>9} | qty {:>6} | {} order(s):", side, priceToString(level.price),
                                      level.total_qty, level.num_orders);
        for (const Order* o = level.head; o != nullptr; o = o->next) {
            out += std::format(" [{}:{}]", o->order_id, o->qty);
        }
        out += '\n';
        return out;
    }

    TickerId ticker_id_;
    Common::MemPool<Order> pool_;
    std::unordered_map<OrderId, Order*> orders_by_id_;
    BidLevels bids_;
    AskLevels asks_;

};

}   // namespace Exchange