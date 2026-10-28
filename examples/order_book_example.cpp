#include <print>

#include "exchange/matcher/order_book.h"

using Exchange::Order;
using Exchange::OrderBook;
using Exchange::OrderType;
using Exchange::Side;

int main() {
    OrderBook book(1, 16);  // ticker 1, room for 16 resting orders

    auto limit = [](Exchange::OrderId id, Side side, Exchange::Price price, Exchange::Qty qty) {
        return Order{.order_id = id,
                     .client_id = 1,
                     .ticker_id = 1,
                     .side = side,
                     .type = OrderType::LIMIT,
                     .price = price,
                     .qty = qty};
    };

    book.add(limit(1, Side::SELL, 10001, 8));  // ask 8 @ 100.01
    book.add(limit(2, Side::BUY, 10000, 5));   // bid 5 @ 100.00
    std::println("after two orders:\n{}", book.toString());

    book.add(limit(3, Side::BUY, 10000, 7));   // bid 7 @ 100.00, queues behind order 2
    book.add(limit(4, Side::SELL, 10002, 3));  // ask 3 @ 100.02
    std::println("after four orders:\n{}", book.toString());

    book.cancel(2);  // the oldest bid at 100.00 leaves; order 3 is now first in line
    std::println("after cancelling order 2:\n{}", book.toString());

    return 0;
}