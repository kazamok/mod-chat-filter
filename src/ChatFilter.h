#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include <vector>
#include <string>

class Channel;

class ChatFilter : public PlayerScript
{
public:
    ChatFilter();

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg);
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* channel);

private:
    bool IsBadWord(const std::string& msg);
    void LogMessage(Player* player, const std::string& originalMsg);
};

void AddChatFilterScripts();
