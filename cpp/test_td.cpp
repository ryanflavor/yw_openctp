#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ThostFtdcTraderApi.h"
#include "env_loader.h"
#include "fix_locale.h"

static std::string BROKER_ID, USER_ID, PASSWORD, TD_FRONT, APP_ID, AUTH_CODE;
static std::atomic<bool> g_login_ok{false};
static std::atomic<bool> g_query_done{false};
static std::mutex g_mtx;
static std::condition_variable g_cv;

class TdSpi : public CThostFtdcTraderSpi {
public:
    explicit TdSpi(CThostFtdcTraderApi *api) : api_(api) {}

    void OnFrontConnected() override {
        printf("[TD] Connected\n");
        CThostFtdcReqAuthenticateField req{};
        strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, USER_ID.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.AppID, APP_ID.c_str(), sizeof(req.AppID) - 1);
        strncpy(req.AuthCode, AUTH_CODE.c_str(), sizeof(req.AuthCode) - 1);
        api_->ReqAuthenticate(&req, 0);
    }

    void OnFrontDisconnected(int nReason) override {
        printf("[TD] Disconnected, reason: %d\n", nReason);
    }

    void OnRspAuthenticate(CThostFtdcRspAuthenticateField *p, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[TD] Auth failed: %d - %s\n", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            return;
        }
        printf("[TD] Auth OK\n");
        CThostFtdcReqUserLoginField req{};
        strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, USER_ID.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.Password, PASSWORD.c_str(), sizeof(req.Password) - 1);
        api_->ReqUserLogin(&req, 0);
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[TD] Login failed: %d - %s\n", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            return;
        }
        printf("[TD] Login OK, TradingDay: %s, FrontID: %d, SessionID: %d\n",
               pRspUserLogin->TradingDay, pRspUserLogin->FrontID, pRspUserLogin->SessionID);
        g_login_ok = true;

        CThostFtdcSettlementInfoConfirmField req{};
        strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.InvestorID, USER_ID.c_str(), sizeof(req.InvestorID) - 1);
        api_->ReqSettlementInfoConfirm(&req, 0);
    }

    void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *p, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[TD] Settlement confirm failed: %s\n", pRspInfo->ErrorMsg);
            return;
        }
        printf("[TD] Settlement confirmed\n");
        CThostFtdcQryTradingAccountField req{};
        strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.InvestorID, USER_ID.c_str(), sizeof(req.InvestorID) - 1);
        api_->ReqQryTradingAccount(&req, 0);
    }

    void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[TD] Query account failed: %s\n", pRspInfo->ErrorMsg);
            return;
        }
        if (pTradingAccount) {
            printf("[TD] Available: %.2f, Margin: %.2f, FrozenCash: %.2f\n",
                   pTradingAccount->Available, pTradingAccount->CurrMargin, pTradingAccount->FrozenCash);
        }
        if (bIsLast) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            CThostFtdcQryInvestorPositionField req{};
            strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
            strncpy(req.InvestorID, USER_ID.c_str(), sizeof(req.InvestorID) - 1);
            api_->ReqQryInvestorPosition(&req, 0);
        }
    }

    void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pPos, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[TD] Query position failed: %s\n", pRspInfo->ErrorMsg);
            return;
        }
        if (pPos && pPos->InstrumentID[0]) {
            const char *dir = (pPos->PosiDirection == '2') ? "Long" : "Short";
            printf("[TD] Position: %s | %s | Qty: %d | PnL: %.2f\n",
                   pPos->InstrumentID, dir, pPos->Position, pPos->PositionProfit);
        }
        if (bIsLast) {
            printf("[TD] Query complete, test passed\n");
            g_query_done = true;
            g_cv.notify_all();
        }
    }

private:
    CThostFtdcTraderApi *api_;
};

int main() {
    fix_locale();
    auto env = load_env("../.env");
    BROKER_ID = env["CTP_BROKER_ID"];
    USER_ID   = env["CTP_USER_ID"];
    PASSWORD  = env["CTP_PASSWORD"];
    TD_FRONT  = env["CTP_TD_FRONT"];
    APP_ID    = env["CTP_APP_ID"];
    AUTH_CODE = env["CTP_AUTH_CODE"];

    printf("============================================================\n");
    printf("openctp TTS TD connectivity test (C++)\n");
    printf("BrokerID: %s, UserID: %s\n", BROKER_ID.c_str(), USER_ID.c_str());
    printf("TD Front: %s\n", TD_FRONT.c_str());
    printf("============================================================\n");

    auto *api = CThostFtdcTraderApi::CreateFtdcTraderApi("./td_flow/");
    TdSpi spi(api);
    api->RegisterSpi(&spi);
    api->SubscribePrivateTopic(THOST_TERT_QUICK);
    api->SubscribePublicTopic(THOST_TERT_QUICK);
    api->RegisterFront(const_cast<char *>(TD_FRONT.c_str()));
    api->Init();

    {
        std::unique_lock<std::mutex> lk(g_mtx);
        if (!g_cv.wait_for(lk, std::chrono::seconds(15), [] { return g_query_done.load(); })) {
            if (!g_login_ok) {
                printf("[TD] Timeout: login failed\n");
                api->RegisterSpi(nullptr);
                api->Release();
                return 1;
            }
        }
    }

    printf("[TD] Test completed\n");
    api->RegisterSpi(nullptr);
    api->Release();
    return 0;
}
