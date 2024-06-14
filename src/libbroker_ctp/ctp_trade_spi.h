// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <mutex>
#include "ctp_support.h"
#include "config.h"
#include "inner_future_master.h"

using namespace std;
using namespace x;
using namespace co;

namespace co {
    constexpr int kStartupStepInit = 0;
    constexpr int kStartupStepLoginOver = 1;
    constexpr int kStartupStepConfirmSettlementOver = 2;
    constexpr int kStartupStepGetContractsOver = 3;
    constexpr int kStartupStepGetInitPositionsOver = 4;

class CTPBroker;
class CTPTradeSpi : public CThostFtdcTraderSpi {
 public:
    CTPTradeSpi(CTPBroker* broker);
    virtual ~CTPTradeSpi() = default;

    inline void SetApi(CThostFtdcTraderApi* api) {
        api_ = api;
    }

    void ReqAuthenticate();  // 客户端认证请求
    void ReqUserLogin();  // 登陆请求
    void ReqSettlementInfoConfirm();  // 请求确认结算单，确认后才可以进行交易
    void ReqQryInstrument();
    void ReqQryInvestorPosition();

    void OnQueryTradeAsset(MemGetTradeAssetMessage* req);

    void OnQueryTradePosition(MemGetTradePositionMessage* req);

    void OnQueryTradeKnock(MemGetTradeKnockMessage* req);

    void OnTradeOrder(MemTradeOrderMessage* req);

    void OnTradeWithdraw(MemTradeWithdrawMessage* req);

//    void OnQueryTradeAsset(const std::string& raw_req);
//    void OnQueryTradePosition(const std::string& raw_req);
//    void OnQueryTradeKnock(const std::string& raw_req);
//    void OnTradeOrder(const std::string& raw_req);
//    void OnTradeWithdraw(const std::string& raw_req);

    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    virtual void OnFrontConnected();

    ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
    virtual void OnFrontDisconnected(int nReason);

    /// 客户端认证响应
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 登录请求响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 登出请求响应
    virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 投资者结算结果确认响应
    virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 请求查询交易所响应
    //virtual void OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

    /// 请求查询合约响应
    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 请求查询资金账户响应
    virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 请求查询投资者持仓响应
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 请求查询报单响应
    virtual void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 请求查询成交响应
    virtual void OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 报单录入请求响应
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 报单录入错误回报
    virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

    /// 报单操作请求响应
    virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    /// 报单操作错误回报
    virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);

    /// 报单通知
    virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

    /// 成交通知
    virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);

    /// 错误应答
    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    // 等待查询合约信息结束
    void Wait();

 protected:
    void Start();
    int GetRequestID();
    void PrepareQuery();
    string GetContractName(const string code);

 private:
    int state_ = 0;
    string broker_id_;
    string investor_id_;
    int64_t date_ = 0;
    int64_t front_id_ = 0;
    int64_t session_id_ = 0;

    int64_t pre_trading_day_ = 0;
    int64_t pre_trading_day_next_ = 0;

    CTPBroker* broker_ = nullptr;
    CThostFtdcTraderApi* api_ = nullptr;
    map<string, string> order_nos_; // CTP的OrderSysId到内部order_no的映射关系，用于在成交回报接收时查找对应的委托合同号

    InnerFutureMaster future_position_master_;
    int64_t pre_query_timestamp_ = 0; // 上次查询的时间戳，用于进行流控控制，CTP限制每秒只能查询一次

    int start_index_ = 0;
    mutex mutex_;
    string rsp_query_msg_;
    string query_cursor_;
    flatbuffers::FlatBufferBuilder req_fbb_;

    CThostFtdcTradingAccountField accout_field_;
    std::unordered_map<int, std::string> query_msg_;
    std::unordered_map<int, std::string> req_msg_;
    std::atomic_bool query_instruments_finish_;
    std::vector <CThostFtdcTradeField> all_ftdc_trades_;

    std::unordered_map<std::string, std::string> withdraw_msg_;  // OnRtnOrder中的RequestID是0，导致必须要自己维护, key是order_no
    std::vector<MemTradeKnock> all_knock_;
    std::unordered_map<std::string, MemTradePosition> all_pos_;
    std::unordered_map<std::string, std::pair<std::string, int>> all_instruments_;  // 保存合约名称与乘数
};
}  // namespace co
