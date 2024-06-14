#include <boost/algorithm/string.hpp>
#include "ctp_trade_spi.h"
#include "ctp_broker.h"

namespace co {
    CTPTradeSpi::CTPTradeSpi(CTPBroker* broker) : CThostFtdcTraderSpi(), broker_(broker) {
        start_index_ = x::RawTime();
        query_instruments_finish_.store(false);
        broker_id_ = Config::Instance()->ctp_broker_id();
        investor_id_ = Config::Instance()->ctp_investor_id();
        future_position_master_.set_risk_forbid_closing_today(Config::Instance()->risk_forbid_closing_today());
        future_position_master_.set_risk_max_today_opening_volume(Config::Instance()->risk_max_today_opening_volume());
    }

    void CTPTradeSpi::ReqAuthenticate() {
        LOG_INFO << "authenticate ...";
        string app_id = Config::Instance()->ctp_app_id();
        string product_info = Config::Instance()->ctp_product_info();
        string auth_code = Config::Instance()->ctp_auth_code();

        CThostFtdcReqAuthenticateField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, broker_id_.c_str());
        strcpy(req.UserID, investor_id_.c_str());
        strcpy(req.AppID, app_id.c_str());
        strcpy(req.UserProductInfo, product_info.c_str());
        strcpy(req.AuthCode, auth_code.c_str());
        int rc = 0;
        while ((rc = api_->ReqAuthenticate(&req, GetRequestID())) != 0) {
            LOG_WARN << "ReqAuthenticate failed: " << CtpApiError(rc) << ", retring ...";
            x::Sleep(CTP_FLOW_CONTROL_MS);
        }
    }

    void CTPTradeSpi::ReqUserLogin() {
        LOG_INFO << "login ...";
        string pwd = Config::Instance()->ctp_password();
        CThostFtdcReqUserLoginField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, broker_id_.c_str());
        strcpy(req.UserID, investor_id_.c_str());
        strcpy(req.Password, pwd.c_str());
        int ret = 0;
        while ((ret = api_->ReqUserLogin(&req, GetRequestID())) != 0) {
            LOG_WARN << "ReqUserLogin failed: " << CtpApiError(ret) << ", retring ...";
            x::Sleep(CTP_FLOW_CONTROL_MS);
        }
    }

    void CTPTradeSpi::ReqSettlementInfoConfirm() {
        LOG_INFO << "confirm settlement info ...";
        CThostFtdcSettlementInfoConfirmField req;
        memset(&req, 0, sizeof(req));
        strcpy(req.BrokerID, broker_id_.c_str());
        strcpy(req.InvestorID, investor_id_.c_str());
        int ret = api_->ReqSettlementInfoConfirm(&req, GetRequestID());
        if (ret != 0) {
            LOG_ERROR << "ReqSettlementInfoConfirm failed: " << CtpApiError(ret);
        }
    }

    void CTPTradeSpi::ReqQryInstrument() {
        LOG_INFO << "query all future contracts ...";
        PrepareQuery();
        CThostFtdcQryInstrumentField req;
        memset(&req, 0, sizeof(req));
        int ret = 0;
        while ((ret = api_->ReqQryInstrument(&req, GetRequestID())) != 0) {
            if (is_flow_control(ret)) {
                LOG_WARN << "ReqQryInstrument failed: " << CtpApiError(ret)
                    << ", retry in " << CTP_FLOW_CONTROL_MS << "ms ...";
                x::Sleep(CTP_FLOW_CONTROL_MS);
                continue;
            } else {
                LOG_ERROR << "ReqQryInstrument failed: " << CtpApiError(ret);
                break;
            }
        }
    }

    void CTPTradeSpi::ReqQryInvestorPosition() {
        PrepareQuery();
        string id = x::UUID();
        MemGetTradePositionMessage msg {};
        strncpy(msg.id, id.c_str(), id.length());
        strcpy(msg.fund_id, investor_id_.c_str());
        msg.timestamp = x::RawDateTime();
        OnQueryTradePosition(&msg);
    }

    void CTPTradeSpi::OnQueryTradeAsset(MemGetTradeAssetMessage* req) {
        PrepareQuery();
        rsp_query_msg_.clear();
        CThostFtdcQryTradingAccountField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, broker_id_.c_str());
        strcpy(field.InvestorID, investor_id_.c_str());
        strcpy(field.CurrencyID, "CNY");  // 只查询人民币资金
        int request_id = GetRequestID();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.emplace(std::make_pair(request_id, string(reinterpret_cast<const char*>(req), sizeof(MemGetTradeAssetMessage))));
        }
        int ret = api_->ReqQryTradingAccount(&field, request_id);
        LOG_INFO << "ReqQryTradingAccount, ret: " << ret;
        if (ret != 0) {
            LOG_ERROR << "query asset error: " << ret;
            string error = "query asset error:" + std::to_string(ret);
            strcpy(req->error, error.c_str());
            broker_->SendRtnMessage(string(reinterpret_cast<const char*>(req), sizeof(MemGetTradeAssetMessage)), kMemTypeQueryTradeAssetRep);
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.erase(request_id);
        }
    }

    void CTPTradeSpi::OnQueryTradePosition(MemGetTradePositionMessage* req) {
        PrepareQuery();
        rsp_query_msg_.clear();
        all_pos_.clear();
        CThostFtdcQryInvestorPositionField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, broker_id_.c_str());
        strcpy(field.InvestorID, investor_id_.c_str());
        int request_id = GetRequestID();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.emplace(std::make_pair(request_id, string(reinterpret_cast<const char*>(req), sizeof(MemGetTradePositionMessage))));
        }
        int ret = api_->ReqQryInvestorPosition(&field, request_id);
        if (ret != 0) {
            LOG_ERROR << "query positon error: " << ret;
            string error = "query positon error:" + std::to_string(ret);
            strcpy(req->error, error.c_str());
            broker_->SendRtnMessage(string(reinterpret_cast<const char*>(req), sizeof(MemGetTradePositionMessage)), kMemTypeQueryTradePositionRep);
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.erase(request_id);
        }
    }

    void CTPTradeSpi::OnQueryTradeKnock(MemGetTradeKnockMessage* req) {
        PrepareQuery();
        rsp_query_msg_.clear();
        rsp_query_msg_.clear();
        all_knock_.clear();
        CThostFtdcQryTradeField field;
        memset(&field, 0, sizeof(req));
        strcpy(field.BrokerID, broker_id_.c_str());
        strcpy(field.InvestorID, investor_id_.c_str());
        strcpy(field.TradeTimeStart, req->cursor);

        int request_id = GetRequestID();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.emplace(std::make_pair(request_id, string(reinterpret_cast<const char*>(req), sizeof(MemGetTradeKnockMessage))));
        }
        int ret = api_->ReqQryTrade(&field, request_id);
        if (ret != 0) {
            LOG_ERROR << "query kock error: " << ret;
            string error = "query knock error:" + std::to_string(ret);
            strcpy(req->error, error.c_str());
            broker_->SendRtnMessage(string(reinterpret_cast<const char*>(req), sizeof(MemGetTradeKnockMessage)), kMemTypeQueryTradeKnockRep);
            std::unique_lock<std::mutex> lock(mutex_);
            query_msg_.erase(request_id);
        }
    }

    void CTPTradeSpi::OnTradeOrder(MemTradeOrderMessage* req) {
        // Thost收到报单指令, 如果没有通过参数校验, 拒绝接受报单指令。用户就会收到OnRspOrderInsert消息, 其中包含了错误编码和错误消息。
        // 如果Thost接受了报单指令, 用户不会收到OnRspOrderInsert, 而会收到OnRtnOrder, 用来更新委托状态。
        // 交易所收到报单后, 通过校验。用户会收到OnRtnOrder、OnRtnTrade。
        // 如果交易所认为报单错误, 用户就会收到OnErrRtnOrderInsert。
        // -------------------------------------------------
        // 1.处理自动开平仓逻辑,
        // 2.执行两个风控策略检查：1.股指期货禁止自动平今仓;2.股指期货当日最大开仓数限制。如果风控检查失败, 则会抛出异常


        string _error_msg;
        int _item_size = req->items_size;
        if (_item_size == 1) {
            MemTradeOrder* order = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
            int64_t auto_oc_flag = order->oc_flag;
            LOG_INFO << "order, code: " << order->code
                << ", bs_flag: " << req->bs_flag
                << ", oc_flag: " << order->oc_flag
                << ", volume: " << order->volume;

            co::fbs::TradeOrderT fb_order;
            fb_order.code = order->code;
            fb_order.bs_flag = req->bs_flag;
            fb_order.oc_flag = order->oc_flag;
            fb_order.volume = order->volume;
            fb_order.price = order->price;
            if (order->oc_flag == kOcFlagAuto) {
                auto_oc_flag = future_position_master_.GetAutoOcFlag(fb_order);
            } else if (order->oc_flag == 100) {
                auto_oc_flag = future_position_master_.GetCloseYestodayFlag(fb_order);
            }

            LOG_INFO << "auto_oc_flag: " << auto_oc_flag;
            string ctp_code;
            for (size_t i = 0; i < strlen(order->code); ++i) {
                if (order->code[i] != '.') {
                    ctp_code[i] = order->code[i];
                } else {
                    break;
                }
            }
          if (order->market == co::kMarketCZCE) {
                DeleteCzceCode(ctp_code);
            }
            CThostFtdcInputOrderField _req;
            memset(&_req, 0, sizeof(_req));
            strcpy(_req.BrokerID, broker_id_.c_str());
            strcpy(_req.InvestorID, investor_id_.c_str());
            strcpy(_req.InstrumentID, ctp_code.c_str());
            int request_id = GetRequestID();
            sprintf(_req.OrderRef, "%d", request_id);
            _req.Direction = bs_flag2ctp(req->bs_flag);
            _req.CombOffsetFlag[0] = oc_flag2ctp(auto_oc_flag);
            _req.CombHedgeFlag[0] = THOST_FTDC_CIDT_Speculation;
            _req.VolumeTotalOriginal = order->volume;
            _req.VolumeCondition = THOST_FTDC_VC_AV;  /// 成交量类型：任何数量
            _req.MinVolume = 1;  // 最小成交量
            _req.IsAutoSuspend = 0;  /// 自动挂起标志: 否
            _req.UserForceClose = 0;  /// 用户强评标志: 否
            _req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;  /// 强平原因: 非强平
            _req.IsSwapOrder = 0;  // 互换单标志
            _req.OrderPriceType = order_price_type2ctp(order->price_type);  // 报单价格条件
            _req.LimitPrice = order->price;  /// 价格
            _req.TimeCondition = THOST_FTDC_TC_GFD; ///有效期类型
            _req.ContingentCondition = THOST_FTDC_CC_Immediately; // 触发条件：立即

            {
                std::unique_lock<std::mutex> lock(mutex_);
                req_msg_.emplace(std::make_pair(request_id, string(reinterpret_cast<const char*>(req), sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder))));
            }
            int ret = api_->ReqOrderInsert(&_req, request_id);
            LOG_INFO << "ReqOrderInsert, request_id: " << request_id;
            if (ret != 0) {
                _error_msg = "order faild, ret: " + std::to_string(ret) + ", " + CtpApiError(ret);
                std::unique_lock<std::mutex> lock(mutex_);
                req_msg_.erase(request_id);
            } else {
                stringstream ss;
                ss << front_id_ << "_" << session_id_ << "_" << _req.OrderRef << "_" << _req.InstrumentID;
                string order_no = ss.str();
                fb_order.order_no = order_no;
                fb_order.oc_flag = auto_oc_flag;
                future_position_master_.Update(fb_order);
            }
        } else {
            _error_msg = "order item is not valid.";
        }

        if (_error_msg.length() > 0) {
            strcpy(req->error, _error_msg.c_str());
            req->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(reinterpret_cast<const char*>(req), sizeof(MemTradeOrderMessage)), kMemTypeTradeOrderRep);
        }
    }

    void CTPTradeSpi::OnTradeWithdraw(MemTradeWithdrawMessage* req) {
        // 撤单响应和回报
        // Thost收到撤单指令, 如果没有通过参数校验, 拒绝接受撤单指令. 用户就会收到OnRspOrderAction 消息, 其中包含了错误编码和错误消息.
        // 如果 Thost 接受了撤单指令, 用户不会收到OnRspOrderAction, 而会收到OnRtnOrder, 用来更新委托状态.
        // 交易所收到撤单后, 通过校验, 执行了撤单操作. 用户会收到OnRtnOrder
        // 如果交易所认为报单错误, 用户就会收到OnErrRtnOrderAction
        string _error_msg;

        CThostFtdcInputOrderActionField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, broker_id_.c_str());
        strcpy(field.InvestorID, investor_id_.c_str());
        field.ActionFlag = THOST_FTDC_AF_Delete;  // 操作标志: 删除
        string order_no = req->order_no;
        vector<string> vec_info;
        boost::split(vec_info, order_no, boost::is_any_of("_"), boost::token_compress_on);
        if (vec_info.size() == 4) {
            int front_id = atoi(vec_info[0].c_str());
            int session_id = atoi(vec_info[1].c_str());
            field.FrontID = front_id;
            field.SessionID = session_id;
            strncpy(field.OrderRef, vec_info[2].c_str(), vec_info[2].length());
            strncpy(field.InstrumentID, vec_info[3].c_str(), vec_info[3].length());
            int _request_id = GetRequestID();
            // 撤单错误使用withdraw_msg_, 正确时使用req_msg_
            {
                std::unique_lock<std::mutex> lock(mutex_);
                req_msg_.emplace(std::make_pair(_request_id, string(reinterpret_cast<const char*>(req), sizeof(MemTradeWithdrawMessage))));
                withdraw_msg_.emplace(std::make_pair(order_no, string(reinterpret_cast<const char*>(req), sizeof(MemTradeWithdrawMessage))));
            }
            LOG_INFO << "ReqOrderAction, request_id: " << _request_id;
            int _ret = api_->ReqOrderAction(&field, _request_id);
            if (_ret != 0) {
                _error_msg = "withdraw faild, ret: " + std::to_string(_ret) + ", " + CtpApiError(_ret);
                std::unique_lock<std::mutex> lock(mutex_);
                req_msg_.erase(_request_id);
                withdraw_msg_.erase(order_no);
            }
        } else {
            _error_msg = "not valid order_no: " + order_no;
        }

        if (_error_msg.length() > 0) {
            strcpy(req->error, _error_msg.c_str());
            req->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(reinterpret_cast<const char*>(req), sizeof(MemTradeWithdrawMessage)), kMemTypeTradeWithdrawRep);
        }
    }

    // ------------------------------------------------------------------------
    /// 当客户端与交易后台建立起通信连接时(还未登录前), 该方法被调用
    void CTPTradeSpi::OnFrontConnected() {
        LOG_INFO << "connect to CTP trade server ok";
        Start();
    }

    /// 当客户端与交易后台通信连接断开时, 该方法被调用. 当发生这个情况后,  API会自动重新连接, 客户端可不做处理.
    void CTPTradeSpi::OnFrontDisconnected(int nReason) {
        stringstream ss;
        ss << "ret=" << nReason << ", msg=";
        switch (nReason) {
            case 0x1001:  //  0x1001 网络读失败
                ss << "read error";
                break;
            case 0x1002:  // 0x1002 网络写失败
                ss << "write error";
                break;
            case 0x2001:  // 0x2001 接收心跳超时
                ss << "recv heartbeat timeout";
                break;
            case 0x2002:  // 0x2002 发送心跳失败
                ss << "send heartbeat timeout";
                break;
            case 0x2003:  // 0x2003 收到错误报文
                ss << "recv broken data";
                break;
            default:
                ss << "unknown error";
                break;
        }
        LOG_INFO << "connection is broken: " << ss.str();
        x::Sleep(2000);
    }

    void CTPTradeSpi::OnRspUserLogout(CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            LOG_INFO << "logout ok";
        } else {
            LOG_ERROR << "logout failed: ret=" << pRspInfo->ErrorID << ", msg=" << CtpToUTF8(pRspInfo->ErrorMsg);
        }
    }

    /// 客户端认证响应//
    void CTPTradeSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField* pRspAuthenticateField, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            LOG_INFO << "authenticate ok";
            ReqUserLogin();
        } else {
            LOG_ERROR << "authenticate failed: " << CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        }
    }

    /// 登录请求响应//
    void CTPTradeSpi::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            date_ = atoi(api_->GetTradingDay());
            front_id_ = pRspUserLogin->FrontID;
            session_id_ = pRspUserLogin->SessionID;
            // order_ref_ = x::ToInt64(x::Trim(pRspUserLogin->MaxOrderRef));
            LOG_INFO << "login ok: trading_day = " << date_ << ", front_id = " << front_id_ << ", session_id = " << session_id_ << ", max_order_ref = " << pRspUserLogin->MaxOrderRef;
            if (IsMonday(date_)) {
                pre_trading_day_ = x::PreDay(date_, 3);
                pre_trading_day_next_ = x::PreDay(date_, 2);
            } else {
                pre_trading_day_ = x::PreDay(date_);
                pre_trading_day_next_ = date_;
            }
            LOG_INFO << "pre_trading_day: " << pre_trading_day_ << ", pre_trading_day_next: " << pre_trading_day_next_;
            if (date_ < 19700101 || date_ > 29991231) {
                LOG_ERROR << "illegal trading day: " << date_;
            } else {
                state_ = kStartupStepLoginOver;  // 登陆成功
                ReqSettlementInfoConfirm();
            }
        } else {
            LOG_ERROR << "login failed: " << CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        }
    }

    void CTPTradeSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            string date = pSettlementInfoConfirm ? pSettlementInfoConfirm->ConfirmDate : "";
            LOG_INFO << "confirm settlement info ok: confirm_date = " << date;
        } else {
            LOG_WARN << "confirm settlement info failed: " << CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        }
        state_ = kStartupStepConfirmSettlementOver;
        ReqQryInstrument();
    }

    void CTPTradeSpi::OnRspQryInstrument(CThostFtdcInstrumentField* p, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
            if (p) {
                string ctp_code = p->InstrumentID;
                int64_t market = ctp_market2std(p->ExchangeID);
                if (market == co::kMarketCZCE) {
                    InsertCzceCode(ctp_code);
                }
                if (market) {
                    string suffix = MarketToSuffix(market).data();
                    string code = ctp_code + suffix;
                    int _multiple = p->VolumeMultiple > 0 ? p->VolumeMultiple : 1;
                    all_instruments_.insert(make_pair(code, make_pair(x::GBKToUTF8(x::Trim(p->InstrumentName)), _multiple)));
                }
            }
            if (bIsLast) {
                LOG_INFO << "query all future contracts ok: contracts = " << all_instruments_.size();
                query_instruments_finish_.store(true);
                for (auto& it : all_ftdc_trades_) {
                    OnRtnTrade(&it);
                }
                all_ftdc_trades_.clear();
                state_ = kStartupStepGetContractsOver;
                ReqQryInvestorPosition();
            }
        } else {
            LOG_ERROR << "query all future contracts failed: " << CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        }
    }

    void CTPTradeSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pTradingAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        try {
            if (bIsLast) {
                string req_message;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    auto it = query_msg_.find(nRequestID);
                    if (it != query_msg_.end()) {
                        req_message = it->second;
                        query_msg_.erase(it);
                    } else {
                        LOG_ERROR << "OnRspQryTradingAccount, not find nRequestID: " << nRequestID;
                    }
                }

                if (req_message.empty()) {
                    LOG_ERROR << "OnRspQryTradingAccount, req_message is empty. ";
                    return;
                }
                int total_num = 0;
                string error;
                MemTradeAsset item {};
                if (pTradingAccount) {
                    total_num = 1;
                    item.balance = (0);
                    item.usable = pTradingAccount->Available;
                    item.margin = pTradingAccount->CurrMargin;
                    item.equity = ctp_equity(pTradingAccount);
                    strcpy(item.fund_id, investor_id_.c_str());
                }
                if (pRspInfo && pRspInfo->ErrorID != 0) {
                    error = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
                }
                MemGetTradeAssetMessage* req = (MemGetTradeAssetMessage*)(req_message.data());
                int length = sizeof(MemGetTradeAssetMessage) + sizeof(MemTradeAsset) * total_num;
                char buffer[length] = "";
                MemGetTradeAssetMessage* rep = (MemGetTradeAssetMessage*)buffer;
                memcpy(rep, req, sizeof(MemGetTradeAssetMessage));
                rep->items_size = total_num;
                if (!error.empty()) {
                    strcpy(rep->error, error.c_str());
                } else if (total_num == 1) {
                    MemTradeAsset* first = (MemTradeAsset*)((char*)buffer + sizeof(MemGetTradeAssetMessage));
                    memcpy(first, &item, sizeof(MemTradeAsset));
                }
                broker_->SendRtnMessage(string(buffer, length), kMemTypeQueryTradeAssetRep);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspQryTradingAccount: " << e.what();
        }
    }

    void CTPTradeSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        try {
            if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
                if (pInvestorPosition) {
                    LOG_INFO << "OnRspQryInvestorPosition, InstrumentID: " << pInvestorPosition->InstrumentID
                        << ", PosiDirection: " << pInvestorPosition->PosiDirection
                        << ", YdPosition: " << pInvestorPosition->YdPosition
                        << ", Position: " << pInvestorPosition->Position
                        << ", LongFrozen: " << pInvestorPosition->LongFrozen
                        << ", ShortFrozen: " << pInvestorPosition->ShortFrozen;
                    string ctp_code = pInvestorPosition->InstrumentID;
                    int64_t market = ctp_market2std(pInvestorPosition->ExchangeID);
                    if (market == co::kMarketCZCE) {
                        InsertCzceCode(ctp_code);
                    }
                    string suffix = MarketToSuffix(market).data();
                    string code = ctp_code + suffix;
                    // int64_t hedge_flag = ctp_hedge_flag2std(pInvestorPosition->HedgeFlag);
                    int64_t bs_flag = ctp_ls_flag2std(pInvestorPosition->PosiDirection);
                    auto it = all_pos_.find(code);
                    if (it == all_pos_.end()) {
                        MemTradePosition item {};
                        strcpy(item.fund_id, investor_id_.c_str());
                        item.market = market;
                        strcpy(item.code, code.c_str());
                        auto itor = all_instruments_.find(code);
                        if (itor != all_instruments_.end()) {
                            string& name = itor->second.first;
                            strcpy(item.name, name.c_str());
                        }
                        all_pos_.insert(std::make_pair(code, item));
                        it = all_pos_.find(code);
                    }
                    // YdPositionYdPositionYdPositionYdPosition YdPosition 表示昨日收盘时持仓数量(静态数值, 日间不随着开平而变化)//
                    // 当前的昨持仓 = 当前持仓数量 - 今开仓数量//
                    if (bs_flag == kBsFlagBuy) {
                        it->second.long_pre_volume = it->second.long_pre_volume + pInvestorPosition->YdPosition;
                        it->second.long_volume = it->second.long_volume + pInvestorPosition->Position;
                    } else if (bs_flag == kBsFlagSell) {
                        it->second.short_pre_volume = it->second.short_pre_volume + pInvestorPosition->YdPosition;
                        it->second.short_volume = it->second.short_volume + pInvestorPosition->Position;
                    }
                }
            } else {
                rsp_query_msg_ = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            }

            if (bIsLast) {
                for (auto it = all_pos_.begin(); it != all_pos_.end(); ++it) {
                    LOG_INFO << it->second.code
                        << ", long_volume: " << it->second.long_volume
                        << ", long_pre_volume: " << it->second.long_pre_volume
                        << ", short_volume: " << it->second.short_volume
                        << ", short_pre_volume: " << it->second.short_pre_volume;
                }
                if (state_ >= kStartupStepGetInitPositionsOver) {
                    string req_message;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        auto it = query_msg_.find(nRequestID);
                        if (it != query_msg_.end()) {
                            req_message = it->second;
                            query_msg_.erase(it);
                        } else {
                            LOG_ERROR << "OnRspQryInvestorPosition, not find nRequestID: " << nRequestID;
                        }
                    }

                    if (req_message.empty()) {
                        LOG_ERROR << "OnRspQryInvestorPosition, req_message is empty. ";
                        return;
                    }
                    MemGetTradePositionMessage* req = (MemGetTradePositionMessage*)(req_message.data());
                    int total_num = all_pos_.size();
                    int length = sizeof(MemGetTradePositionMessage) + sizeof(MemTradePosition) * total_num;
                    char buffer[length] = "";
                    MemGetTradePositionMessage* rep = (MemGetTradePositionMessage*)buffer;
                    memcpy(rep, req, sizeof(MemGetTradePositionMessage));
                    rep->items_size = total_num;
                    if (total_num) {
                        int index = 0;
                        MemTradePosition *first = (MemTradePosition * )((char *)buffer + sizeof(MemGetTradePositionMessage));
                        for (auto it = all_pos_.begin(); it != all_pos_.end(); ++it) {
                            MemTradePosition *pos = first + index++;
                            memcpy(pos, &it->second, sizeof(MemTradePosition));
                        }
                    }
                    if (!rsp_query_msg_.empty()) {
                        strcpy(rep->error, rsp_query_msg_.c_str());
                    }
                    broker_->SendRtnMessage(string(buffer, length), kMemTypeQueryTradePositionRep);
                } else {
                    vector<MemTradePosition> _positions;
                    for (auto it = all_pos_.begin(); it != all_pos_.end(); ++it) {
                        _positions.push_back(it->second);
                    }
                    future_position_master_.Init(_positions);
                    state_ = kStartupStepGetInitPositionsOver;
                }
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspQryInvestorPosition: " << e.what();
        }
    }

    /// 请求查询报单响应//
    void CTPTradeSpi::OnRspQryOrder(CThostFtdcOrderField* pOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
    }

    /// 请求查询成交响应//
    void CTPTradeSpi::OnRspQryTrade(CThostFtdcTradeField* pTrade, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        try {
            if (pRspInfo == NULL || pRspInfo->ErrorID == 0) {
                if (pTrade) {
                    string order_sys_id = x::Trim(pTrade->OrderSysID);
                    string order_no;
                    map<string, string>::iterator itr_order_no = order_nos_.find(order_sys_id);
                    if (itr_order_no != order_nos_.end()) {
                        order_no = itr_order_no->second;
                    }
                    if (!order_no.empty()) {
                        string ctp_code = pTrade->InstrumentID;
                        int64_t market = ctp_market2std(pTrade->ExchangeID);
                        if (market == co::kMarketCZCE) {
                            InsertCzceCode(ctp_code);
                        }
                        string suffix = MarketToSuffix(market).data();
                        string code = ctp_code + suffix;
                        string match_no = x::Trim(pTrade->TradingDay) + "_" + x::Trim(pTrade->TradeID);
                        double match_amount = pTrade->Price * pTrade->Volume;
                        string name;
                        auto it = all_instruments_.find(code);
                        if (it != all_instruments_.end()) {
                            name = it->second.first;
                            match_amount = match_amount * it->second.second;
                        }

                        MemTradeKnock item {};
                        strcpy(item.fund_id, investor_id_.c_str());
                        if (strcmp(pTrade->TradeTime, "06:00:00") > 0 && strcmp(pTrade->TradeTime, "18:00:00") <= 0) {
                            item.timestamp = CtpTimestamp(atoll(pTrade->TradingDay), pTrade->TradeTime);
                        } else if (strcmp(pTrade->TradeTime, "18:00:00") > 0 && strcmp(pTrade->TradeTime, "23:59:59") <= 0) {
                            item.timestamp = CtpTimestamp(pre_trading_day_, pTrade->TradeTime);
                        } else {
                            item.timestamp = CtpTimestamp(pre_trading_day_next_, pTrade->TradeTime);
                        }

                        strcpy(item.code, code.c_str());
                        strcpy(item.name, name.c_str());
                        item.market = market;
                        strcpy(item.order_no, order_no.c_str());
                        strcpy(item.match_no, match_no.c_str());

                        item.bs_flag = ctp_bs_flag2std(pTrade->Direction);
                        item.oc_flag = ctp_oc_flag2std(pTrade->OffsetFlag);
                        item.match_type = kMatchTypeOK;
                        item.match_volume = pTrade->Volume;
                        item.match_price = pTrade->Price;
                        item.match_amount = match_amount;
                        all_knock_.emplace_back(item);
                    } else {
                        LOG_WARN << "ignore knock because no order_no found of order_sys_id: " << order_sys_id;
                    }
                    query_cursor_ = pTrade->TradeTime;
                }
            } else {
                rsp_query_msg_ = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            }

            if (bIsLast) {
                string req_message;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    auto it = query_msg_.find(nRequestID);
                    if (it != query_msg_.end()) {
                        req_message = it->second;
                        query_msg_.erase(it);
                    } else {
                        LOG_ERROR << "OnRspQryTrade, not find nRequestID: " << nRequestID;
                    }
                }
                if (req_message.empty()) {
                    LOG_ERROR << "OnRspQryTrade, req_message is empty. ";
                    return;
                }

                MemGetTradeKnockMessage* req = (MemGetTradeKnockMessage*)(req_message.data());
                int total_num = all_knock_.size();
                int length = sizeof(MemGetTradeKnockMessage) + sizeof(MemTradeKnock) * total_num;
                char buffer[length] = "";
                MemGetTradeKnockMessage* rep = (MemGetTradeKnockMessage*)buffer;
                memcpy(rep, req, sizeof(MemGetTradeKnockMessage));
                rep->items_size = total_num;

                if (total_num > 0) {
                    MemTradeKnock* first = (MemTradeKnock*)((char*)buffer + sizeof(MemGetTradeKnockMessage));
                    for (int i = 0; i < total_num; i++) {
                        MemTradeKnock* knock = first + i;;
                        memcpy(knock, &all_knock_[i], sizeof(MemTradeKnock));
                    }
                }
                if (!rsp_query_msg_.empty()) {
                    strcpy(rep->error, query_cursor_.c_str());
                }
                strcpy(rep->next_cursor, rsp_query_msg_.c_str());
                broker_->SendRtnMessage(string(buffer, length), kMemTypeQueryTradeKnockRep);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspQryTrade: " << e.what();
        }
    }

    /// 报单录入请求响应(CTP打回的废单会通过该函数返回)
    void CTPTradeSpi::OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        if (pInputOrder) {
            LOG_INFO << __FUNCTION__ << ", InstrumentID: " << pInputOrder->InstrumentID
                << ", OrderRef: " << pInputOrder->OrderRef
                << ", ExchangeID: " << pInputOrder->ExchangeID
                << ", Direction: " << pInputOrder->Direction
                << ", CombOffsetFlag: " << pInputOrder->CombOffsetFlag
                << ", VolumeTotalOriginal: " << pInputOrder->VolumeTotalOriginal
                << ", nRequestID: " << nRequestID;
        }

        try {
            string req_message;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = req_msg_.find(nRequestID);
                if (it != req_msg_.end()) {
                    req_message = it->second;
                    req_msg_.erase(it);
                } else {
                    LOG_ERROR << "OnRspOrderInsert, not find nRequestID: " << nRequestID;
                }
            }

            if (req_message.empty()) {
                LOG_ERROR << "OnRspOrderInsert, req_message is empty. ";
                return;
            }
            MemTradeOrderMessage* req = (MemTradeOrderMessage*)(req_message.data());
            int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
            char buffer[length] = "";
            memcpy(buffer, req, length);
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
            string error = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            strcpy(rep->error, error.c_str());
            rep->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);

            {
                MemTradeOrder* order = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
                co::fbs::TradeOrderT _order;
                _order.trade_type = kTradeTypeFuture;
                _order.fund_id = investor_id_;
                stringstream ss;
                ss << front_id_<< "_" << session_id_ << "_" << pInputOrder->OrderRef << "_" << pInputOrder->InstrumentID;
                string order_no = ss.str();
                _order.order_no = order_no;
                _order.market = order->market;
                _order.code = order->code;
                _order.bs_flag = req->bs_flag;
                _order.oc_flag = ctp_oc_flag2std(pInputOrder->CombOffsetFlag[0]);
                _order.volume = order->volume;
                _order.price = order->price;
                _order.match_volume = 0;
                _order.withdraw_volume = order->volume;
                future_position_master_.Update(_order);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnOrderTicketError: " << e.what();
        }
    }

    // 交易所打回的废单会通过该函数返回//
    void CTPTradeSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo) {
        if (pInputOrder) {
            LOG_INFO << "OnErrRtnOrderInsert, InstrumentID: " << pInputOrder->InstrumentID
                << ", OrderRef: " << pInputOrder->OrderRef
                << ", Direction: " << pInputOrder->Direction
                << ", LimitPrice: " << pInputOrder->LimitPrice
                << ", VolumeTotalOriginal: " << pInputOrder->VolumeTotalOriginal
                << ", ExchangeID: " << pInputOrder->ExchangeID
                << ", CombOffsetFlag: " << pInputOrder->CombOffsetFlag
                << ", nRequestID: " << pInputOrder->RequestID;
        }

        try {
            string req_message;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = req_msg_.find(pInputOrder->RequestID);
                if (it != req_msg_.end()) {
                    req_message = it->second;
                    req_msg_.erase(it);
                } else {
                    LOG_ERROR << "OnErrRtnOrderInsert, not find nRequestID: " << pInputOrder->RequestID;
                    return;
                }
            }

            if (req_message.empty()) {
                LOG_ERROR << "OnErrRtnOrderInsert, req_message is empty. ";
                return;
            }
            MemTradeOrderMessage* req = (MemTradeOrderMessage*)(req_message.data());
            int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
            char buffer[length] = "";
            memcpy(buffer, req, length);
            MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
            string error = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            strcpy(rep->error, error.c_str());
            rep->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);

            {
                MemTradeOrder* order = (MemTradeOrder*)((char*)req + sizeof(MemTradeOrderMessage));
                co::fbs::TradeOrderT _order;
                _order.trade_type = kTradeTypeFuture;
                _order.fund_id = investor_id_;
                stringstream ss;
                ss << front_id_<< "_" << session_id_ << "_" << pInputOrder->OrderRef << "_" << pInputOrder->InstrumentID;
                string order_no = ss.str();
                _order.order_no = order_no;
                _order.market = order->market;
                _order.code = order->code;
                _order.bs_flag = req->bs_flag;
                _order.oc_flag = ctp_oc_flag2std(pInputOrder->CombOffsetFlag[0]);
                _order.volume = order->volume;
                _order.price = order->price;
                _order.match_volume = 0;
                _order.withdraw_volume = order->volume;
                future_position_master_.Update(_order);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnOrderTicketError: " << e.what();
        }
    }

    /// 报单操作请求响应//
    void CTPTradeSpi::OnRspOrderAction(CThostFtdcInputOrderActionField* pInputOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        try {
            LOG_INFO << __FUNCTION__ << ", nRequestID: " << nRequestID << ", ErrorId: " << pRspInfo->ErrorID;
            string req_message;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = req_msg_.find(nRequestID);
                if (it != req_msg_.end()) {
                    req_message = it->second;
                    req_msg_.erase(it);
                } else {
                    LOG_ERROR << "not find nRequestID: " << nRequestID;
                }
            }

            MemTradeWithdrawMessage* req = (MemTradeWithdrawMessage*)(req_message.data());
            int length = sizeof(MemTradeWithdrawMessage);
            char buffer[length] = "";
            memcpy(buffer, req, length);
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
            string error = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            strcpy(rep->error, error.c_str());
            rep->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
        } catch (std::exception& e) {
            LOG_ERROR << "OnRspOrderAction: " << e.what();
        }
    }

    void CTPTradeSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField* pOrderAction, CThostFtdcRspInfoField* pRspInfo) {
        try {
            LOG_INFO << __FUNCTION__ << ", nRequestID: " << pOrderAction->RequestID << ", ErrorId: " << pRspInfo->ErrorID;

            string req_message;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = req_msg_.find(pOrderAction->RequestID);
                if (it != req_msg_.end()) {
                    req_message = it->second;
                    req_msg_.erase(it);
                } else {
                    LOG_ERROR << "not find nRequestID: " << pOrderAction->RequestID;
                }
            }

            if (req_message.empty()) {
                LOG_ERROR << "req_message is empty. ";
                return;
            }
            MemTradeWithdrawMessage* req = (MemTradeWithdrawMessage*)(req_message.data());
            int length = sizeof(MemTradeWithdrawMessage);
            char buffer[length] = "";
            memcpy(buffer, req, length);
            MemTradeWithdrawMessage* rep = (MemTradeWithdrawMessage*)buffer;
            string error = CtpError(pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            strcpy(rep->error, error.c_str());
            rep->rep_time = x::RawDateTime();
            broker_->SendRtnMessage(string(buffer, length), kMemTypeTradeWithdrawRep);
        } catch (std::exception& e) {
            LOG_ERROR << "OnErrRtnOrderAction: " << e.what();
        }
    }

/// 报单通知, 报单成功或状态变化
// 报单流程：
// 1.客户端调用ReqOrderInsert进行报单;
// 2.CTP接收到报单请求
//   2.1 如果CTP认为报单成功, 会回调一次OnRtnOrder, 此时有FrontID、SessionId和OrderRef, 此时没有OrderSysID;
//   2.2 如果CTP认为报单失败, 会回调OnRspOrderInsert返回报单失败;
// 3.CTP将委托报给交易所
//   3.1如果交易所返回报单成功, 则会再回调一次OnRtnOrder, 此时会有OrderSysId;
//   3.2如果交易所返回报单失败（比如超出涨跌停价）, 也会再调用一次OnRtnOrder, 此时没有OrderSysId, 接着还会再调用一次OnErrRtnOrderInsert
//  RequestID的值是0
    void CTPTradeSpi::OnRtnOrder(CThostFtdcOrderField* pOrder) {
        if (pOrder) {
            LOG_INFO << "OnRtnOrder, InstrumentID: " << pOrder->InstrumentID
                << ", ExchangeID: " << pOrder->ExchangeID
                << ", FrontID: " << pOrder->FrontID
                << ", SessionID: " << pOrder->SessionID
                << ", OrderRef: " << pOrder->OrderRef
                << ", Direction: " << pOrder->Direction
                << ", CombOffsetFlag: " << pOrder->CombOffsetFlag
                << ", RequestID: " << pOrder->RequestID
                << ", OrderSysID: " << pOrder->OrderSysID
                << ", OrderStatus: " << pOrder->OrderStatus
                << ", OrderSubmitStatus: " << pOrder->OrderSubmitStatus
                << ", TradingDay: " << pOrder->TradingDay
                << ", GTDDate: " << pOrder->GTDDate
                << ", InsertDate: " << pOrder->InsertDate
                << ", InsertTime: " << pOrder->InsertTime
                << ", CancelTime: " << pOrder->CancelTime
                << ", ActiveTime: " << pOrder->ActiveTime
                << ", SuspendTime: " << pOrder->SuspendTime
                << ", UpdateTime: " << pOrder->UpdateTime
                << ", LimitPrice: " << pOrder->LimitPrice
                << ", VolumeTraded: " << pOrder->VolumeTraded
                << ", VolumeTotal: " << pOrder->VolumeTotal;
        }

        try {
            // 委托合同号: <前置编号>_<会话编号>_<报单引用>_<代码>//
            string order_sys_id = x::Trim(pOrder->OrderSysID);
            int64_t order_ref = atoi(pOrder->OrderRef);
            string ctp_code = pOrder->InstrumentID;
            stringstream ss;
            ss << pOrder->FrontID << "_" << pOrder->SessionID << "_" << order_ref << "_" << ctp_code;
            string order_no = ss.str();
            if (!order_sys_id.empty()) {
                order_nos_[order_sys_id] = order_no;
            }
            int64_t order_state = ctp_order_state2std(pOrder->OrderStatus, pOrder->OrderSubmitStatus);
            LOG_INFO << "order_no: " << order_no << ", order_state: " << order_state;
            if (order_state == kOrderPartlyCanceled || order_state == kOrderFullyCanceled) {
                string req_message = "";
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    auto itor = withdraw_msg_.find(order_no);
                    if (itor != withdraw_msg_.end()) {
                        req_message = itor->second;
                        withdraw_msg_.erase(itor);
                    }
                }
                if (req_message.length()) {
                    MemTradeWithdrawMessage *req = (MemTradeWithdrawMessage *) (req_message.data());
                    MemTradeOrderMessage rep {};
                    memcpy(&rep, req, sizeof(MemTradeWithdrawMessage));
                    rep.rep_time = x::RawDateTime();
                    broker_->SendRtnMessage(string(reinterpret_cast<const char*>(&rep), sizeof(MemTradeWithdrawMessage)), kMemTypeTradeWithdrawRep);
                }
            } else {
                int _RequestID = atol(x::Trim(pOrder->OrderRef).c_str());
                string req_message = "";
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    auto itor = req_msg_.find(_RequestID);
                    if (itor != req_msg_.end()) {
                        req_message = itor->second;
                        req_msg_.erase(itor);
                    }
                }

                if (req_message.length()) {
                    MemTradeOrderMessage* req = (MemTradeOrderMessage*)(req_message.data());
                    int length = sizeof(MemTradeOrderMessage) + sizeof(MemTradeOrder) * req->items_size;
                    char buffer[length] = "";
                    memcpy(buffer, req, length);
                    MemTradeOrderMessage* rep = (MemTradeOrderMessage*)buffer;
                    auto order = (MemTradeOrder*)((char*)rep + sizeof(MemTradeOrderMessage));
                    strcpy(order->order_no, order_no.c_str());
                    rep->rep_time = x::RawDateTime();
                    broker_->SendRtnMessage(string(buffer, length), kMemTypeTradeOrderRep);
                }
            }

            // -------------------------------------------------------------------------
            int64_t market = ctp_market2std(pOrder->ExchangeID);
            if (market == co::kMarketCZCE) {
                InsertCzceCode(ctp_code);
            }
            string suffix = MarketToSuffix(market).data();
            string code = ctp_code + suffix;
            int64_t withdraw_volume = 0;
            {
                co::fbs::TradeOrderT _order;
                _order.market = market;
                _order.code = code;
                _order.order_no = order_no;
                _order.bs_flag = ctp_bs_flag2std(pOrder->Direction);
                _order.oc_flag = ctp_oc_flag2std(pOrder->CombOffsetFlag[0]);
                _order.volume = pOrder->VolumeTotalOriginal;
                _order.price = pOrder->LimitPrice;
                _order.match_volume = pOrder->VolumeTraded;
                if (order_state == kOrderPartlyCanceled || order_state == kOrderFullyCanceled) {
                    withdraw_volume = pOrder->VolumeTotalOriginal - pOrder->VolumeTraded;
                    if (withdraw_volume > 0) {
                        _order.withdraw_volume = withdraw_volume;
                    } else {
                        _order.withdraw_volume = pOrder->VolumeTotal;
                    }
                } else if (order_state == kOrderFailed) {
                    _order.withdraw_volume = pOrder->VolumeTotal;
                }
                future_position_master_.Update(_order);
            }

            // -----------------------------------------------------
            if (order_state == kOrderPartlyCanceled || order_state == kOrderFullyCanceled || order_state == kOrderFailed) {
                MemTradeKnock _knock {};
                if (order_state == kOrderFailed) {
                    _knock.timestamp = x::RawDateTime();
                } else {
                    if (strcmp(pOrder->CancelTime, "06:00:00") > 0 && strcmp(pOrder->CancelTime, "18:00:00") <= 0) {
                        _knock.timestamp = CtpTimestamp(date_, pOrder->CancelTime);
                    } else if (strcmp(pOrder->CancelTime, "18:00:00") > 0 && strcmp(pOrder->CancelTime, "23:59:59") <= 0) {
                        _knock.timestamp = CtpTimestamp(pre_trading_day_, pOrder->CancelTime);
                    } else {
                        _knock.timestamp = CtpTimestamp(pre_trading_day_next_, pOrder->CancelTime);
                    }
                }
                strcpy(_knock.fund_id, investor_id_.c_str());
                string match_no = "_" + order_no;
                strcpy(_knock.order_no, order_no.c_str());
                strcpy(_knock.match_no, match_no.c_str());
                strcpy(_knock.code, code.c_str());
                _knock.market = market;

                auto it = all_instruments_.find(code);
                if (it != all_instruments_.end()) {
                    string name = it->second.first;
                    strcpy(_knock.name, name.c_str());
                }
                _knock.bs_flag = ctp_bs_flag2std(pOrder->Direction);
                _knock.oc_flag = ctp_oc_flag2std(pOrder->CombOffsetFlag[0]);
                if (order_state == kOrderPartlyCanceled || order_state == kOrderFullyCanceled) {
                    _knock.match_type = kMatchTypeWithdrawOK;
                    _knock.match_volume = withdraw_volume;
                } else {
                    _knock.match_type = kMatchTypeFailed;
                    _knock.match_volume = pOrder->VolumeTotalOriginal;
                    string error = x::GBKToUTF8(x::Trim(pOrder->StatusMsg));
                    strcpy(_knock.error, error.c_str());
                }
                _knock.match_price = 0;
                _knock.match_amount = 0;
                broker_->SendRtnMessage(string(reinterpret_cast<const char*>(&_knock), sizeof(MemTradeKnock)), kMemTypeTradeKnock);
            }
        } catch (std::exception& e) {
            LOG_ERROR << "OnRtnOrder: " << e.what();
        }
    }

    /// 成交通知(测试发现, 委托状态更新比成交数据更快)
    void CTPTradeSpi::OnRtnTrade(CThostFtdcTradeField* pTrade) {
        if (!query_instruments_finish_.load()) {
            all_ftdc_trades_.push_back(*pTrade);
            LOG_INFO << "query instrument not finish";
            return;
        }
        if (pTrade) {
            LOG_INFO << "OnRtnTrade, InstrumentID: " << pTrade->InstrumentID
                << ", OrderRef: " << pTrade->OrderRef
                << ", Direction: " << pTrade->Direction
                << ", CombOffsetFlag: " << pTrade->OffsetFlag
                << ", TradeID: " << pTrade->TradeID
                << ", OrderSysID: " << pTrade->OrderSysID
                << ", Price: " << pTrade->Price
                << ", Volume: " << pTrade->Volume
                << ", TradeTime: " << pTrade->TradeTime
                << ", TradeDate: " << pTrade->TradeDate
                << ", TradingDay: " << pTrade->TradingDay;
        }

        try {
            // CTP推过来的成交数据中没有FrontId和SessionId, 无法生成委托合同号, 需要根据OrderSysId从映射表中查找//
            string order_sys_id = x::Trim(pTrade->OrderSysID);
            string match_no = x::Trim(pTrade->TradeID);
            map<string, string>::iterator itr_order_no = order_nos_.find(order_sys_id);
            if (itr_order_no != order_nos_.end()) {
                string order_no = itr_order_no->second;
                string ctp_code = pTrade->InstrumentID;
                int64_t market = ctp_market2std(pTrade->ExchangeID);
                if (market == co::kMarketCZCE) {
                    InsertCzceCode(ctp_code);
                }
                string suffix =MarketToSuffix(market).data();
                string code = ctp_code + suffix;
                string match_no = x::Trim(pTrade->TradingDay) + "_" + x::Trim(pTrade->TradeID);
                double match_amount = pTrade->Price * pTrade->Volume;
                string name;
                auto it = all_instruments_.find(code);
                if (it != all_instruments_.end()) {
                    name = it->second.first;
                    match_amount = match_amount * it->second.second;
                }

                MemTradeKnock _knock {};
                if (strcmp(pTrade->TradeTime, "06:00:00") > 0 && strcmp(pTrade->TradeTime, "18:00:00") <= 0) {
                    _knock.timestamp = CtpTimestamp(date_, pTrade->TradeTime);
                } else if (strcmp(pTrade->TradeTime, "18:00:00") > 0 && strcmp(pTrade->TradeTime, "23:59:59") <= 0) {
                    _knock.timestamp = CtpTimestamp(pre_trading_day_, pTrade->TradeTime);
                } else {
                    // 00:00:01 �� 06:00:00
                    _knock.timestamp = CtpTimestamp(pre_trading_day_next_, pTrade->TradeTime);
                }

                strcpy(_knock.fund_id, investor_id_.c_str());
                strcpy(_knock.code, code.c_str());
                strcpy(_knock.name, name.c_str());
                strcpy(_knock.order_no, order_no.c_str());
                strcpy(_knock.match_no, match_no.c_str());
                _knock.bs_flag = ctp_bs_flag2std(pTrade->Direction);
                _knock.oc_flag = ctp_oc_flag2std(pTrade->OffsetFlag);
                _knock.match_type = kMatchTypeOK;
                _knock.match_volume = pTrade->Volume;
                _knock.match_price = pTrade->Price;
                _knock.match_amount = match_amount;
                broker_->SendRtnMessage(string((char*)(&_knock), sizeof(MemTradeKnock)), kMemTypeTradeKnock);
            } else {
                LOG_WARN << "no order_no found of knock: order_sys_id = " << order_sys_id << ", match_no = " << match_no;
            }
        } catch (std::exception& e) {
            LOG_ERROR << "recv knock failed: " << e.what();
        }
    }

    /// 错误应答
    void CTPTradeSpi::OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
        LOG_ERROR << "OnRspError: ret=" << pRspInfo->ErrorID << ", msg=" << CtpToUTF8(pRspInfo->ErrorMsg);
        if (pRspInfo->ErrorID == 90) {
            x::Sleep(CTP_FLOW_CONTROL_MS);
        }
        if (state_ == kStartupStepInit) {
            Start();
        } else if (state_ == kStartupStepLoginOver) {
            ReqSettlementInfoConfirm();
        } else if (state_ == kStartupStepConfirmSettlementOver) {
            ReqQryInstrument();
        } else if (state_ == kStartupStepGetContractsOver) {
            ReqQryInvestorPosition();
        } else if (state_ == kStartupStepGetInitPositionsOver) {  // 已启动完成之后，返回异步响应

        }
    }

    void CTPTradeSpi::Start() {
        state_ = kStartupStepInit;
        string ctp_app_id = Config::Instance()->ctp_app_id();
        if (!ctp_app_id.empty()) {
            ReqAuthenticate();
        } else {
            ReqUserLogin();
        }
    }

    void CTPTradeSpi::Wait() {
        while (state_ < kStartupStepGetInitPositionsOver) {
            x::Sleep(10);
        }
    }

    void CTPTradeSpi::PrepareQuery() {
        int64_t elapsed_ms = x::Timestamp() - pre_query_timestamp_;
        int64_t Sleep_ms = CTP_FLOW_CONTROL_MS - elapsed_ms;
        if (Sleep_ms > 0) {
            LOG_INFO << elapsed_ms << "ms elapsed after pre query, Sleep " << Sleep_ms << "ms for flow control ...";
            x::Sleep(Sleep_ms);
        }
        pre_query_timestamp_ = x::Timestamp();
    }

    string CTPTradeSpi::GetContractName(const string code) {
        string name = "";
        auto it = all_instruments_.find(code);
        if (it != all_instruments_.end()) {
            name = it->second.first;
        }
        return name;
    }

    int CTPTradeSpi::GetRequestID() {
        return ++start_index_;
    }

}  // namespace co
