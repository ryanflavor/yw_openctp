#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ThostFtdcMdApi.h"
#include "env_loader.h"
#include "fix_locale.h"

static std::string BROKER_ID, USER_ID, PASSWORD, MD_FRONT;
static std::atomic<bool> g_login_ok{false};
static std::atomic<int> g_tick_count{0};
static std::mutex g_mtx;
static std::condition_variable g_cv;

static const char *INSTRUMENTS[] = {"ag2606", "IF2604"};
static const int NUM_INSTRUMENTS = 2;

class MdSpi : public CThostFtdcMdSpi {
public:
    explicit MdSpi(CThostFtdcMdApi *api) : api_(api) {}

    void OnFrontConnected() override {
        printf("[MD] Connected\n");
        CThostFtdcReqUserLoginField req{};
        strncpy(req.BrokerID, BROKER_ID.c_str(), sizeof(req.BrokerID) - 1);
        strncpy(req.UserID, USER_ID.c_str(), sizeof(req.UserID) - 1);
        strncpy(req.Password, PASSWORD.c_str(), sizeof(req.Password) - 1);
        api_->ReqUserLogin(&req, 0);
    }

    void OnFrontDisconnected(int nReason) override {
        printf("[MD] Disconnected, reason: %d\n", nReason);
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0) {
            printf("[MD] Login failed: %d - %s\n", pRspInfo->ErrorID, pRspInfo->ErrorMsg);
            return;
        }
        g_login_ok = true;
        printf("[MD] Login OK, TradingDay: %s\n", pRspUserLogin->TradingDay);

        char *ppInstrumentID[NUM_INSTRUMENTS];
        for (int i = 0; i < NUM_INSTRUMENTS; i++)
            ppInstrumentID[i] = const_cast<char *>(INSTRUMENTS[i]);
        api_->SubscribeMarketData(ppInstrumentID, NUM_INSTRUMENTS);
    }

    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override {
        if (pRspInfo && pRspInfo->ErrorID != 0)
            printf("[MD] Subscribe failed %s: %s\n", pInstrument->InstrumentID, pRspInfo->ErrorMsg);
        else
            printf("[MD] Subscribed: %s\n", pInstrument->InstrumentID);
    }

    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pData) override {
        int cnt = ++g_tick_count;
        printf("[MD] %10s | Last: %10.2f | Bid: %10.2f x %-5d | Ask: %10.2f x %-5d | Vol: %d\n",
               pData->InstrumentID, pData->LastPrice,
               pData->BidPrice1, pData->BidVolume1,
               pData->AskPrice1, pData->AskVolume1,
               pData->Volume);
        if (cnt >= 20) {
            printf("\n[MD] Received %d ticks, test passed\n", cnt);
            g_cv.notify_all();
        }
    }

private:
    CThostFtdcMdApi *api_;
};

int main() {
    fix_locale();
    auto env = load_env("../.env");
    BROKER_ID = env["CTP_BROKER_ID"];
    USER_ID   = env["CTP_USER_ID"];
    PASSWORD  = env["CTP_PASSWORD"];
    MD_FRONT  = env["CTP_MD_FRONT"];

    printf("============================================================\n");
    printf("openctp TTS MD connectivity test (C++)\n");
    printf("BrokerID: %s, UserID: %s\n", BROKER_ID.c_str(), USER_ID.c_str());
    printf("MD Front: %s\n", MD_FRONT.c_str());
    printf("============================================================\n");

    auto *api = CThostFtdcMdApi::CreateFtdcMdApi("./md_flow/");
    MdSpi spi(api);
    api->RegisterSpi(&spi);
    api->RegisterFront(const_cast<char *>(MD_FRONT.c_str()));
    api->Init();

    {
        std::unique_lock<std::mutex> lk(g_mtx);
        if (!g_cv.wait_for(lk, std::chrono::seconds(30), [] { return g_tick_count >= 20; })) {
            if (!g_login_ok) {
                printf("[MD] Timeout: login failed\n");
                api->RegisterSpi(nullptr);
                api->Release();
                return 1;
            }
            if (g_tick_count == 0) {
                printf("[MD] Login OK but no ticks (may not be trading hours)\n");
            }
        }
    }

    printf("[MD] Test completed, received %d ticks\n", g_tick_count.load());
    api->RegisterSpi(nullptr);
    api->Release();
    return 0;
}
