//
// Created by tobye on 26/02/2024.
//

#ifndef MMHFTAGENT_H
#define MMHFTAGENT_H

#include "Agent.h"
#include "Order.h"


class MMHFTAgent : public Agent {
public:
    MMHFTAgent(const Simulation* simulation);
    MMHFTAgent(const Simulation* simulation, const std::string& name);

    void configure(const pugi::xml_node& node, const std::string& configurationPath);

    // Inherited via Agent
    void receiveMessage(const MessagePtr& msg) override;

private:
    std::string m_exchange;

    unsigned int m_depth;

    Timestamp m_timeStep;

    // Rolling mean of the mid price
    Money m_rollingMean; // Rolling mean of the mid price
    double m_vMin; // Minimum volume for limit orders
    double m_vMax; // Maximum volume for limit orders
    double m_vMinus; // Volume for limit orders when predicting opposite

    Money m_currentMidPrice;
    Money m_halfSpread;

    void scheduleMarketMaking();
    OrderID m_outstandingBuyOrder;
    OrderID m_outstandingSellOrder;
};


#endif //MMHFTAGENT_H
