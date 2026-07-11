#include "OrderBook.hpp"

#include <gtest/gtest.h>

using namespace ::testing;

struct OrderBookTests : Test {
    OrderBook order_book{};
    Order buyOrder{OrderId{1}, OrderType::Limit, Quantity{100}, Price{200}, TradeSide::Buy};
    Order sellOrder{OrderId{2}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Sell};
    Order matchingSellOrder{OrderId{3}, OrderType::Limit, Quantity{100}, Price{150}, TradeSide::Sell};
    Order matchingBuyOrder{OrderId{4}, OrderType::Limit, Quantity{100}, Price{350}, TradeSide::Buy};
};

TEST_F(OrderBookTests, canAddBuyLimitOrder) {
    order_book.addOrder(buyOrder);
    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(buyOrder.getId()), buyOrder);
}

TEST_F(OrderBookTests, canAddSellLimitOrder) {
    order_book.addOrder(sellOrder);
    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(sellOrder.getId()), sellOrder);
}

TEST_F(OrderBookTests, canCancelBuyLimitOrder) {
    order_book.addOrder(buyOrder);
    order_book.cancelOrder(buyOrder.getId());
    ASSERT_EQ(order_book.getOrdersCount(), 0);
    auto trades = order_book.addOrder(matchingSellOrder);
    ASSERT_TRUE(trades.empty()); // no matches, order book cleaned properly
    ASSERT_EQ(order_book.getOrdersCount(), 1);
}

TEST_F(OrderBookTests, canCancelSellLimitOrder) {
    order_book.addOrder(sellOrder);
    order_book.cancelOrder(sellOrder.getId());
    ASSERT_EQ(order_book.getOrdersCount(), 0);
    auto trades = order_book.addOrder(matchingBuyOrder);
    ASSERT_TRUE(trades.empty()); // no matches, order book cleaned properly
    ASSERT_EQ(order_book.getOrdersCount(), 1);
}

TEST_F(OrderBookTests, cancellingLastBidOfferRemovesPriceLevel) {
    order_book.addOrder(buyOrder);
    Order buyOrderSamePriceLevel{OrderId{123}, OrderType::Limit, Quantity{100}, buyOrder.getPrice(), TradeSide::Buy};
    order_book.addOrder(buyOrderSamePriceLevel);
    order_book.cancelOrder(buyOrder.getId());
    ASSERT_EQ(order_book.getBids().size(), 1);
    order_book.cancelOrder(buyOrderSamePriceLevel.getId());
    ASSERT_EQ(order_book.getBids().size(), 0);
}

TEST_F(OrderBookTests, cancellingLastAskOfferRemovesPriceLevel) {
    order_book.addOrder(sellOrder);
    Order sellOrderSamePriceLevel{OrderId{123}, OrderType::Limit, Quantity{100}, sellOrder.getPrice(), TradeSide::Sell};
    order_book.addOrder(sellOrderSamePriceLevel);
    order_book.cancelOrder(sellOrder.getId());
    ASSERT_EQ(order_book.getAsks().size(), 1);
    order_book.cancelOrder(sellOrderSamePriceLevel.getId());
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, sellLimitOrderDoesNotFillWhenPricesMismatch) {
    order_book.addOrder(buyOrder);
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, buyLimitOrderLimitDoesNotFillWhenPricesMismatch) {
    order_book.addOrder(sellOrder);

    auto trades = order_book.addOrder(buyOrder);
    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, buyLimitOrderFillsFullyWhenPricesAndQuantityMatch) {
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


TEST_F(OrderBookTests, sellLimitOrderFillsFullyWhenPricesAndQuantityMatch) {
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

TEST_F(OrderBookTests, buyLimitOrderFillsPartially) {
    order_book.addOrder(matchingSellOrder);
    Order partialFillOrder{
        OrderId{1}, OrderType::Limit, Quantity{matchingSellOrder.getInitialQuantity() / 2}, Price{200}, TradeSide::Buy
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

TEST_F(OrderBookTests, sellLimitOrderFillsPartially) {
    order_book.addOrder(buyOrder);
    Order partialFillOrder{
        OrderId{2}, OrderType::Limit, Quantity{buyOrder.getInitialQuantity() / 2}, Price{200}, TradeSide::Sell
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

TEST_F(OrderBookTests, triesManyExistingLimitOrdersFromSinglePriceLevel) {
    Order buyOrder1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{200}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Limit, Quantity{150}, Price{200}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Limit, Quantity{250}, Price{200}, TradeSide::Buy};

    Order sellOrder{OrderId{4}, OrderType::Limit, Quantity{450}, Price{200}, TradeSide::Sell};

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
    Order buyOrder1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Limit, Quantity{20}, Price{250}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Limit, Quantity{150}, Price{225}, TradeSide::Buy};
    Order buyOrder4{OrderId{4}, OrderType::Limit, Quantity{250}, Price{210}, TradeSide::Buy};

    Order sellOrder{OrderId{5}, OrderType::Limit, Quantity{600}, Price{200}, TradeSide::Sell};

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

TEST_F(OrderBookTests, buyLimitOrderMatchesCheaperAskBelowTopOfBook) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{100}, Price{150}, TradeSide::Sell});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{100}, Price{300}, TradeSide::Sell});

    auto trades = order_book.addOrder({OrderId{3}, OrderType::Limit, Quantity{100}, Price{200}, TradeSide::Buy});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 100);
    EXPECT_EQ(trades[0].price, 150);
    EXPECT_EQ(trades[0].order_id_b, 1);
    EXPECT_EQ(trades[0].aggressor_side, TradeSide::Buy);

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 1);
    ASSERT_EQ(order_book.getBids().size(), 0);
}

TEST_F(OrderBookTests, buyLimitOrderSweepsAskLevelsFromBestPrice) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Sell});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{20}, Price{225}, TradeSide::Sell});
    order_book.addOrder({OrderId{3}, OrderType::Limit, Quantity{150}, Price{210}, TradeSide::Sell});

    auto trades = order_book.addOrder({OrderId{4}, OrderType::Limit, Quantity{600}, Price{260}, TradeSide::Buy});

    ASSERT_EQ(trades.size(), 3);
    EXPECT_EQ(trades[0].price, 210);
    EXPECT_EQ(trades[0].order_id_b, 3);
    EXPECT_EQ(trades[1].price, 225);
    EXPECT_EQ(trades[1].order_id_b, 2);
    EXPECT_EQ(trades[2].price, 250);
    EXPECT_EQ(trades[2].order_id_b, 1);

    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
    ASSERT_EQ(order_book.getBids().size(), 1);
}

TEST_F(OrderBookTests, modifyLimitOrderTest) {
    Order buyOrder1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};

    order_book.addOrder(buyOrder1);
    order_book.modifyOrder(buyOrder1.getId(), Quantity{123}, Price{456});

    ASSERT_EQ(order_book.getOrder(buyOrder1.getId()).getQuantity(), Quantity{123});
    ASSERT_EQ(order_book.getOrder(buyOrder1.getId()).getPrice(), Price{456});
}

TEST_F(OrderBookTests, modifyLimitOrderGoesToTheEndOfPriceLevelQueueWhenModifiedQuantityIsBigger) {
    Order buyOrder1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Limit, Quantity{20}, Price{250}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Limit, Quantity{40}, Price{250}, TradeSide::Buy};

    Order sellOrder{OrderId{4}, OrderType::Limit, Quantity{30}, Price{200}, TradeSide::Sell};

    order_book.addOrder(buyOrder1);
    order_book.addOrder(buyOrder2);
    order_book.addOrder(buyOrder3);
    order_book.modifyOrder(buyOrder1.getId(), Quantity{130}, buyOrder1.getPrice());
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 20);
    EXPECT_EQ(trades[0].order_id_b, buyOrder2.getId());
    EXPECT_EQ(trades[1].quantity, 10);
    EXPECT_EQ(trades[1].order_id_b, buyOrder3.getId());

    EXPECT_EQ(order_book.getOrder(buyOrder1.getId()).getFilled(), 0); // previous orders filled the incoming order fully
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, modifyLimitOrderStaysOnTheSamePositionWhenModifiedQuantityIsLower) {
    Order buyOrder1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};
    Order buyOrder2{OrderId{2}, OrderType::Limit, Quantity{20}, Price{250}, TradeSide::Buy};
    Order buyOrder3{OrderId{3}, OrderType::Limit, Quantity{40}, Price{250}, TradeSide::Buy};

    Order sellOrder{OrderId{4}, OrderType::Limit, Quantity{40}, Price{200}, TradeSide::Sell};

    order_book.addOrder(buyOrder1);
    order_book.addOrder(buyOrder2);
    order_book.addOrder(buyOrder3);
    order_book.modifyOrder(buyOrder1.getId(), Quantity{30}, buyOrder1.getPrice());
    auto trades = order_book.addOrder(sellOrder);

    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 30);
    EXPECT_EQ(trades[0].order_id_b, buyOrder1.getId());
    EXPECT_EQ(trades[1].quantity, 10);
    EXPECT_EQ(trades[1].order_id_b, buyOrder2.getId());

    EXPECT_EQ(order_book.getOrder(buyOrder2.getId()).getFilled(), 10);
    ASSERT_EQ(order_book.getOrdersCount(), 2);
}

TEST_F(OrderBookTests, recudeExecutedOrder) {
    Order order{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};

    order_book.addOrder(order);
    order_book.reduceExecutedOrder(order.getId(), 40);
    ASSERT_EQ(order_book.getOrder(order.getId()).getQuantity(), 60);
    order_book.reduceExecutedOrder(order.getId(), 40);
    ASSERT_EQ(order_book.getOrder(order.getId()).getQuantity(), 20);
    order_book.reduceExecutedOrder(order.getId(), 20);
    ASSERT_EQ(order_book.getOrdersCount(), 0);
}

TEST_F(OrderBookTests, recudeExecutedOrderDoesNotCrashWhenReducesNonExistentOrder) {
    ASSERT_NO_THROW(order_book.reduceExecutedOrder(2137, 40));
}

TEST_F(OrderBookTests, replaceOrder) {
    Order order1{OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy};
    Order order2{OrderId{2}, OrderType::Limit, Quantity{150}, Price{350}, TradeSide::Buy};
    order_book.addOrder(order1);
    order_book.replaceOrder(order1.getId(), order2.getId(), order2.getQuantity(), order2.getPrice());
    ASSERT_EQ(order_book.getOrdersCount(), 1);
    ASSERT_EQ(order_book.getOrder(order2.getId()), order2);
}

TEST_F(OrderBookTests, replaceOrderDoesNotCrashWhenReplacingNonExistentOrder) {
    ASSERT_NO_THROW(order_book.replaceOrder(1,2,3,4));
}

TEST_F(OrderBookTests, FillOrKillDoesNotGetFilledWhenNotEnoughQuantity) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{100}, Price{250}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2); // order discarded
    ASSERT_EQ(order_book.getBids().size(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, FillOrKillDoesNotGetFilledOnPriceMismatch) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{100}, Price{240}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{500}, Price{240}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{300}, Price{250}, TradeSide::Sell});

    ASSERT_TRUE(trades.empty());
    ASSERT_EQ(order_book.getOrdersCount(), 2); // order discarded
    ASSERT_EQ(order_book.getBids().size(), 1);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, FillOrKillGetsFilledWhenQuantityAndPriceMatch) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{200}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{400}, Price{250}, TradeSide::Buy});
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
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{200}, Price{260}, TradeSide::Buy});
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
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{200}, Price{260}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{3}, OrderType::FillOrKill, Quantity{600}, Price{250}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 2);

    ASSERT_EQ(order_book.getOrdersCount(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
}

TEST_F(OrderBookTests, marketOrderDoesNotRestAtTheBook) {
    auto trades = order_book.addOrder({OrderId{3}, OrderType::Market, Quantity{700}, Price{270}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
    ASSERT_EQ(order_book.getOrdersCount(), 0);
}

TEST_F(OrderBookTests, IOCOrderDoesNotRestAtTheBook) {
    auto trades = order_book.addOrder({OrderId{3}, OrderType::ImmediateOrCancel, Quantity{700}, Price{270}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 0);
    ASSERT_EQ(order_book.getBids().size(), 0);
    ASSERT_EQ(order_book.getAsks().size(), 0);
    ASSERT_EQ(order_book.getOrdersCount(), 0);
}

TEST_F(OrderBookTests, marketOrderFillsAtEveryPrice) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{200}, Price{260}, TradeSide::Buy});
    order_book.addOrder({OrderId{3}, OrderType::Limit, Quantity{50}, Price{10000}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{4}, OrderType::Market, Quantity{700}, Price{270}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 3);

    ASSERT_EQ(order_book.getOrdersCount(), 0);
}

TEST_F(OrderBookTests, IOCOrderFillsLiquidityAndDoesNotRestAtTheBook) {
    order_book.addOrder({OrderId{1}, OrderType::Limit, Quantity{400}, Price{250}, TradeSide::Buy});
    order_book.addOrder({OrderId{2}, OrderType::Limit, Quantity{200}, Price{260}, TradeSide::Buy});
    order_book.addOrder({OrderId{3}, OrderType::Limit, Quantity{1000}, Price{240}, TradeSide::Buy});
    auto trades = order_book.addOrder({OrderId{4}, OrderType::ImmediateOrCancel, Quantity{700}, Price{245}, TradeSide::Sell});

    ASSERT_EQ(trades.size(), 2);
    ASSERT_EQ(trades[0].quantity, 200);
    ASSERT_EQ(trades[0].order_id_b, 2);
    ASSERT_EQ(trades[0].price, 260);
    ASSERT_EQ(trades[1].quantity, 400);
    ASSERT_EQ(trades[1].order_id_b, 1);
    ASSERT_EQ(trades[1].price, 250);

    ASSERT_EQ(order_book.getOrdersCount(), 1);
}
