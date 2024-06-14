#include <string>

#include "ctp_support.h"
#include "boost/algorithm/string.hpp"

namespace co {

    bool is_flow_control(int ret_code) {
        return ((ret_code == -2) || (ret_code == -3));
    }

    string CtpToUTF8(const char* str) {
        string text;
        if (str) {
            try {
                std::string s = str;
                text = x::GBKToUTF8(s);
            } catch (std::exception & e) {
                text = str;
            }
        }
        return text;
    }

    string CtpApiError(int rc) {
        stringstream ss;
        string ret;
        switch (rc) {
        case 0:
            ret = "0-ok";
            break;
        case -1:
            ret = "-1-network error";
            break;
        case -2:
        case -3:
            ss << rc << "-flow control error";
            ret = ss.str();
            break;
        default:
            ss << rc << "-unknown error";
            ret = ss.str();
            break;
        }
        return ret;
    }

    string CtpError(int code, const char* msg) {
        stringstream ss;
        ss << code << "-" << CtpToUTF8(msg);
        return ss.str();
    }

    string CtpError(TThostFtdcErrorIDType code, TThostFtdcErrorMsgType msg) {
        stringstream ss;
        ss << code << "-" << CtpToUTF8(msg);
        return ss.str();
    }

    int64_t CtpTimestamp(int64_t date, TThostFtdcTimeType time) {
        string s_time = time;  // 15:00:01
        boost::algorithm::replace_all(s_time, ":", "");
        int64_t i_time = atoll(s_time.c_str()) * 1000;
        int64_t Timestamp = date * 1000000000LL + i_time % 1000000000LL;
        if (date < 19700101 || date > 30000101 || i_time < 0 || i_time > 235959999) {
            LOG_ERROR << "illegal ctp Timestamp: date = " << date << ", time = " << time;
        }
        return Timestamp;
    }

    int64_t ctp_time2std(TThostFtdcTimeType v) {
        string s = v;  // 15:00:01
        boost::algorithm::replace_all(s, ":", "");
        int64_t stamp = atoi(s.c_str()) * 1000;
        return stamp;
    }

    int64_t ctp_market2std(TThostFtdcExchangeIDType v) {
        int64_t std_market = 0;
        string s = "." + string(v);
        if (s == co::kSuffixCFFEX) {
            std_market = co::kMarketCFFEX;
        } else if (s == co::kSuffixSHFE) {
            std_market = co::kMarketSHFE;
        } else if (s == co::kSuffixDCE) {
            std_market = co::kMarketDCE;
        } else if (s == co::kSuffixCZCE) {
            std_market = co::kMarketCZCE;
        } else if (s == co::kSuffixINE) {
            std_market = co::kMarketINE;
        } else if (s == ".GFEX") {
            std_market = co::kMarketGFE;
        } else {
            LOG_ERROR << "unknown ctp_market: " << v;
        }
        return std_market;
    }

    string market2ctp(int64_t v) {
        string ret;
        switch (v) {
        case co::kMarketCFFEX:
            ret = co::kSuffixCFFEX.substr(1);
            break;
        case co::kMarketSHFE:
            ret = co::kSuffixSHFE.substr(1);
            break;
        case co::kMarketDCE:
            ret = co::kSuffixDCE.substr(1);
            break;
        case co::kMarketCZCE:
            ret = co::kSuffixCZCE.substr(1);
            break;
        case co::kMarketINE:
            ret = co::kSuffixINE.substr(1);
            break;
        case co::kMarketGFE:
            ret = co::kSuffixGFE.substr(1);
            break;
        default:
            LOG_ERROR << "unknown market: " << v;
            break;
        }
        return ret;
    }

    int64_t ctp_bs_flag2std(TThostFtdcDirectionType v) {
        int64_t std_bs_flag = 0;
        switch (v) {
        case THOST_FTDC_D_Buy:
            std_bs_flag = kBsFlagBuy;
            break;
        case THOST_FTDC_D_Sell:
            std_bs_flag = kBsFlagSell;
            break;
        default:
            LOG_ERROR << "unknown ctp_bs_flag: " << v;
            break;
        }
        return std_bs_flag;
    }

    TThostFtdcDirectionType bs_flag2ctp(int64_t v) {
        TThostFtdcDirectionType ctp_bs_flag = '\0';
        switch (v) {
        case kBsFlagBuy:
            ctp_bs_flag = THOST_FTDC_D_Buy;
            break;
        case kBsFlagSell:
            ctp_bs_flag = THOST_FTDC_D_Sell;
            break;
        default:
            LOG_ERROR << "unknown bs_flag: " << v;
            break;
        }
        return ctp_bs_flag;
    }

    int64_t ctp_ls_flag2std(TThostFtdcPosiDirectionType v) {
        int64_t std_bs_flag = 0;
        switch (v) {
            // case THOST_FTDC_PD_Net:  // ��
            //    break;
        case THOST_FTDC_PD_Long:  // ��ͷ
            std_bs_flag = kBsFlagBuy;
            break;
        case THOST_FTDC_PD_Short:  // ��ͷ
            std_bs_flag = kBsFlagSell;
            break;
        default:
            LOG_ERROR << "unknown ctp_ls_flag: " << v;
            break;
        }
        return std_bs_flag;
    }

    int64_t ctp_hedge_flag2std(TThostFtdcHedgeFlagType v) {
        int64_t std_hedge_flag = 0;
        switch (v) {
        case THOST_FTDC_CIDT_Speculation:
            std_hedge_flag = kHedgeFlagSpeculate;
            break;
        case THOST_FTDC_CIDT_Arbitrage:
            std_hedge_flag = kHedgeFlagArbitrage;
            break;
        case THOST_FTDC_CIDT_Hedge:
            std_hedge_flag = kHedgeFlagHedge;
            break;
        default:
            LOG_ERROR << "unknown ctp_hedge_flag: " << v;
            break;
        }
        return std_hedge_flag;
    }

    TThostFtdcHedgeFlagType hedge_flag2ctp(int64_t v) {
        TThostFtdcHedgeFlagType ctp_hedge_flag = 0;
        switch (v) {
        case kHedgeFlagSpeculate:
            ctp_hedge_flag = THOST_FTDC_CIDT_Speculation;
            break;
        case kHedgeFlagArbitrage:
            ctp_hedge_flag = THOST_FTDC_CIDT_Arbitrage;
            break;
        case kHedgeFlagHedge:
            ctp_hedge_flag = THOST_FTDC_CIDT_Hedge;
            break;
        default:
            LOG_ERROR << "unknown hedge_flag: " << v;
            break;
        }
        return ctp_hedge_flag;
    }

    int64_t ctp_oc_flag2std(TThostFtdcOffsetFlagType v) {
        int32_t std_oc_flag = 0;
        switch (v) {
        case THOST_FTDC_OF_Open:  /// ����
            std_oc_flag = kOcFlagOpen;
            break;
        case THOST_FTDC_OF_Close:  /// ƽ��
            std_oc_flag = kOcFlagClose;
            break;
        case THOST_FTDC_OF_ForceClose:  /// ǿƽ
            std_oc_flag = kOcFlagForceClose;
            break;
        case THOST_FTDC_OF_CloseToday:  /// ƽ��
            std_oc_flag = kOcFlagCloseToday;
            break;
        case THOST_FTDC_OF_CloseYesterday:  /// ƽ��
            std_oc_flag = kOcFlagCloseYesterday;
            break;
        case THOST_FTDC_OF_ForceOff:  /// ǿ��
            std_oc_flag = kOcFlagForceOff;
            break;
        case THOST_FTDC_OF_LocalForceClose:  /// ����ǿƽ
            std_oc_flag = kOcFlagLocalForceClose;
            break;
        default:
            LOG_ERROR << "unknown ctp_oc_flag: " << v;
            break;
        }
        return std_oc_flag;
    }

    TThostFtdcOffsetFlagType oc_flag2ctp(int64_t v) {
        TThostFtdcOffsetFlagType ctp_oc_flag = '\0';
        switch (v) {
        case kOcFlagOpen:  /// ����
            ctp_oc_flag = THOST_FTDC_OF_Open;
            break;
        case kOcFlagClose:  /// ƽ��
            ctp_oc_flag = THOST_FTDC_OF_Close;
            break;
        case kOcFlagForceClose:  /// ǿƽ
            ctp_oc_flag = THOST_FTDC_OF_ForceClose;
            break;
        case kOcFlagCloseToday :  /// ƽ��
            ctp_oc_flag = THOST_FTDC_OF_CloseToday;
            break;
        case kOcFlagCloseYesterday:  /// ƽ��
            ctp_oc_flag = THOST_FTDC_OF_CloseYesterday;
            break;
        case kOcFlagForceOff:  /// ǿ��
            ctp_oc_flag = THOST_FTDC_OF_ForceOff;
            break;
        case kOcFlagLocalForceClose:  /// ����ǿƽ
            ctp_oc_flag = THOST_FTDC_OF_LocalForceClose;
            break;
        default:
            LOG_ERROR << "unknown oc_flag: " << v;
            break;
        }
        return ctp_oc_flag;
    }

    int64_t ctp_order_state2std(TThostFtdcOrderStatusType order_state, TThostFtdcOrderSubmitStatusType submit_state) {
        // ί��״̬: 0: δ��, 1: ����, (2: �ѱ�), 3: �ѱ�����, 4: ���ɴ���, (5: ����), (6: �ѳ�), (7: ����), (8: �ѳ�), (9: �ϵ�)
        int64_t std_order_state = 0;
        switch (order_state) {
        case THOST_FTDC_OST_AllTraded:  /// ȫ���ɽ�
            std_order_state = kOrderFullyKnocked;
            break;
        case THOST_FTDC_OST_PartTradedQueueing:  /// ���ֳɽ����ڶ�����
            std_order_state = kOrderPartlyKnocked;
            break;
        case THOST_FTDC_OST_PartTradedNotQueueing:  /// ���ֳɽ����ڶ�����, �������ɲ�����
            std_order_state = kOrderPartlyCanceled;
            break;
        case THOST_FTDC_OST_NoTradeQueueing:  /// δ�ɽ����ڶ�����
            std_order_state = kOrderCreated;
            break;
        case THOST_FTDC_OST_NoTradeNotQueueing:  /// δ�ɽ����ڶ�����, �Ƿ��걨
            std_order_state = kOrderFailed;
            break;
        case THOST_FTDC_OST_Canceled:  /// ����
            if (submit_state == THOST_FTDC_OSS_InsertRejected || submit_state == THOST_FTDC_OSS_ModifyRejected) {  // �����Ѿ����ܾ�, �ĵ��Ѿ����ܾ�
                std_order_state = kOrderFailed;
            } else {
                std_order_state = kOrderFullyCanceled;
            }
            break;
        case THOST_FTDC_OST_Unknown:  /// δ֪
            std_order_state = kOrderCreated;
            break;
        case THOST_FTDC_OST_NotTouched:  /// ��δ����
            std_order_state = kOrderCreated;
            break;
        case THOST_FTDC_OST_Touched:  /// �Ѵ���
            std_order_state = kOrderCreated;
            break;
        default:
            LOG_ERROR << "unknown ctp_order_state: " << order_state;
            break;
        }
        return std_order_state;
    }

    TThostFtdcOrderPriceTypeType order_price_type2ctp(int64_t v) {
        TThostFtdcOrderPriceTypeType ret = '\0';
        switch (v) {
        case 0:
            ret = THOST_FTDC_OPT_LimitPrice;  /// Ĭ��: �޼�
            break;
        case 1:
            ret = THOST_FTDC_OPT_AnyPrice;  /// �����
            break;
        case 2:
            ret = THOST_FTDC_OPT_LimitPrice;  /// �޼�
            break;
        case 3:
            ret = THOST_FTDC_OPT_BestPrice;  /// ���ż�
            break;
        case 4:
            ret = THOST_FTDC_OPT_LastPrice;  /// ���¼�
            break;
        case 5:
            ret = THOST_FTDC_OPT_LastPricePlusOneTicks;  /// ���¼۸����ϸ�1��ticks
            break;
        case 6:
            ret = THOST_FTDC_OPT_LastPricePlusTwoTicks;  /// ���¼۸����ϸ�2��ticks
            break;
        case 7:
            ret = THOST_FTDC_OPT_LastPricePlusThreeTicks;  /// ���¼۸����ϸ�3��ticks
            break;
        case 8:
            ret = THOST_FTDC_OPT_AskPrice1;  /// ��һ��
            break;
        case 9:
            ret = THOST_FTDC_OPT_AskPrice1PlusOneTicks;  /// ��һ�۸����ϸ�1��ticks
            break;
        case 10:
            ret = THOST_FTDC_OPT_AskPrice1PlusTwoTicks;  /// ��һ�۸����ϸ�2��ticks
            break;
        case 11:
            ret = THOST_FTDC_OPT_AskPrice1PlusThreeTicks;  /// ��һ�۸����ϸ�3��ticks
            break;
        case 12:
            ret = THOST_FTDC_OPT_BidPrice1;  /// ��һ��
            break;
        case 13:
            ret = THOST_FTDC_OPT_BidPrice1PlusOneTicks;  /// ��һ�۸����ϸ�1��ticks
            break;
        case 14:
            ret = THOST_FTDC_OPT_BidPrice1PlusTwoTicks;  /// ��һ�۸����ϸ�2��ticks
            break;
        case 15:
            ret = THOST_FTDC_OPT_BidPrice1PlusThreeTicks;  /// ��һ�۸����ϸ�3��ticks
            break;
        case 16:
            ret = THOST_FTDC_OPT_FiveLevelPrice;  // �嵵��
            break;
        default:
            LOG_ERROR << "unknown order_price_type: " << v;
            break;
        }
        return ret;
    }

    int64_t ctp_order_price_type2std(TThostFtdcOrderPriceTypeType v) {
        int64_t ret = 0;
        switch (v) {
        case THOST_FTDC_OPT_AnyPrice:
            ret = 1;  /// �����
            break;
        case THOST_FTDC_OPT_LimitPrice:
            ret = 2;  /// �޼�
            break;
        case THOST_FTDC_OPT_BestPrice:
            ret = 3;  /// ���ż�
            break;
        case THOST_FTDC_OPT_LastPrice:
            ret = 4;  /// ���¼�
            break;
        case THOST_FTDC_OPT_LastPricePlusOneTicks:
            ret = 5;  /// ���¼۸����ϸ�1��ticks
            break;
        case THOST_FTDC_OPT_LastPricePlusTwoTicks:
            ret = 6;  /// ���¼۸����ϸ�2��ticks
            break;
        case THOST_FTDC_OPT_LastPricePlusThreeTicks:
            ret = 7;  /// ���¼۸����ϸ�3��ticks
            break;
        case THOST_FTDC_OPT_AskPrice1:
            ret = 8;  /// ��һ��
            break;
        case THOST_FTDC_OPT_AskPrice1PlusOneTicks:
            ret = 9;  /// ��һ�۸����ϸ�1��ticks
            break;
        case THOST_FTDC_OPT_AskPrice1PlusTwoTicks:
            ret = 10;  /// ��һ�۸����ϸ�2��ticks
            break;
        case THOST_FTDC_OPT_AskPrice1PlusThreeTicks:
            ret = 11;  /// ��һ�۸����ϸ�3��ticks
            break;
        case THOST_FTDC_OPT_BidPrice1:
            ret = 12;  /// ��һ��
            break;
        case THOST_FTDC_OPT_BidPrice1PlusOneTicks:
            ret = 13;  /// ��һ�۸����ϸ�1��ticks
            break;
        case THOST_FTDC_OPT_BidPrice1PlusTwoTicks:
            ret = 14;  /// ��һ�۸����ϸ�2��ticks
            break;
        case THOST_FTDC_OPT_BidPrice1PlusThreeTicks:
            ret = 15;  /// ��һ�۸����ϸ�3��ticks
            break;
        case THOST_FTDC_OPT_FiveLevelPrice:
            ret = 16;  // �嵵��
            break;
        default:
            LOG_ERROR << "unknown order_price_type: " << v;
            break;
        }
        return ret;
    }

    TThostFtdcTimeConditionType order_time_condition2ctp(string v) {
        TThostFtdcTimeConditionType ret;
        if (v.empty() || v == "General_Order") {  // ������Ч
            ret = THOST_FTDC_TC_GFD;
        } else if (v == "Automatically_Withdraw") {
            ret = THOST_FTDC_TC_IOC;
        } else {
            LOG_ERROR << "unknown order_time_condition: " << v;
        }
        return ret;
    }

    double ctp_equity(CThostFtdcTradingAccountField *p) {
        /*
        ==========================================
        �ϴν���׼����: ��          1006015.23
        - �ϴ����ö��: ����                0.00
        - �ϴ���Ѻ���: ����                0.00
        + ��Ѻ���: ��������                0.00
        - ���ճ���: ��������                0.00
        + �������: ��������                0.00
        ------------------------------------------
        = ��̬Ȩ��: ��������          1006015.23
        + ƽ��ӯ��: ��������                0.00
        + �ֲ�ӯ��: ��������                0.00
        + Ȩ����: ����������                0.00
        - ������: ����������                0.00
        ------------------------------------------
        = ��̬Ȩ��: ��������          1006015.23��
        - ռ�ñ�֤��: ������                0.00
        - ���ᱣ֤��: ������           432128.00
        - ����������: ������               27.01
        - ���֤��: ������                0.00
        - ����Ȩ����: ������                0.00
        + ���ý��: ��������                0.00
        ------------------------------------------
        = �����ʽ�: ��������           573860.22
        ==========================================

        ==========================================
        �����ʽ�: ��������                0.00
        ��ȡ�ʽ�: ��������           401702.16
        ==========================================
        */
        double static_equity = p->PreBalance
            - p->PreCredit
            - p->PreMortgage
            + p->Mortgage
            - p->Withdraw
            + p->Deposit;
        double equity = static_equity
            + p->CloseProfit
            + p->PositionProfit
            - p->Commission;
        return equity;
    }

    // SR2109 SR109, ֻתSR2109
    void DeleteCzceCode(string& code) {
        int _num = 0;
        int index = -1;
        for (int _i = 0; _i < code.length(); _i++) {
            char temp = code.at(_i);
            if (temp >= '0' && temp <= '9') {
                _num++;
                if (index < 0) {
                    index = _i;
                }
            }
        }
        if (_num == 4) {
            code.erase(index, 1);
        }
    }

    void InsertCzceCode(string& code) {
        int _num = 0;
        int index = -1;
        for (int _i = 0; _i < code.length(); _i++) {
            char temp = code.at(_i);
            if (temp >= '0' && temp <= '9') {
                _num++;
                if (index < 0) {
                    index = _i;
                }
            }
        }
        if (_num == 3) {
            code.insert(index, "2");
        }
    }

    bool IsMonday(int64_t date) {
        int years = date / 10000;
        int left = date % 10000;
        int months = left / 100;
        int days = left % 100;
        int week_day = -1;
        if (1 == months || 2 == months) {
            months += 12;
            years--;
        }
        week_day = (days + 1 + 2 * months + 3 * (months + 1) / 5 + years + years / 4 - years / 100 + years / 400) % 7;
        if (week_day == 1) {
            return true;
        } else {
            return false;
        }
    }

}  // namespace co
