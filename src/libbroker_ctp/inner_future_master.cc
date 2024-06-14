// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "inner_future_master.h"
constexpr int kCFFEXOptionLength = 14;

namespace co {

    void InnerFutureMaster::Init(const vector<MemTradePosition>& positions) {
        // 初始化持仓，这里的初始持仓数据应该是今天开盘前的数据，而不是当前状态的持仓。开盘前，只有昨持仓，今日开仓数应该为0
        LOG_INFO << "init inner future position ...";
        state_ = 1;
        for (auto m : positions) {
            InnerFuturePositionPtr buy_pos = GetPosition(m.code, kHedgeFlagSpeculate, kBsFlagBuy);
            buy_pos->set_yd_volume(m.long_pre_volume);
            InnerFuturePositionPtr sell_pos = GetPosition(m.code, kHedgeFlagSpeculate, kBsFlagSell);
            sell_pos->set_yd_volume(m.short_pre_volume);
        }
        for (auto order : init_orders_) {
            Update(*order);
        }
        LOG_INFO << "init inner future position ok: positions = " << positions.size() << ", orders = " << init_orders_.size();
        init_orders_.clear();
        state_ = 2;
    }

    void InnerFutureMaster::Update(const co::fbs::TradeOrderT& order) {
        // 更新内部持仓，理论上如果CTP推送过来的委托状态不发生数据丢失和数据顺序错乱的情况，内部持仓就是准确的。
        if (state_ == 0) {  // 未开始初始化，先缓存起来等待处理
            std::shared_ptr<co::fbs::TradeOrderT> m = make_shared<co::fbs::TradeOrderT>(order);
            init_orders_.push_back(m);
            return;
        }
        string code = order.code;
        int64_t market = order.market;
        string order_no = order.order_no;
        int64_t hedge_flag = kHedgeFlagSpeculate;
        int64_t bs_flag = order.bs_flag;
        int64_t oc_flag = order.oc_flag;
        // -------------------------------------------------------------------
        // 计算本次应冻结和解冻的数量
        InnerFutureOrderPtr iorder;
        map<string, InnerFutureOrderPtr>::iterator itr_order = orders_.find(order_no);
        if (itr_order != orders_.end()) {
            iorder = itr_order->second;
        } else {
            iorder = std::make_shared<InnerFutureOrder>();
            orders_[order_no] = iorder;
        }
        int64_t new_order_volume = order.volume - iorder->order_volume();  // 本次新增委托数，等于本次新增委托数量
        int64_t new_match_volume = order.match_volume - iorder->match_volume();  // 本次新增成交数量
        int64_t new_withdraw_volume = order.withdraw_volume - iorder->withdraw_volume();  // 本次新增撤单数量（废单认为是全部撤单）
        // 当前断线重连后，可能会收到之前已经推过来的委托，出现负数的情况，这里强制置为0，避免出现内部持仓的回滚。
        if (new_order_volume < 0) {
            LOG_WARN << "update future inner position failed: new_order_volume = " << new_order_volume << ", InnerFutureOrder, order_no: " << order_no;
            new_order_volume = 0;
        }
        if (new_match_volume < 0) {
            LOG_WARN << "update future inner position failed: new_match_volume = " << new_match_volume << ", InnerFutureOrder, order_no: " << order_no;
            new_match_volume = 0;
        }
        if (new_withdraw_volume < 0) {
            LOG_WARN << "update future inner position failed: new_withdraw_volume = " << new_withdraw_volume << ", InnerFutureOrder, order_no: " << order_no;
            new_withdraw_volume = 0;
        }
        if (new_order_volume <= 0 && new_match_volume <= 0 && new_withdraw_volume <= 0) {
            LOG_INFO << "not deal [InnerFutureOrder], order_no: " << order_no
                   << ", bs_flag : " << bs_flag
                   << ", oc_flag : " << oc_flag
                   << ", order_volume: " << iorder->order_volume()
                   << ", match_volume: " << iorder->match_volume()
                   << ", withdraw_volume: " << iorder->withdraw_volume();
            return;
        }
        iorder->set_order_volume(iorder->order_volume() + new_order_volume);
        iorder->set_match_volume(iorder->match_volume() + new_match_volume);
        iorder->set_withdraw_volume(iorder->withdraw_volume() + new_withdraw_volume);
        // -------------------------------------------------------------------
        int64_t _oc_flag = 0;
        switch (order.oc_flag) {
        case kOcFlagOpen:  // 开仓
            _oc_flag = oc_flag;
            break;
        case kOcFlagClose:  // 平仓
        case kOcFlagForceClose:  // 强平
        case kOcFlagForceOff:  // 强减
        case kOcFlagLocalForceClose:  // 本地强平
            _oc_flag = market == co::kMarketSHFE ? kOcFlagCloseYesterday : oc_flag;
            break;
        case kOcFlagCloseToday:  // 平今（只有上期所才有平今和平昨的概念）
            _oc_flag = kOcFlagCloseToday;
            break;
        case kOcFlagCloseYesterday:  // 平昨（只有上期所才有平今和平昨的概念）
            _oc_flag = kOcFlagCloseYesterday;
            break;
        default:
            LOG_WARN << "unknown oc_flag for updating future inner position: "; //  << order.Utf8DebugString()
            return;
        }
        LOG_INFO << "[InnerFutureOrder], code: " << code
            << ", order_no : " << order_no
            << ", bs_flag : " << bs_flag
            << ", oc_flag : " << _oc_flag
            << ", order_volume: " << iorder->order_volume()
            << ", match_volume: " << iorder->match_volume()
            << ", withdraw_volume: " << iorder->withdraw_volume();
        // 获取待更新的持仓
        InnerFuturePositionPtr pos;
        if ((bs_flag == kBsFlagBuy && _oc_flag == kOcFlagOpen) ||
            (bs_flag == kBsFlagSell && (_oc_flag == kOcFlagClose || _oc_flag == kOcFlagCloseToday || _oc_flag == kOcFlagCloseYesterday))) {
            // 买开和卖平（更新买持仓）
            pos = GetPosition(code, hedge_flag, kBsFlagBuy);
        } else if ((bs_flag == kBsFlagSell && _oc_flag == kOcFlagOpen) ||
            (bs_flag == kBsFlagBuy && (_oc_flag == kOcFlagClose || _oc_flag == kOcFlagCloseToday || _oc_flag == kOcFlagCloseYesterday))) {
            // 卖开和买平（更新卖持仓）
            pos = GetPosition(code, hedge_flag, kBsFlagSell);
        } else {
            LOG_WARN << "illegal bs_flag or oc_flag for updating future inner position: "; //  << order.Utf8DebugString()
            return;
        }

        if (pos == nullptr) {
            LOG_ERROR << "pos is empty.";
        }
        stringstream ss;
        ss << "[InnerFuturePosition] Order{code=" << code
            << ", market=" << market
            << ", bs_flag=" << bs_flag
            << ", oc_flag=" << oc_flag
            << ", order_no=" << order.order_no
            << ", order_volume=+" << new_order_volume << "/" << order.volume
            << ", match_volume=+" << new_match_volume << "/" << order.match_volume
            << ", withdraw_volume=+" << new_withdraw_volume << "/" << order.withdraw_volume
            << "}, " << pos->ToString();
        // 内部持仓更新逻辑
        // 1.买开（更新买持仓）
        // 1.1 买开委托：增加开仓冻结；
        // 1.2 买开成交：减少开仓冻结，增加持仓，增加已开仓数；
        // 1.3 买开撤单：减少开仓冻结；
        // 2.买平（更新卖持仓）
        // 2.1 买平委托：减少持仓，增加平仓冻结；
        // 2.2 买平成交：减少平仓冻结，增加已平仓数；
        // 2.3 买平撤单：减少平仓冻结，增加持仓；
        // 3.卖开（更新卖持仓）
        // 3.1 卖开委托：增加开仓冻结；
        // 3.2 卖开成交：减少开仓冻结，增加持仓、增加已开仓数；
        // 3.3 卖开撤单：减少开仓冻结；
        // 4.卖平（更新买持仓）
        // 4.1 卖平委托：减少持仓，增加平仓冻结；
        // 4.2 卖平成交：减少平仓冻结，增加已平仓数；
        // 4.3 卖平撤单：减少平仓冻结，增加持仓；
        switch (_oc_flag) {
        case kOcFlagOpen:
            if (new_order_volume > 0) { // 开仓委托：增加开仓冻结
                pos->set_td_opening_volume(pos->td_opening_volume() + new_order_volume);
            }
            if (new_match_volume > 0) { // 开仓成交：减少开仓冻结，增加持仓，增加已开仓数
                pos->set_td_opening_volume(pos->td_opening_volume() - new_match_volume);
                pos->set_td_volume(pos->td_volume() + new_match_volume);
                pos->set_td_open_volume(pos->td_open_volume() + new_match_volume);
            }
            if (new_withdraw_volume > 0) { // 开仓撤单：减少开仓冻结
                pos->set_td_opening_volume(pos->td_opening_volume() - new_withdraw_volume);
            }
            break;
        case kOcFlagClose: // ƽ��
            // 先平昨后平今，@TODO 这里没有考虑以下情况：“除上期所外的三家交易在涉及到平今手续费减免时先平今后平昨（后开先平）”
            if (new_order_volume > 0) { // 平仓委托：减少持仓，增加平仓冻结
                if (market == co::kMarketSHFE) { // 上期所，先平昨再平今
                    int64_t yd = pos->yd_volume() >= new_order_volume ? new_order_volume : pos->yd_volume(); // 平昨委托数量
                    if (yd < 0) {
                        yd = 0;
                    }
                    int64_t td = yd < new_order_volume ? new_order_volume - yd : 0; // 平今委托数量
                    if (yd > 0) {
                        pos->set_yd_closing_volume(pos->yd_closing_volume() + yd);
                        pos->set_yd_volume(pos->yd_volume() - yd);
                    }
                    if (td > 0) {
                        int64_t td_closing_volume = pos->td_closing_volume() + td;
                        int64_t td_volume = pos->td_volume() - td;
                        if (td_volume < 0) {
                            LOG_WARN << "update future inner position failed: td_volume = " << td_volume << ", broken_order: "; // << order.Utf8DebugString();
                        }
                        pos->set_td_closing_volume(td_closing_volume);
                        pos->set_td_volume(td_volume);
                    }
                } else { // 其他交易所，先平今再平昨
                    int64_t td = pos->td_volume() >= new_order_volume ? new_order_volume : pos->td_volume(); // 平今委托数量
                    if (td < 0) {
                        td = 0;
                    }
                    int64_t yd = td < new_order_volume ? new_order_volume - td : 0; // 平昨委托数量
                    if (td > 0) {
                        pos->set_td_closing_volume(pos->td_closing_volume() + td);
                        pos->set_td_volume(pos->td_volume() - td);
                    }
                    if (yd > 0) {
                        int64_t yd_closing_volume = pos->yd_closing_volume() + yd;
                        int64_t yd_volume = pos->yd_volume() - yd;
                        if (yd_volume < 0) {
                            LOG_WARN << "update future inner position failed: yd_volume = " << yd_volume << ", broken_order: "; // << order.Utf8DebugString();
                        }
                        pos->set_yd_closing_volume(yd_closing_volume);
                        pos->set_yd_volume(yd_volume);
                    }
                }
            }
            if (new_match_volume > 0) { // 平仓成交：减少平仓冻结，增加已平仓数
                if (market == co::kMarketSHFE || market == co::kMarketINE) { // 上期所，先平昨再平今
                    int64_t yd = pos->yd_closing_volume() >= new_match_volume ? new_match_volume : pos->yd_closing_volume();
                    if (yd < 0) {
                        yd = 0;
                    }
                    int64_t td = yd < new_match_volume ? new_match_volume - yd : 0;
                    if (yd > 0) {
                        int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                        int64_t yd_close_volume = pos->yd_close_volume() + yd;
                        pos->set_yd_closing_volume(yd_closing_volume);
                        pos->set_yd_close_volume(yd_close_volume);
                    }
                    if (td > 0) {
                        int64_t td_closing_volume = pos->td_closing_volume() - td;
                        int64_t td_close_volume = pos->td_close_volume() + td;
                        if (td_closing_volume < 0) {
                            LOG_WARN << "update future inner position failed: td_closing_volume = " << td_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                        }
                        pos->set_td_closing_volume(td_closing_volume);
                        pos->set_td_close_volume(td_close_volume);
                    }
                } else { // 其他交易所，先平今再平昨
                    int64_t td = pos->td_closing_volume() >= new_match_volume ? new_match_volume : pos->td_closing_volume();
                    if (td < 0) {
                        td = 0;
                    }
                    int64_t yd = td < new_match_volume ? new_match_volume - td : 0;
                    if (td > 0) {
                        int64_t td_closing_volume = pos->td_closing_volume() - td;
                        int64_t td_close_volume = pos->td_close_volume() + td;
                        pos->set_td_closing_volume(td_closing_volume);
                        pos->set_td_close_volume(td_close_volume);
                    }
                    if (yd > 0) {
                        int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                        int64_t yd_close_volume = pos->yd_close_volume() + yd;
                        if (yd_closing_volume < 0) {
                            LOG_WARN << "update future inner position failed: yd_closing_volume = " << yd_closing_volume << ", broken_order: "; //  << order.Utf8DebugString();
                        }
                        pos->set_yd_closing_volume(yd_closing_volume);
                        pos->set_yd_close_volume(yd_close_volume);
                    }
                }
            }
            if (new_withdraw_volume > 0) { // 平仓撤单：减少平仓冻结，增加持仓
                if (market == co::kMarketSHFE || market == co::kMarketINE) { // 上期所，先撤今再撤昨
                    int64_t td = pos->td_closing_volume() >= new_withdraw_volume ? new_withdraw_volume : pos->td_closing_volume();
                    if (td < 0) {
                        td = 0;
                    }
                    int64_t yd = td < new_withdraw_volume ? new_withdraw_volume - td : 0;
                    if (td > 0) {
                        int64_t td_closing_volume = pos->td_closing_volume() - td;
                        int64_t td_volume = pos->td_volume() + td;
                        if (td_closing_volume < 0) {
                            LOG_WARN << "update future inner position failed: td_closing_volume = " << td_closing_volume << ", broken_order: "; //  << order.Utf8DebugString();
                        }
                        pos->set_td_closing_volume(td_closing_volume);
                        pos->set_td_volume(td_volume);
                    }
                    if (yd > 0) {
                        int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                        int64_t yd_volume = pos->yd_volume() + yd;
                        if (yd_closing_volume < 0) {
                            LOG_WARN << "update future inner position failed: yd_closing_volume = " << yd_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                        }
                        pos->set_yd_closing_volume(yd_closing_volume);
                        pos->set_yd_volume(yd_volume);
                    }
                } else { // 其他交易所，先撤昨再撤今
                    int64_t yd = pos->yd_closing_volume() >= new_withdraw_volume ? new_withdraw_volume : pos->yd_closing_volume();
                    if (yd < 0) {
                        yd = 0;
                    }
                    int64_t td = yd < new_withdraw_volume ? new_withdraw_volume - yd : 0;
                    if (yd > 0) {
                        int64_t yd_closing_volume = pos->yd_closing_volume() - yd;
                        int64_t yd_volume = pos->yd_volume() + yd;
                        pos->set_yd_closing_volume(yd_closing_volume);
                        pos->set_yd_volume(yd_volume);
                    }
                    if (td > 0) {
                        int64_t td_closing_volume = pos->td_closing_volume() - td;
                        int64_t td_volume = pos->td_volume() + td;
                        if (td_closing_volume < 0) {
                            LOG_WARN << "update future inner position failed: td_closing_volume = " << td_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                        }
                        pos->set_td_closing_volume(td_closing_volume);
                        pos->set_td_volume(td_volume);
                    }
                }
            }
            break;
        case kOcFlagCloseToday: // 平今
            if (new_order_volume > 0) { // 平仓委托：减少持仓，增加平仓冻结
                int64_t td_volume = pos->td_volume() - new_order_volume;
                int64_t td_closing_volume = pos->td_closing_volume() + new_order_volume;
                if (td_volume < 0) {
                    LOG_WARN << "update future inner position failed: td_volume = " << td_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_td_closing_volume(td_closing_volume);
                pos->set_td_volume(td_volume);
            }
            if (new_match_volume > 0) { // 平仓成交：减少平仓冻结，增加已平仓数
                int64_t td_closing_volume = pos->td_closing_volume() - new_match_volume;
                int64_t td_close_volume = pos->td_close_volume() + new_match_volume;
                if (td_closing_volume < 0) {
                    LOG_WARN << "update future inner position failed: td_closing_volume = " << td_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_td_closing_volume(td_closing_volume);
                pos->set_td_close_volume(td_close_volume);
            }
            if (new_withdraw_volume > 0) { // 平仓撤单：减少平仓冻结，增加持仓
                int64_t td_closing_volume = pos->td_closing_volume() - new_withdraw_volume;
                int64_t td_volume = pos->td_volume() + new_withdraw_volume;
                if (td_closing_volume < 0) {
                    LOG_WARN << "update future inner position failed: td_closing_volume = " << td_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_td_closing_volume(td_closing_volume);
                pos->set_td_volume(td_volume);
            }
            break;
        case kOcFlagCloseYesterday: // 平昨
            if (new_order_volume > 0) { // 平仓委托：减少持仓，增加平仓冻结
                int64_t yd_volume = pos->yd_volume() - new_order_volume;
                int64_t yd_closing_volume = pos->yd_closing_volume() + new_order_volume;
                if (yd_volume < 0) {
                    LOG_WARN << "update future inner position failed: yd_volume = " << yd_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_yd_closing_volume(yd_closing_volume);
                pos->set_yd_volume(yd_volume);
            }
            if (new_match_volume > 0) { // 平仓成交：减少平仓冻结，增加已平仓数
                int64_t yd_closing_volume = pos->yd_closing_volume() - new_match_volume;
                int64_t yd_close_volume = pos->yd_close_volume() + new_match_volume;
                if (yd_closing_volume < 0) {
                    LOG_WARN << "update future inner position failed: yd_closing_volume = " << yd_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_yd_closing_volume(yd_closing_volume);
                pos->set_yd_close_volume(yd_close_volume);
            }
            if (new_withdraw_volume > 0) { // 平仓撤单：减少平仓冻结，增加持仓
                int64_t yd_closing_volume = pos->yd_closing_volume() - new_withdraw_volume;
                int64_t yd_volume = pos->yd_volume() + new_withdraw_volume;
                if (yd_closing_volume < 0) {
                    LOG_WARN << "update future inner position failed: yd_closing_volume = " << yd_closing_volume << ", broken_order: "; // << order.Utf8DebugString();
                }
                pos->set_yd_closing_volume(yd_closing_volume);
                pos->set_yd_volume(yd_volume);
            }
            break;
        default:
            break;
        }
        ss << " -> " << pos->ToString();
        LOG_INFO << ss.str();
        // ------------------------------------------------
        // 风控策略：更新当前期货类型的已开仓数和开仓冻结数之和
        if (code.length() > kCFFEXOptionLength) {
            return;
        }
        if (risk_max_today_opening_volume_ >= 0 && _oc_flag == kOcFlagOpen && (new_order_volume > 0 || new_withdraw_volume > 0)) {
            string type = code.length() > 2 ? code.substr(0, 2) : "";
            if (type == "IF" || type == "IH" || type == "IC" || type == "IM") {
                int64_t volume = new_order_volume - new_withdraw_volume;
                map<string, int64_t>::iterator itr = open_cache_.find(type);
                if (itr != open_cache_.end()) {
                    itr->second += volume;
                } else {
                    open_cache_[type] = volume;
                }
            }
        }
    }

    int64_t InnerFutureMaster::GetAutoOcFlag(const co::fbs::TradeOrderT& order) {
        // 买开（bs_flag=买，oc_flag=自动）:
        // 1.如果有卖方向头寸，则执行：买平；
        // 2.如果没有卖方向头寸或卖方向头寸不足，则执行：买开
        // 卖开（bs_flag=买，oc_flag=自动）：
        // 1.如果有买方向头寸，则执行：卖平;
        // 2.如果没有买方向头寸或买方向头寸不足，则执行：卖开
        if (order.oc_flag != kOcFlagAuto) { // 不是自动开平仓，直接返回请求中设定的开平仓标记
            CheckRisk(order.code, order.bs_flag, order.oc_flag, order.volume);
            return order.oc_flag;
        }
        int64_t ret_oc_flag = kOcFlagOpen; // 默认开仓
        string code = order.code;
        int64_t _bs_flag = order.bs_flag;
        if (_bs_flag != kBsFlagBuy && _bs_flag != kBsFlagSell) {
            CheckRisk(order.code, order.bs_flag, order.oc_flag, order.volume);
            return ret_oc_flag;
        }
        int64_t r_bs_flag = _bs_flag == kBsFlagBuy ? kBsFlagSell : kBsFlagBuy;
        string key = GetKey(code, kHedgeFlagSpeculate, r_bs_flag);
        map<string, InnerFuturePositionPtr>::iterator itr = positions_.find(key);
        if (itr == positions_.end()) {
            CheckRisk(order.code, order.bs_flag, order.oc_flag, order.volume);
            return ret_oc_flag;
        }
        InnerFuturePositionPtr pos = itr->second;
        LOG_INFO << "GetAutoOcFlag: " << pos->ToString();
        int64_t order_volume = order.volume;
        // 上期所，平仓时需要指定是平今仓还是昨仓；
        // 其他交易所，平仓时不指定是平今仓还是昨仓，交易所自动以“先开先平”的原则进行处理。
        if (order.market == co::kMarketSHFE || order.market == co::kMarketINE) {
            if (pos->yd_volume() >= order_volume) { // 先平昨仓
                ret_oc_flag = kOcFlagCloseYesterday;
            } else if (pos->td_volume() >= order_volume) { // 后平今仓
                ret_oc_flag = kOcFlagCloseToday;
            }
        } else {
            if (pos->yd_volume() >= order_volume) { // 先平昨仓
                ret_oc_flag = kOcFlagClose;
            } else if (pos->td_volume() >= order_volume) { // 后平今仓
                ret_oc_flag = kOcFlagClose;
            }
            // ---------------------------------------------
            // 风控策略：禁止股指期货自动平今仓
            if (order.code.length() <= kCFFEXOptionLength) {
                if (risk_forbid_closing_today_ && ret_oc_flag == kOcFlagClose) {
                    string code = order.code;
                    string type = code.length() > 2 ? code.substr(0, 2) : "";
                    if (type == "IF" || type == "IH" || type == "IC" || type == "IM") {
                        // 如果有今仓，不管有没有昨仓，CTP都会执行平今的操作，这里要修改为开仓
                        if (pos->td_volume() > 0) {
                            ret_oc_flag = kOcFlagOpen; // 修改为开仓
                        }
                    }
                }
            }
            // ---------------------------------------------
        }
        CheckRisk(code, _bs_flag, ret_oc_flag, order_volume);
        return ret_oc_flag;
    }

    int64_t InnerFutureMaster::GetCloseYestodayFlag(const co::fbs::TradeOrderT& order) {
        // 平昨仓,不平今仓, 如果昨仓数量不足,就开仓
        int64_t ret_oc_flag = kOcFlagOpen;// 默认开仓
        string code = order.code;
        int64_t _bs_flag = order.bs_flag;
        int64_t r_bs_flag = _bs_flag == kBsFlagBuy ? kBsFlagSell : kBsFlagBuy;
        string key = GetKey(code, kHedgeFlagSpeculate, r_bs_flag);
        map<string, InnerFuturePositionPtr>::iterator itr = positions_.find(key);
        // 无昨仓
        if (itr == positions_.end()) {
            LOG_INFO << "no yestoday volume, open flag.";
            CheckRisk(order.code, order.bs_flag, order.oc_flag, order.volume);
            return ret_oc_flag;
        }
        InnerFuturePositionPtr pos = itr->second;
        LOG_INFO << "GetCloseYestodayFlag: " << pos->ToString();
        int64_t order_volume = order.volume;
        LOG_INFO << "yd_volume: " << pos->yd_volume() << ", order_volume: " << order_volume
            << ", market: " << order.market;
        if (order.market == co::kMarketSHFE) {
            if (pos->yd_volume() >= order_volume) { // 只平昨仓
                ret_oc_flag = kOcFlagCloseYesterday;
            }
        } else {
            if (pos->yd_volume() >= order_volume) { // 只平昨仓
                ret_oc_flag = kOcFlagClose;
            }
        }
        CheckRisk(code, _bs_flag, ret_oc_flag, order_volume);
        return ret_oc_flag;
    }

    string InnerFutureMaster::GetKey(string code, int64_t hedge_flag, int64_t bs_flag) {
        stringstream ss;
        ss << code << "_" << hedge_flag << "_" << bs_flag;
        return ss.str();
    }

    InnerFuturePositionPtr InnerFutureMaster::GetPosition(string code, int64_t hedge_flag, int64_t bs_flag) {
        InnerFuturePositionPtr pos;
        string key = GetKey(code, hedge_flag, bs_flag);
        map<string, InnerFuturePositionPtr>::iterator itr_pos = positions_.find(key);
        if (itr_pos != positions_.end()) {
            pos = itr_pos->second;
        } else {
            pos = InnerFuturePosition::New(code, hedge_flag, bs_flag);
            positions_[key] = pos;
        }
        return pos;
    }

    void InnerFutureMaster::CheckRisk(string code, int64_t bs_flag, int64_t oc_flag, int64_t order_volume) {
        // --------------------IO2208-C-4250.CFFEX----------------------------
        if (code.length() > kCFFEXOptionLength) {
            return;
        }
        // 风控策略：限制股指期货当日开仓数量
        if (risk_max_today_opening_volume_ >= 0 && oc_flag == kOcFlagOpen) {
            string type = code.length() > 2 ? code.substr(0, 2) : "";
            if (type == "IF" || type == "IH" || type == "IC" || type == "IM") {
                int64_t open_volume = 0; // 当前期货类型的已开仓数和开仓冻结数之和
                map<string, int64_t>::iterator itr = open_cache_.find(type);
                if (itr != open_cache_.end()) {
                    open_volume = itr->second;
                }
                if (order_volume + open_volume > risk_max_today_opening_volume_) {
                    stringstream ss;
                    ss << "[开仓数限制]风控检查失败，委托数:" << order_volume << "，已开仓:" << open_volume << "，最大开仓数限制:" << risk_max_today_opening_volume_;
                    string str = ss.str();
                    str = x::GBKToUTF8(str);
                    throw runtime_error(str);
                }
            }
        }
    }
}  // namespace co
