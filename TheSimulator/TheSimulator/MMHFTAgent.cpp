//
// Created by teverist on 26/02/2024.
//
#include "MMHFTAgent.h"
#include "Simulation.h"

#include "ExchangeAgentMessagePayloads.h"


MMHFTAgent::MMHFTAgent(const Simulation* simulation)
    : Agent(simulation), m_exchange(""), m_depth(1), m_timeStep(1), m_rollingMean(0), m_vMin(1), m_vMax(1), m_vMinus(1), m_currentMidPrice(1), m_halfSpread(0.01) { }


MMHFTAgent::MMHFTAgent(const Simulation* simulation, const std::string& name)
    : Agent(simulation, name), m_exchange(""), m_depth(1), m_timeStep(1), m_rollingMean(0), m_vMin(1), m_vMax(1), m_vMinus(1), m_currentMidPrice(1), m_halfSpread(0.01) { }


void MMHFTAgent::configure(const pugi::xml_node& node, const std::string& configurationPath) {
    Agent::configure(node, configurationPath);

    pugi::xml_attribute att;
    if (!(att = node.attribute("exchange")).empty()) {
        m_exchange = simulation()->parameters().processString(att.as_string());
    }

    if (!(att = node.attribute("timeStep")).empty()) {
        m_timeStep = std::stoul(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("vMin")).empty()) {
        m_vMin = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("vMax")).empty()) {
        m_vMax = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("vMinus")).empty()) {
        m_vMinus = std::stod(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("depth")).empty()) {
        m_depth = std::stoul(simulation()->parameters().processString(att.as_string()));
    }

    if (!(att = node.attribute("rollingMean")).empty()) {
        m_rollingMean = Money(std::stod(simulation()->parameters().processString(att.as_string())));
    }

    if (!(att = node.attribute("halfSpread")).empty()) {
        m_halfSpread = Money(std::stod(simulation()->parameters().processString(att.as_string())));
    }
}

void MMHFTAgent::receiveMessage(const MessagePtr& msg) {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();

    if (msg->type == "EVENT_SIMULATION_START") {
        // trigger immediate market making
        simulation()->dispatchMessage(currentTimestamp, 0, this->name(), this->name(), "WAKEUP_FOR_MARKETMAKING", std::make_shared<EmptyPayload>());
    } else if (msg->type == "WAKEUP_FOR_MARKETMAKING") {
        // cancel the outstanding orders
        auto cpptr = std::make_shared<CancelOrdersPayload>();
        if (m_outstandingBuyOrder != 0) {
            cpptr->cancellations.push_back(CancelOrdersCancellation(m_outstandingBuyOrder, m_depth));
        }
        if (m_outstandingSellOrder != 0) {
            cpptr->cancellations.push_back(CancelOrdersCancellation(m_outstandingSellOrder, m_depth));
        }
        if (!cpptr->cancellations.empty()) {
            simulation()->dispatchMessage(currentTimestamp, 0, this->name(), m_exchange, "CANCEL_ORDERS", cpptr);
        }

        // Market Making Strategy
        // if predicts next order is buy
        // Submit sell at best price with volume U(vmin + vmax)
        // Subit buy at best price with volume v-
        // if predicts next order is sell
        // Submit buy at best price with volume U(vmin + vmax)
        // Submit sell at best price with volume v-
        // Each round, the market maker generates a prediction for the sign of the next period’s order using a simple w
        // period rolling-mean estimate w. When a market maker predicts that a buy order will arrive next,
        // she will set her sell limit order volume to a uniformly distributed random number
        // between vmin and vmax and her buy limit order volume to v-
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> distribution(m_vMin, m_vMax);

        Money newSellPrice = m_currentMidPrice + m_halfSpread;
        Money newBuyPrice = m_currentMidPrice - m_halfSpread;
        double buyVolume;
        double sellVolume;

        // if predicts next order is buy
        if (m_rollingMean > 0 && m_rollingMean > m_currentMidPrice) {
            sellVolume = distribution(gen);
            buyVolume = m_vMinus;
        } else if (m_rollingMean > 0 && m_rollingMean < m_currentMidPrice) {
            buyVolume = distribution(gen);
            sellVolume = m_vMinus;
        } else {
            return; // Error case
        }

        auto pptr = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Sell, sellVolume, newSellPrice);
        simulation()->dispatchMessage(currentTimestamp, 0, this->name(), m_exchange, "PLACE_ORDER_LIMIT", pptr);

        pptr = std::make_shared<PlaceOrderLimitPayload>(OrderDirection::Buy, buyVolume, newBuyPrice);
        simulation()->dispatchMessage(currentTimestamp, 0, this->name(), m_exchange, "PLACE_ORDER_LIMIT", pptr);

        // schedule the next market making
        scheduleMarketMaking();
    }
}


void MMHFTAgent::scheduleMarketMaking() {
    const Timestamp currentTimestamp = simulation()->currentTimestamp();
    simulation()->dispatchMessage(currentTimestamp, m_timeStep, this->name(), this->name(), "WAKEUP_FOR_MARKETMAKING", std::make_shared<EmptyPayload>());
}
