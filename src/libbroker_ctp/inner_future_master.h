// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <x/x.h>
#include <coral/coral.h>
#include "inner_future_position.h"
#include "inner_future_order.h"
using namespace std;

namespace co {
    /**
     * 期货内部持仓管理
     * 工作流程：
     * 1.系统启动时，使用开盘前的昨持仓进行初始化；
     * 2.接收到委托状态更新时，更新内部持仓；
     * 3.报单时，计算自动开平仓逻辑，并返回处理后的买卖方向和开平仓标记；该步骤里面只计算，并不更新内部持仓数据。
     *
     * “平今”和“平昨”的区别：
     * 上期所的持仓分今仓（当日开仓）和昨仓（历史持仓），平仓时需要指定是平今仓还是昨仓。
     * 若对上期所的持仓直接使用THOST_FTDC_OF_Close，则效果同使用THOST_FTDC_OF_CloseYesterday；
     * 若对其他交易所的持仓使用THOST_FTDC_OF_CloseToday或THOST_FTDC_OF_CloseYesterday，则效果同THOST_FTDC_OF_Close。
     *
     * 平仓顺序
     * 1.国内四家交易所的平仓顺序统一规则为先开先平。
     * 2.郑商所在此基础上还有先平单腿持仓，再平组合持仓。
     * 3.除上期所外的三家交易在涉及到平今手续费减免时先平今后平昨（后开先平）。
     */
class InnerFutureMaster {
 public:
    void Init(const vector<MemTradePosition>& positions);
    void Update(const co::fbs::TradeOrderT& order);

    /**
        * 计算自动开平仓方向
        * @param order: 委托
        * @return: 处理后的开平仓标记
        */
    int64_t GetAutoOcFlag(const co::fbs::TradeOrderT& order);

    int64_t GetCloseYestodayFlag(const co::fbs::TradeOrderT& order);

    inline void set_risk_forbid_closing_today(bool value) {
        risk_forbid_closing_today_ = value;
    }

    inline void set_risk_max_today_opening_volume(int64_t value) {
        risk_max_today_opening_volume_ = value;
    }

 protected:
    string GetKey(string code, int64_t hedge_flag, int64_t bs_flag);
    InnerFuturePositionPtr GetPosition(string code, int64_t hedge_flag, int64_t bs_flag);
    void CheckRisk(string code, int64_t bs_flag, int64_t oc_flag, int64_t order_volume);

 private:
    int state_ = 0;  // 0-未初始化，1-初始化中，2-完成初始化
    vector<std::shared_ptr<co::fbs::TradeOrderT>> init_orders_;  // 等待初始化的委托列表，因为程序启动后委托会先推过来，之后才能查询持仓进行初始化
    map<string, InnerFuturePositionPtr> positions_;  // <code>_<hedge_flag>_<bs_flag>
    map<string, InnerFutureOrderPtr> orders_;  // order_no -> order

    // ----------------------------------------
    bool risk_forbid_closing_today_ = false;  // 风控策略：禁止股指期货自动开平仓时平今仓
    int64_t risk_max_today_opening_volume_ = 0;  // 风控策略：限制股指期货当日最大开仓数
    map<string, int64_t> open_cache_;  // 合约类型（IF、IH、IC） -> 已开仓数 + 开仓冻结数，用于限制当日最大开仓数
};

typedef std::shared_ptr<InnerFutureMaster> InnerFutureMasterPtr;
}  // namespace co
