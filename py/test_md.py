"""openctp TTS 行情连接测试"""
import fix_locale  # noqa: F401  must precede openctp_ctp
import os
import time
from pathlib import Path
from dotenv import load_dotenv

load_dotenv(Path(__file__).resolve().parent.parent / ".env")

from openctp_ctp import mdapi

BROKER_ID = os.getenv("CTP_BROKER_ID")
USER_ID = os.getenv("CTP_USER_ID")
PASSWORD = os.getenv("CTP_PASSWORD")
MD_FRONT = os.getenv("CTP_MD_FRONT")

SUBSCRIBE_INSTRUMENTS = ["ag2606", "IF2604"]


class MdSpi(mdapi.CThostFtdcMdSpi):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self.login_ok = False
        self.tick_count = 0

    def OnFrontConnected(self):
        print("[MD] 前置连接成功")
        req = mdapi.CThostFtdcReqUserLoginField()
        req.BrokerID = BROKER_ID
        req.UserID = USER_ID
        req.Password = PASSWORD
        self.api.ReqUserLogin(req, 0)

    def OnFrontDisconnected(self, nReason):
        print(f"[MD] 前置断开连接, 原因: {nReason}")

    def OnRspUserLogin(self, pRspUserLogin, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(f"[MD] 登录失败: {pRspInfo.ErrorID} - {pRspInfo.ErrorMsg}")
            return
        self.login_ok = True
        print(f"[MD] 登录成功, 交易日: {pRspUserLogin.TradingDay}")
        print(f"[MD] 订阅合约: {SUBSCRIBE_INSTRUMENTS}")
        self.api.SubscribeMarketData(
            [i.encode() if isinstance(i, str) else i for i in SUBSCRIBE_INSTRUMENTS],
            len(SUBSCRIBE_INSTRUMENTS),
        )

    def OnRspSubMarketData(self, pSpecificInstrument, pRspInfo, nRequestID, bIsLast):
        if pRspInfo and pRspInfo.ErrorID != 0:
            print(
                f"[MD] 订阅失败 {pSpecificInstrument.InstrumentID}: "
                f"{pRspInfo.ErrorMsg}"
            )
        else:
            print(f"[MD] 订阅成功: {pSpecificInstrument.InstrumentID}")

    def OnRtnDepthMarketData(self, pData):
        self.tick_count += 1
        print(
            f"[MD] {pData.InstrumentID:>10} | "
            f"最新: {pData.LastPrice:>10.2f} | "
            f"买一: {pData.BidPrice1:>10.2f} x {pData.BidVolume1:<5} | "
            f"卖一: {pData.AskPrice1:>10.2f} x {pData.AskVolume1:<5} | "
            f"成交量: {pData.Volume}"
        )
        if self.tick_count >= 20:
            print(f"\n[MD] 已收到 {self.tick_count} 个 tick，行情测试通过")


def run_md_test():
    print("=" * 60)
    print("openctp TTS 行情连接测试")
    print(f"BrokerID: {BROKER_ID}, UserID: {USER_ID}")
    print(f"MD Front: {MD_FRONT}")
    print(f"订阅合约: {SUBSCRIBE_INSTRUMENTS}")
    print("=" * 60)

    api = mdapi.CThostFtdcMdApi.CreateFtdcMdApi("./md_flow/")
    spi = MdSpi(api)
    api.RegisterSpi(spi)
    api.RegisterFront(MD_FRONT)
    api.Init()

    for _ in range(30):
        time.sleep(1)
        if spi.tick_count >= 20:
            break

    if not spi.login_ok:
        print("[MD] 超时: 未能成功登录")
    elif spi.tick_count == 0:
        print("[MD] 登录成功但未收到行情（可能非交易时间）")
    else:
        print(f"[MD] 测试完成，共收到 {spi.tick_count} 个 tick")
    return spi.login_ok


def test_md_connectivity():
    """行情接口连通性测试"""
    assert run_md_test(), "MD login failed within timeout"


if __name__ == "__main__":
    import sys
    sys.exit(0 if run_md_test() else 1)
