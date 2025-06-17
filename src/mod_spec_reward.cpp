/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

// By BetaYi (QQ:1518429315 ; 1518429315@qq.com) 群：719526711

#include "ScriptMgr.h"
#include "Config.h"
#include <Player.h>
#include "Group.h"
#include "GroupMgr.h"
#include "InstanceScript.h"
#include "Chat.h"
#include "WorldSession.h"
#include "Creature.h"
#include "WorldSessionMgr.h"
#include "Mail.h"

static bool sModuleEnabled, sAnnounceEnabled;
static uint32 sRewardSpecFlags, sMinBossLevel, sDungeonTokenId, sRaidTokenId, sTokenCount;
std::string sHealerRewardText;
std::string sTankRewardText;
std::string sDpsRewardText;

enum SpecType
{
    FLAG_SPEC_HEALER = 0x00000001,
    FLAG_SPEC_DPS    = 0x00000002,
    FLAG_SPEC_TANK   = 0x00000004
};

class SpecReward : public PlayerScript
{
public:
    SpecReward() : PlayerScript("SpecReward") { }

    void OnPlayerLogin(Player* player) override
    {
        if (sModuleEnabled && sAnnounceEnabled)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cff4CFF00[系统]|r 根据天赋发放击杀副本Boss奖励模块已启用。");
        }
    }

    void GiveReward(Player* player, uint32 itemId, uint32 count)
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count) == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, itemId, count, true);
        }
        else
        {
            MailDraft draft("副本奖励", "尊敬的玩家：\n\n由于您的背包已满，系统已通过邮件将奖励发放给您，请注意查收。\n\n感谢您的参与，祝游戏愉快！");
            Item* item = Item::CreateItem(itemId, count, player);
            if (item)
            {
                draft.AddItem(item);
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                draft.SendMailTo(trans, MailReceiver(player), MailSender(MAIL_CREATURE, 0), MAIL_CHECK_MASK_COPIED);
                CharacterDatabase.CommitTransaction(trans);
            }
        }
    }

    void OnPlayerCreatureKill(Player* killer, Creature* boss) override
    {
        if (!sModuleEnabled || !boss->IsDungeonBoss() || boss->GetLevel() <= sMinBossLevel)
            return;

        Map* map = killer->GetMap();
        bool isRaid = map->IsRaid();
        uint32 tokenItemId = isRaid ? sRaidTokenId : sDungeonTokenId;

        std::ostringstream rewardLog;
        rewardLog << "[击杀Boss: " << boss->GetNameForLocaleIdx(LOCALE_zhCN) << "]\n";

        uint32 rewardCount = 0;
        for (const auto& playerRef : map->GetPlayers())
        {
            Player* member = playerRef.GetSource();
            if (!member)
                continue;

            std::string name = member->GetName();

            if (member->HasHealSpec() && (sRewardSpecFlags & FLAG_SPEC_HEALER))
            {
                GiveReward(member, tokenItemId, sTokenCount);
                rewardLog << " - |cff00ff00" << name << "|r " << sHealerRewardText << "\n";
                ++rewardCount;
            }
            else if (member->HasTankSpec() && (sRewardSpecFlags & FLAG_SPEC_TANK))
            {
                GiveReward(member, tokenItemId, sTokenCount);
                rewardLog << " - |cff00aaff" << name << "|r " << sTankRewardText << "\n";
                ++rewardCount;
            }
            else if (member->HasCasterSpec() && (sRewardSpecFlags & FLAG_SPEC_DPS))
            {
                GiveReward(member, tokenItemId, sTokenCount);
                rewardLog << " - |cffffcc00" << name << "|r " << sDpsRewardText << "\n";
                ++rewardCount;
            }
        }

        if (rewardCount > 0)
        {
            for (const auto& playerRef : map->GetPlayers())
            {
                Player* member = playerRef.GetSource();
                if (member)
                {
                    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, rewardLog.str().c_str(), member);
                }
            }
        }
    }
};

class SpecRewardConfig : public WorldScript
{
public:
    SpecRewardConfig() : WorldScript("SpecRewardConfig") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload)
            LoadConfig();
    }

    void LoadConfig()
    {
        sModuleEnabled      = sConfigMgr->GetOption<bool>("SpecReward.Enable", true);
        sAnnounceEnabled    = sConfigMgr->GetOption<bool>("SpecReward.Announce", true);
        sRewardSpecFlags    = sConfigMgr->GetOption<uint32>("SpecReward.SpecFlags", 7);
        sMinBossLevel       = sConfigMgr->GetOption<uint32>("SpecReward.MinBossLevel", 80);
        sDungeonTokenId     = sConfigMgr->GetOption<uint32>("SpecReward.DungeonToken", 38186);
        sRaidTokenId        = sConfigMgr->GetOption<uint32>("SpecReward.RaidToken", 38186);
        sTokenCount         = sConfigMgr->GetOption<uint32>("SpecReward.TokenCount", 1);
        sHealerRewardText   = sConfigMgr->GetOption<std::string>("SpecReward.HealerText", "获得了治疗奖励");
        sTankRewardText     = sConfigMgr->GetOption<std::string>("SpecReward.TankText", "获得了坦克奖励");
        sDpsRewardText      = sConfigMgr->GetOption<std::string>("SpecReward.DPSText", "获得了输出奖励");
    }
};

void Addmod_spec_rewardScripts()
{
    new SpecRewardConfig();
    new SpecReward();
}