"""openctp TTS 交易连接测试"""
import fix_locale  # noqa: F401  must precede openctp_ctp
import os
import time
from pathlib import Path
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env")

from openctp_ctp import tdapi

BROKER_ID = os.getenv("CTP_BROKER_ID")
USER_ID = os.getenv("CTP_USER_ID")
PASSWORD = os.getenv("CTP_PASSWORD")
TD_FRONT = os.getenv("CTP_TD_FRONT")
APP_ID = os.getenv("CTP_APP_ID", "")
AUTH_CODE = os.getenv("CTP_AUTH_CODE", "")


class TdSpi(tdapi.CThostFtdcTraderSpi):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self.login_ok = False
        self.front_id = 0
        self.session_id = 0

    def OnFrontConnected(self):
        print("[TD] 前置连接成功")
        req = tdapi.CThostFtdcReqAuthenticateField()
        req.BrokerID = BROKER_ID
        req.UserID = USER_ID
        req.AppID = APP_ID
        req.AuthCode = AUTH_CODE
        self.api.ReqAuthenticate(req, 0)

    def OnFrontDisconnected(self, nReason):
        print(f"[TD] 前置断开连接, 原因: {nReason}")

    def OnRspAuthenticate(self, pRspAuthenticateField, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[TD] 认证失败: {pRspInfo.ErrorID} - {pRspInfo.ErrorMsg}")
            return
        print("[TD] 客户端认证成功")
        req = tdapi.CThostFtdcReqUserLoginField()
        req.BrokerID = BROKER_ID
        req.UserID = USER_ID
        req.Password = PASSWORD
        self.api.ReqUserLogin(req, 0)

    def OnRspUserLogin(self, pRspUserLogin, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[TD] 登录失败: {pRspInfo.ErrorID} - {pRspInfo.ErrorMsg}")
            return
        self.front_id = pRspUserLogin.FrontID
        self.session_id = pRspUserLogin.SessionID
        self.login_ok = True
        print(f"[TD] 登录成功")
        print(f"  交易日: {pRspUserLogin.TradingDay}")
        print(f"  FrontID: {self.front_id}, SessionID: {self.session_id}")
        print(f"  最大报单引用: {pRspUserLogin.MaxOrderRef}")

        # 确认结算单
        req = tdapi.CThostFtdcSettlementInfoConfirmField()
        req.BrokerID = BROKER_ID
        req.InvestorID = USER_ID
        self.api.ReqSettlementInfoConfirm(req, 0)

    def OnRspSettlementInfoConfirm(self, pSettlementInfoConfirm, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[TD] 确认结算单失败: {pRspInfo.ErrorMsg}")
            return
        print("[TD] 结算单确认成功")
        # 查询资金
        req = tdapi.CThostFtdcQryTradingAccountField()
        req.BrokerID = BROKER_ID
        req.InvestorID = USER_ID
        self.api.ReqQryTradingAccount(req, 0)

    def OnRspQryTradingAccount(self, pTradingAccount, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[TD] 查询资金失败: {pRspInfo.ErrorMsg}")
            return
        if pTradingAccount:
            print(f"\n[TD] === 账户资金 ===")
            print(f"  可用资金: {pTradingAccount.Available:.2f}")
            print(f"  当前保证金: {pTradingAccount.CurrMargin:.2f}")
            print(f"  冻结资金: {pTradingAccount.FrozenCash:.2f}")
            print(f"  平仓盈亏: {pTradingAccount.CloseProfit:.2f}")
            print(f"  持仓盈亏: {pTradingAccount.PositionProfit:.2f}")

        if bIsLast:
            # 查询持仓
            time.sleep(1)
            req = tdapi.CThostFtdcQryInvestorPositionField()
            req.BrokerID = BROKER_ID
            req.InvestorID = USER_ID
            self.api.ReqQryInvestorPosition(req, 0)

    def OnRspQryInvestorPosition(self, pInvestorPosition, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[TD] 查询持仓失败: {pRspInfo.ErrorMsg}")
            return
        if pInvestorPosition and pInvestorPosition.InstrumentID:
            direction = "多" if pInvestorPosition.PosiDirection == "2" else "空"
            print(
                f"[TD] 持仓: {pInvestorPosition.InstrumentID} | "
                f"{direction} | 数量: {pInvestorPosition.Position} | "
                f"持仓盈亏: {pInvestorPosition.PositionProfit:.2f}"
            )
        if bIsLast:
            self.query_done = True
            print("\n[TD] 查询完成，交易接口测试通过")


def run_td_test():
    print("=" * 60)
    print("openctp TTS 交易连接测试")
    print(f"BrokerID: {BROKER_ID}, UserID: {USER_ID}")
    print(f"TD Front: {TD_FRONT}")
    print("=" * 60)

    api = tdapi.CThostFtdcTraderApi.CreateFtdcTraderApi("./td_flow/")
    spi = TdSpi(api)
    api.RegisterSpi(spi)
    api.SubscribePrivateTopic(tdapi.THOST_TERT_QUICK)
    api.SubscribePublicTopic(tdapi.THOST_TERT_QUICK)
    api.RegisterFront(TD_FRONT)
    api.Init()

    time.sleep(15)
    if not spi.login_ok:
        print("[TD] 超时: 未能成功登录")
    else:
        print("\n[TD] 测试完成")
    return spi.login_ok


def test_td_connectivity():
    """pytest: 交易接口连通性测试"""
    assert run_td_test(), "TD login failed within timeout"


if __name__ == "__main__":
    import sys
    sys.exit(0 if run_td_test() else 1)
