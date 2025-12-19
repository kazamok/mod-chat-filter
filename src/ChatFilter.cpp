#include "ChatFilter.h"
#include "Chat.h"
#include "Log.h"
#include "WorldSessionMgr.h"
#include "AccountMgr.h"
#include <fstream>
#include <ctime>
#include <filesystem>
#include <sstream>

// Configuration storage
bool cf_enabled;
bool cf_log_enabled;
std::vector<std::string> cf_badwords;
bool cf_sender_see_message; // Not used yet, but loaded

// Helper to split the badwords string
void split(const std::string& s, char delim, std::vector<std::string>& elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            // Convert to lowercase for case-insensitive comparison
            std::transform(item.begin(), item.end(), item.begin(),
                [](unsigned char c){ return std::tolower(c); });
            elems.push_back(item);
        }
    }
}

class ChatFilterConfig : public WorldScript
{
public:
    ChatFilterConfig() : WorldScript("ChatFilterConfig") {}

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            cf_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Enable", true);
            cf_log_enabled = sConfigMgr->GetOption<bool>("ChatFilter.Log.Enable", true);
            cf_sender_see_message = sConfigMgr->GetOption<bool>("ChatFilter.Sender.SeeMessage", false);
            std::string badwords_str = sConfigMgr->GetOption<std::string>("ChatFilter.Badwords", "badword1,badword2");
            split(badwords_str, ',', cf_badwords);

            if (cf_enabled) {
                LOG_INFO("server.loading", "채팅 필터 모듈이 활성화되었습니다.");
                if (cf_log_enabled) {
                    LOG_INFO("server.loading", "채팅 필터 로깅이 활성화되었습니다.");
                }
            }
        }
    }
};

ChatFilter::ChatFilter() : PlayerScript("ChatFilter") {}

bool ChatFilter::IsBadWord(const std::string& msg)
{
    std::string lower_msg = msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(),
        [](unsigned char c){ return std::tolower(c); });

    for (const auto& badword : cf_badwords) {
        if (lower_msg.find(badword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void ChatFilter::LogMessage(Player* player, const std::string& originalMsg)
{
    if (!cf_log_enabled) {
        return;
    }

    // Create directory if it doesn't exist
    if (!std::filesystem::is_directory("logs/chatfilter") || !std::filesystem::exists("logs/chatfilter")) {
        std::filesystem::create_directory("logs/chatfilter");
    }

    time_t now = time(0);
    tm* ltm = localtime(&now);

    char date_buf[20];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", ltm);
    std::string log_file = "logs/chatfilter/" + std::string(date_buf) + ".log";

    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", ltm);
    
    std::string accountName;
    if (player->GetSession())
    {
        AccountMgr::GetName(player->GetSession()->GetAccountId(), accountName);
    }

    std::ofstream logfile;
    logfile.open(log_file, std::ios_base::app);
    if (logfile.is_open())
    {
        logfile << "[" << time_buf << "] ";
        logfile << "Player: " << player->GetName() << " ";
        logfile << "(Account: " << accountName << ", GUID: " << player->GetGUID().GetCounter() << ") ";
        logfile << "said: " << originalMsg << std::endl;
        logfile.close();
    }
}

// Handler for Say, Yell, Whisper, etc.
void ChatFilter::OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg)
{
    if (!cf_enabled || player->IsGameMaster() || lang == LANG_ADDON) {
        return;
    }
    
    if (IsBadWord(msg))
    {
        LogMessage(player, msg);
        msg.clear(); 
        ChatHandler(player->GetSession()).PSendSysMessage("Your message contained inappropriate language and was not sent.");
    }
}

// Handler for Channel messages
void ChatFilter::OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* /*channel*/)
{
    // The logic is identical, so we just call the other handler.
    OnPlayerChat(player, type, lang, msg);
}

void AddSC_mod_chat_filter()
{
    new ChatFilterConfig();
    new ChatFilter();
}
