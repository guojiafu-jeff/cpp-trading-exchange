#include "exchange/matcher/order_book.h"

#include <string>

#include "gtest/gtest.h"

namespace {

using Exchange::Order;
using Exchange::OrderBook;
using Exchange::OrderId;
using Exchange::OrderType;
using Exchange::Price;
using Exchange::Qty;
using Exchange::Side;

constexpr Exchange::TickerId kTicker = 1;

Order makeLimit(OrderId id, Side side, Price price, Qty qty) {
    return Order{.order_id = id,
                 .client_id = 42,
                 .ticker_id = kTicker,
                 .side = side,
                 .type = OrderType::LIMIT,
                 .price = price,
                 .qty = qty};
}

TEST(OrderBook, EmptyBookHasNoBestPrices) {
    OrderBook book(kTicker, 8);
    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_FALSE(book.bestAsk().has_value());
    EXPECT_EQ(book.numOrders(), 0u);
}

TEST(OrderBook, AddRestsOrdersAndTracksBestPrices) {
    OrderBook book(kTicker, 8);
    ASSERT_NE(book.add(makeLimit(1, Side::BUY, 9999, 5)), nullptr);
    ASSERT_NE(book.add(makeLimit(2, Side::BUY, 10000, 7)), nullptr);
    ASSERT_NE(book.add(makeLimit(3, Side::SELL, 10002, 3)), nullptr);
    ASSERT_NE(book.add(makeLimit(4, Side::SELL, 10001, 8)), nullptr);
    EXPECT_EQ(book.bestBid().value(), 10000);
    EXPECT_EQ(book.bestAsk().value(), 10001);
    EXPECT_EQ(book.numOrders(), 4u);
    ASSERT_NE(book.find(2), nullptr);
    EXPECT_EQ(book.find(2)->qty, 7u);
}

TEST(OrderBook, SamePriceKeepsArrivalOrder) {
    OrderBook book(kTicker, 8);
    book.add(makeLimit(1, Side::BUY, 10000, 5));
    book.add(makeLimit(2, Side::BUY, 10000, 7));
    const std::string s = book.toString();
    EXPECT_NE(s.find("[1:5] [2:7]"), std::string::npos) << s;
}

TEST(OrderBook, CancelRemovesOrderAndEmptyLevel) {
    OrderBook book(kTicker, 8);
    book.add(makeLimit(1, Side::BUY, 10000, 5));
    book.add(makeLimit(2, Side::BUY, 9999, 5));
    EXPECT_TRUE(book.cancel(1));
    EXPECT_EQ(book.bestBid().value(), 9999);
    EXPECT_EQ(book.find(1), nullptr);
    EXPECT_EQ(book.numOrders(), 1u);
}

TEST(OrderBook, CancelMiddleOfQueueKeepsNeighboursLinked) {
    OrderBook book(kTicker, 8);
    book.add(makeLimit(1, Side::SELL, 10001, 1));
    book.add(makeLimit(2, Side::SELL, 10001, 2));
    book.add(makeLimit(3, Side::SELL, 10001, 3));
    EXPECT_TRUE(book.cancel(2));
    const std::string s = book.toString();
    EXPECT_NE(s.find("[1:1] [3:3]"), std::string::npos) << s;
    EXPECT_TRUE(book.cancel(1));
    EXPECT_TRUE(book.cancel(3));
    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBook, CancelUnknownIdFails) {
    OrderBook book(kTicker, 8);
    EXPECT_FALSE(book.cancel(99));
}

TEST(OrderBook, RejectsBadOrders) {
    OrderBook book(kTicker, 2);
    EXPECT_EQ(book.add(makeLimit(1, Side::BUY, 10000, 0)), nullptr);  // zero quantity
    Order wrong_ticker = makeLimit(2, Side::BUY, 10000, 1);
    wrong_ticker.ticker_id = 7;
    EXPECT_EQ(book.add(wrong_ticker), nullptr);  // not this book's ticker
    ASSERT_NE(book.add(makeLimit(3, Side::BUY, 10000, 1)), nullptr);
    EXPECT_EQ(book.add(makeLimit(3, Side::BUY, 10000, 1)), nullptr);  // duplicate id
    ASSERT_NE(book.add(makeLimit(4, Side::BUY, 10000, 1)), nullptr);
    EXPECT_EQ(book.add(makeLimit(5, Side::BUY, 10000, 1)), nullptr);  // pool of 2 is full
}

TEST(OrderBook, CancelledSlotCanBeReused) {
    OrderBook book(kTicker, 1);
    ASSERT_NE(book.add(makeLimit(1, Side::BUY, 10000, 1)), nullptr);
    EXPECT_TRUE(book.cancel(1));
    EXPECT_NE(book.add(makeLimit(2, Side::BUY, 10000, 1)), nullptr);
}

}  // namespace