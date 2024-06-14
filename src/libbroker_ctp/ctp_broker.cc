#include "ctp_broker.h"
#include "ctp_trade_spi.h"

//using namespace autotrade;

namespace co {

    CTPBroker::~CTPBroker() {
        if (ctp_api_) {
            ctp_api_->RegisterSpi(nullptr);
            ctp_api_->Release();
            ctp_api_ = nullptr;
        }
        if (ctp_spi_) {
            delete ctp_spi_;
            ctp_spi_ = nullptr;
        }
    }

    void CTPBroker::OnInit() {
        LOG_INFO << "initialize CTPBroker ...";
        string ctp_investor_id = Config::Instance()->ctp_investor_id();
        MemTradeAccount acc {};
        acc.type = kTradeTypeFuture;
        strncpy(acc.fund_id, ctp_investor_id.c_str(), ctp_investor_id.length());
        acc.batch_order_size = 0;
        LOG_INFO << "aaaa: " << acc.fund_id << ", aaaa: " << ctp_investor_id;
        AddAccount(acc);
        ctp_spi_ = new CTPTradeSpi(this);
        thread_ = std::make_shared<std::thread>(std::bind(&CTPBroker::RunCtp, this));
        thread_->detach();
        ctp_spi_->Wait();
        LOG_INFO << "initialize CTPBroker successfully";
    }

    void CTPBroker::RunCtp() {
        bool disable_subscribe = Config::Instance()->disable_subscribe();
        ctp_api_ = CThostFtdcTraderApi::CreateFtdcTraderApi("");
        ctp_spi_->SetApi(ctp_api_);
        ctp_api_->RegisterSpi(ctp_spi_);
        string addr = Config::Instance()->ctp_trade_front();
        ctp_api_->RegisterFront((char*)addr.c_str());
        if (!disable_subscribe) {
            ctp_api_->SubscribePublicTopic(THOST_TERT_RESTART);
            ctp_api_->SubscribePrivateTopic(THOST_TERT_RESTART);
        }
        ctp_api_->Init();
        ctp_api_->Join();
    }


    void CTPBroker::OnQueryTradeAsset(MemGetTradeAssetMessage* req) {
        ctp_spi_->OnQueryTradeAsset(req);
    }

    void CTPBroker::OnQueryTradePosition(MemGetTradePositionMessage* req) {
        ctp_spi_->OnQueryTradePosition(req);
    }

    void CTPBroker::OnQueryTradeKnock(MemGetTradeKnockMessage* req) {
        ctp_spi_->OnQueryTradeKnock(req);
    }

    void CTPBroker::OnTradeOrder(MemTradeOrderMessage* req) {
        ctp_spi_->OnTradeOrder(req);
    }

    void CTPBroker::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
        ctp_spi_->OnTradeWithdraw(req);
    }
}
