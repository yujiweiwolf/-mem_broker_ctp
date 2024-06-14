// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <x/x.h>
#include <coral/coral.h>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"
#include "ThostFtdcUserApiStruct.h"
#include "ThostFtdcUserApiDataType.h"


#ifdef _WIN32
#pragma comment(lib, "thostmduserapi_se.lib")
#pragma comment(lib, "thosttraderapi_se.lib")
#endif

namespace co {
    // CTP�Բ�ѯ������������������, ÿ��������һ�β�ѯ����
    constexpr int64_t CTP_FLOW_CONTROL_MS = 1000;

    string CtpApiError(int rc);
    string CtpToUTF8(const char* str);
    string CtpError(int code, const char* msg);
    string CtpError(TThostFtdcErrorIDType code, TThostFtdcErrorMsgType msg);
    int64_t CtpTimestamp(int64_t date, TThostFtdcTimeType time);

    bool is_flow_control(int ret_code);
    int64_t ctp_time2std(TThostFtdcTimeType v);
    int64_t ctp_market2std(TThostFtdcExchangeIDType v);
    string market2ctp(int64_t v);
    int64_t ctp_bs_flag2std(TThostFtdcDirectionType v);
    TThostFtdcDirectionType bs_flag2ctp(int64_t v);
    int64_t ctp_ls_flag2std(TThostFtdcPosiDirectionType v);
    int64_t ctp_hedge_flag2std(TThostFtdcHedgeFlagType v);
    TThostFtdcHedgeFlagType hedge_flag2ctp(int64_t v);
    int64_t ctp_oc_flag2std(TThostFtdcOffsetFlagType v);
    TThostFtdcOffsetFlagType oc_flag2ctp(int64_t v);
    int64_t ctp_order_state2std(TThostFtdcOrderStatusType order_state, TThostFtdcOrderSubmitStatusType submit_state);
    TThostFtdcOrderPriceTypeType order_price_type2ctp(int64_t v);
    int64_t ctp_order_price_type2std(TThostFtdcOrderPriceTypeType v);
    TThostFtdcTimeConditionType order_time_condition2ctp(string v);

    double ctp_equity(CThostFtdcTradingAccountField* p);
    void DeleteCzceCode(string& code);
    void InsertCzceCode(string& code);
    bool IsMonday(int64_t date);
}  // namespace co
