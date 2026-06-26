#include "OrderBook.hpp"

#include <gtest/gtest.h>

using namespace ::testing;

struct OrderBookTests : Test {
    OrderBook order_book{};
    Order buyOrder{OrderId{1}, OrderType::Market, Quantity{100}, Price{200}, TradeSide::Buy};
    Order sellOrder{OrderId{2}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Sell};
    Order matchingSellOrder{OrderId{3}, OrderType::Market, Quantity{100}, Price{150}, TradeSide::Sell};
    Order matchingBuyOrder{OrderId{4}, OrderType::Market, Quantity{100}, Price{350}, TradeSide::Buy};
};

TEST_F(OrderBookTests, canAddBuyMarketOrder) {
    order_book.addOrder(buyOrder);
    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(buyOrder.getId()), buyOrder);
}

TEST_F(OrderBookTests, canAddSellMarketOrder) {
    order_book.addOrder(sellOrder);
    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(sellOrder.getId()), sellOrder);
}

TEST_F(OrderBookTests, canCancelBuyMarketOrder) {
    order_book.addOrder(buyOrder);
    order_book.cancelOrder(buyOrder.getId());
    ASSERT_EQ(order_book.getOrdersCount(), 0);
    auto trades = order_book.addOrder(matchingSellOrder);
    ASSERT_TRUE(trades.empty()); // no matches, order book cleaned properly
    ASSERT_EQ(order_book.getOrdersCount(), 1);
}

TEST_F(OrderBookTests, canCancelSellMarketOrder) {
    order_book.addOrder(sellOrder);
    order_book.cancelOrder(sellOrder.getId());
    ASSERT_EQ(order_book.getOrdersCount(), 0);
    auto trades = order_book.addOrder(matchingBuyOrder);
    ASSERT_TRUE(trades.empty()); // no matches, order book cleaned properly
    ASSERT_EQ(order_book.getOrdersCount(), 1);
}

TEST_F(OrderBookTests, cancellingLastBidOfferRemovesPriceLevel) {
    order_book.addOrder(buyOrder);
    Order buyOrderSamePriceLevel{OrderId{123}, OrderType::Market, Quantity{100}, buyOrder.getPrice(), TradeSide::Buy};
    order_book.addOrder(buyOrderSamePriceLevel);
    order_book.cancelOrder(buyOrder.getId());
    ASSERT_EQ(order_book.getBids().size(), 1);
    order_book.cancelOrder(buyOrderSamePriceLevel.getId());
    ASSERT_EQ(order_book.getBids().size(), 0);
}

TEST_F(OrderBookTests, cancellingLastAskOfferRemovesPriceLevel) {
    order_book.addOrder(sellOrder);
    Order sellOrderSamePriceLevel{OrderId{123}, OrderType::Market, Quantity{100}, sellOrder.getPrice(), TradeSide::Sell};
    order_book.addOrder(sellOrderSamePriceLevel);
    order_book.cancelOrder(sellOrder.getId());
    ASSERT_EQ(order_book.getAsks().size(), 1);
    order_book.cancelOrder(sellOrderSamePriceLevel.getId());
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, sellMarketOrderDoesNotFillWhenPricesMismatch) {
    order_book.addOrder(buyOrder);
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, buyMarketOrderMarketDoesNotFillWhenPricesMismatch) {
    order_book.addOrder(sellOrder);

    auto trades = order_book.addOrder(buyOrder);
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, buyMarketOrderFillsFullyWhenPricesAndQuantityMatch) {
    order_book.addOrder(matchingSellOrder);

    auto trades = order_book.addOrder(buyOrder);
    ASSERT_EQ(trades.size(), 1);
    auto &trade = trades.front();
    EXPECT_EQ(trade.quantity, buyOrder.getInitialQuantity());
    EXPECT_EQ(trade.price, matchingSellOrder.getPrice());
    EXPECT_EQ(trade.aggressor_id, buyOrder.getId());
    EXPECT_EQ(trade.aggressor_side, TradeSide::Buy);
    EXPECT_EQ(trade.order_id_a, buyOrder.getId());
    EXPECT_EQ(trade.order_id_b, matchingSellOrder.getId());

    ASSERT_EQ(order_book.getOrdersCount(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
}


TEST_F(OrderBookTests, sellMarketOrderFillsFullyWhenPricesAndQuantityMatch) {
    order_book.addOrder(buyOrder);

    auto trades = order_book.addOrder(matchingSellOrder);

    ASSERT_EQ(trades.size(), 1);
    auto &trade = trades.front();
    EXPECT_EQ(trade.quantity, buyOrder.getInitialQuantity());
    EXPECT_EQ(trade.price, buyOrder.getPrice());
    EXPECT_EQ(trade.aggressor_id, matchingSellOrder.getId());
    EXPECT_EQ(trade.aggressor_side, TradeSide::Sell);
    EXPECT_EQ(trade.order_id_a, matchingSellOrder.getId());
    EXPECT_EQ(trade.order_id_b, buyOrder.getId());

    ASSERT_EQ(order_book.getOrdersCount(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
}

TEST_F(OrderBookTests, buyMarketOrderFillsPartially) {
    order_book.addOrder(matchingSellOrder);
    Order partialFillOrder{
        OrderId{1}, OrderType::Market, Quantity{matchingSellOrder.getInitialQuantity() / 2}, Price{200}, TradeSide::Buy
    };

    auto trades = order_book.addOrder(partialFillOrder);
    ASSERT_EQ(trades.size(), 1);
    auto &trade = trades.front();
    EXPECT_EQ(trade.quantity, partialFillOrder.getInitialQuantity());
    EXPECT_EQ(trade.price, matchingSellOrder.getPrice());
    EXPECT_EQ(trade.aggressor_id, partialFillOrder.getId());
    EXPECT_EQ(trade.aggressor_side, TradeSide::Buy);
    EXPECT_EQ(trade.order_id_a, partialFillOrder.getId());
    EXPECT_EQ(trade.order_id_b, matchingSellOrder.getId());

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getBids().size(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 1);
}

TEST_F(OrderBookTests, sellMarketOrderFillsPartially) {
    order_book.addOrder(buyOrder);
    Order partialFillOrder{
        OrderId{2}, OrderType::Market, Quantity{buyOrder.getInitialQuantity() / 2}, Price{200}, TradeSide::Sell
    };


    auto trades = order_book.addOrder(partialFillOrder);

    ASSERT_EQ(trades.size(), 1);
    auto &trade = trades.front();
    EXPECT_EQ(trade.quantity, partialFillOrder.getInitialQuantity());
    EXPECT_EQ(trade.price, buyOrder.getPrice());
    EXPECT_EQ(trade.aggressor_id, partialFillOrder.getId());
    EXPECT_EQ(trade.aggressor_side, TradeSide::Sell);
    EXPECT_EQ(trade.order_id_a, partialFillOrder.getId());
    EXPECT_EQ(trade.order_id_b, buyOrder.getId());

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getBids().size(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, triesManyExistingMarketOrdersFromSinglePriceLevel) {
    Order buyOrder1{OrderId{1}, OrderType::Market, Quantity{100}, Price{200}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Market, Quantity{150}, Price{200}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Market, Quantity{250}, Price{200}, TradeSide::Buy};

    Order sellOrder{OrderId{4}, OrderType::Market, Quantity{450}, Price{200}, TradeSide::Sell};

    order_book.addOrder(buyOrder1);
    order_book.addOrder(buyOrder2);
    order_book.addOrder(buyOrder3);
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_EQ(trades.size(), 3);
    EXPECT_EQ(trades[0].quantity, buyOrder1.getQuantity());
    EXPECT_EQ(trades[1].quantity, buyOrder2.getQuantity());
    EXPECT_EQ(trades[2].quantity, 200);

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(buyOrder3.getId()).getFilled(), 200);
}

TEST_F(OrderBookTests, triesManyExistingOrdersFromDifferentPriceLevels) {
    Order buyOrder1{OrderId{1}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Market, Quantity{20}, Price{250}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Market, Quantity{150}, Price{225}, TradeSide::Buy};
    Order buyOrder4{OrderId{4}, OrderType::Market, Quantity{250}, Price{210}, TradeSide::Buy};

    Order sellOrder{OrderId{5}, OrderType::Market, Quantity{600}, Price{200}, TradeSide::Sell};

    order_book.addOrder(buyOrder1);
    order_book.addOrder(buyOrder2);
    order_book.addOrder(buyOrder3);
    order_book.addOrder(buyOrder4);
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_EQ(trades.size(), 4);
    EXPECT_EQ(trades[0].quantity, buyOrder1.getQuantity());
    EXPECT_EQ(trades[1].quantity, buyOrder2.getQuantity());
    EXPECT_EQ(trades[2].quantity, buyOrder3.getQuantity());
    EXPECT_EQ(trades[3].quantity, buyOrder4.getQuantity());

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(sellOrder.getId()).getFilled(),
              buyOrder1.getInitialQuantity() + buyOrder2.getInitialQuantity() + buyOrder3.getInitialQuantity() +
              buyOrder4.getInitialQuantity());
}

TEST_F(OrderBookTests, modifyMarketOrderTest) {
    Order buyOrder1{OrderId{1}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Buy};

    order_book.addOrder(buyOrder1);
    order_book.modifyOrder(buyOrder1.getId(), Quantity{123}, Price{456});

    ASSERT_EQ(order_book.getOrder(buyOrder1.getId()).getQuantity(), Quantity{123});
    ASSERT_EQ(order_book.getOrder(buyOrder1.getId()).getPrice(), Price{456});
}

TEST_F(OrderBookTests, modifyMarketOrderGoesToTheEndOfPriceLevelQueueFifo) {
    Order buyOrder1{OrderId{1}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Market, Quantity{20}, Price{250}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Market, Quantity{40}, Price{250}, TradeSide::Buy};

    Order sellOrder{OrderId{4}, OrderType::Market, Quantity{30}, Price{200}, TradeSide::Sell};

    order_book.addOrder(buyOrder1);
    order_book.addOrder(buyOrder2);
    order_book.addOrder(buyOrder3);
    order_book.modifyOrder(buyOrder1.getId(), Quantity{30}, buyOrder1.getPrice());
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 20);
    EXPECT_EQ(trades[0].order_id_b, buyOrder2.getId());
    EXPECT_EQ(trades[1].quantity, 10);
    EXPECT_EQ(trades[1].order_id_b, buyOrder3.getId());

    EXPECT_EQ(order_book.getOrder(buyOrder1.getId()).getFilled(), 0); // previous orders filled the incoming order fully
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}


TEST_F(OrderBookTests, FillOrKillDoesNotGetFilledWhenNotEnoughQuantity) {
    order_book.addOrder({OrderId{1}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Market, Quantity{100}, Price{250}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2); // order discarded
    ASSERT_EQ(order_book.getBids().size(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, FillOrKillDoesNotGetFilledOnPriceMismatch) {
    order_book.addOrder({OrderId{1}, OrderType::Market, Quantity{100}, Price{240}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Market, Quantity{500}, Price{240}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2); // order discarded
    ASSERT_EQ(order_book.getBids().size(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, FillOrKillGetsFilledWhenQuantityAndPriceMatch) {
    order_book.addOrder({OrderId{1}, OrderType::Market, Quantity{200}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Market, Quantity{400}, Price{250}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 2);
    ASSERT_EQ(trades[0].quantity, 200);
    ASSERT_EQ(trades[0].order_id_a, 3);
    ASSERT_EQ(trades[0].order_id_b, 1);
    ASSERT_EQ(trades[1].quantity, 100);
    ASSERT_EQ(trades[1].order_id_a, 3);
    ASSERT_EQ(trades[1].order_id_b, 2);

    ASSERT_EQ(order_book.getOrdersCount(), 1); // 1 partial fill, 1 full
    ASSERT_EQ(order_book.getOrder(OrderId{2}).getQuantity(), 300);
}

TEST_F(OrderBookTests, FillOrKillGetsFilledWhenQuantityAndPriceMatchMultiLevel) {
    order_book.addOrder({OrderId{1}, OrderType::Market, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Market, Quantity{200}, Price{260}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 2);
    ASSERT_EQ(trades[0].quantity, 200);
    ASSERT_EQ(trades[0].price, 260);
    ASSERT_EQ(trades[0].order_id_a, 3);
    ASSERT_EQ(trades[0].order_id_b, 2);

    ASSERT_EQ(trades[1].quantity, 100);
    ASSERT_EQ(trades[1].price, 250);
    ASSERT_EQ(trades[1].order_id_a, 3);
    ASSERT_EQ(trades[1].order_id_b, 1);

    ASSERT_EQ(order_book.getOrdersCount(), 1); // 1 partial fill, 1 full
    ASSERT_EQ(order_book.getOrder(OrderId{1}).getQuantity(), 300);
    ASSERT_EQ(order_book.getBids().size(), 1); // one level cleared
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, FillOrKillRemovesLevelWhenEmptied) {
    order_book.addOrder({OrderId{1}, OrderType::Market, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Market, Quantity{200}, Price{260}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{600}, Price{250}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 2);

    ASSERT_EQ(order_book.getOrdersCount(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}