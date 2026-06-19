#include "ItchParser.hpp"

int main() {
    ITCH::ItchParser parser{"/home/kuba/CLionProjects/OrderBook/downloads/12302019.NASDAQ_ITCH50"};
    parser.parseAll();
}