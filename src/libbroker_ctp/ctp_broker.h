#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>

#include "ctp_support.h"
#include "mem_broker/mem_base_broker.h"
#include "mem_broker/mem_server.h"

namespace co {

    using namespace std;
    class CTPTradeSpi;

    /**
     * CTP柜台适配器
     * @author Guangxu Pan, bajizhh@gmail.com
     * @since 2017-04-13 16:26:18
     * @version 2017-04-13 16:26:18
     */
    class CTPBroker : public MemBroker {
    public:
        friend class CTPTradeSpi;
        explicit CTPBroker() = default;
        ~CTPBroker();
        
    protected:
        void OnInit();
        void RunCtp();

        void OnQueryTradeAsset(MemGetTradeAssetMessage* req);

        void OnQueryTradePosition(MemGetTradePositionMessage* req);

        void OnQueryTradeKnock(MemGetTradeKnockMessage* req);

        void OnTradeOrder(MemTradeOrderMessage* req);

        void OnTradeWithdraw(MemTradeWithdrawMessage* req);

    private:

        CThostFtdcTraderApi* ctp_api_ = nullptr;
        CTPTradeSpi* ctp_spi_ = nullptr;
        std::shared_ptr<std::thread> thread_; // 单独开一个线程供CTP使用，以免业务流程处理阻塞底层通信。
    };
}
