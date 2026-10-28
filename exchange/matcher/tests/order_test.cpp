#include "exchange/matcher/order.h"

#include <string>

#include "gtest/gtest.h"

namespace {

TEST(Order, ToStringShowsEveryField) {
    const Exchange::Order o{.order_id = 42,
                            .client_id = 7,
                            .ticker_id = 1,
                            .side = Exchange::Side::BUY,
                            .type = Exchange::OrderType::LIMIT,
                            .price = 10001,
                            .qty = 10};
    EXPECT_EQ(o.toString(), "Order[id=42 client=7 ticker=1 BUY LIMIT px=100.01 qty=10]");
}

TEST(Order, PriceToStringPadsCents) {
    EXPECT_EQ(Exchange::priceToString(10001), "100.01");
    EXPECT_EQ(Exchange::priceToString(9999), "99.99");
    EXPECT_EQ(Exchange::priceToString(500), "5.00");
    EXPECT_EQ(Exchange::priceToString(0), "0.00");
}

TEST(Order, DefaultOrderIsInvalidAndUnlinked) {
    const Exchange::Order o;
    EXPECT_EQ(o.order_id, Exchange::kInvalidOrderId);
    EXPECT_EQ(o.qty, 0u);
    EXPECT_EQ(o.prev, nullptr);
    EXPECT_EQ(o.next, nullptr);
}

}  // namespace