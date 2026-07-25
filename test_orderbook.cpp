#include<gtest/gtest.h>
import OrderBookModels;
import OrderBookEngine;

using namespace Trading;

class OrderBookTest : public ::testing::Test {
protected:
	OrderBook book;
};

TEST_F(OrderBookTest, SimpleCrossMatch) {
	Order sell_order{1, 500000, 100, 100, Side::Sell, OrderType::GoodTillCancel};
	book.add_order(sell_order);

	Order buy_order{2, 505000, 40, 40, Side::Buy, OrderType::GoodTillCancel};
	auto trades = book.add_order(buy_order);

	ASSERT_EQ(trades.size(), 1);
	EXPECT_EQ(trades[0].bid_trade.quantity, 40);
	EXPECT_EQ(trades[0].bid_trade.price, 500000);

	OrderBookLevelInfos levels = book.get_level_infos();
	ASSERT_EQ(levels.asks.size(), 1);
	EXPECT_EQ(levels.asks[0].quantity, 60);
	EXPECT_TRUE(levels.bids.empty());
}

TEST_F(OrderBookTest, FillOrKillIsRejectedWhenLiquidityLacking) {
    book.add_order(Order{1, 10000, 50, 50, Side::Sell, OrderType::GoodTillCancel});
    book.add_order(Order{2, 10000, 30, 30, Side::Sell, OrderType::GoodTillCancel});

    Order fok_buy{3, 10050, 100, 100, Side::Buy, OrderType::FillOrKill};
    auto trades = book.add_order(fok_buy);

    EXPECT_TRUE(trades.empty());

    OrderBookLevelInfos levels = book.get_level_infos();
    EXPECT_EQ(levels.asks[0].quantity, 80);
}

TEST_F(OrderBookTest, ModifyUpsizeLosesPriority) {
    book.add_order(Order{1, 100000, 100, 100, Side::Buy, OrderType::GoodTillCancel}); // First in line
    book.add_order(Order{2, 100000, 100, 100, Side::Buy, OrderType::GoodTillCancel}); // Second in line

    OrderModify upsize_mod{1, 150, 100000, Side::Buy};
    book.modify_order(upsize_mod);
    Order market_sell{3, 0, 100, 100, Side::Sell, OrderType::Market};
    auto trades = book.add_order(market_sell);
    OrderBookLevelInfos levels = book.get_level_infos();
	for (const auto& level : levels.bids) {
		std::cout << "GHOST HUNT - Price: " << level.price << " | Qty: " << level.quantity << "\n";
	}
    ASSERT_EQ(levels.bids.size(), 1);
    EXPECT_EQ(levels.bids[0].quantity, 150);
}