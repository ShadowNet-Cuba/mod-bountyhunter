/* Copyright 
original script: Sydess
updated Script: Paranoia
released : privatedonut
Reworked : Micrah
Version : 3.3.5 
Bounty Hunter.
Updated : 02/01/2020
*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Creature.h"
#include "Define.h"
#include "GossipDef.h"
#include "ScriptedGossip.h"
#include "Chat.h"
#include "SpellAuraEffects.h"
#include "CreatureAI.h"
#include "ObjectAccessor.h"

#include <cstring>

#define SET_CURRENCY 0  //0 for gold, 1 for honor, 2 for tokens
#define TOKEN_ID 0 // token id

#if SET_CURRENCY == 0
#define BOUNTY_1 "Me gustaría colocar una recompensa de 20 Oro."
#define BOUNTY_2 "Me gustaría colocar una recompensa de 40 Oro."
#define BOUNTY_3 "Me gustaría colocar una recompensa de 100 Oro."
#define BOUNTY_4 "Me gustaría colocar una recompensa de 200 Oro."
#define BOUNTY_5 "Me gustaría colocar una recompensa de 300 Oro."
#define BOUNTY_6 "Me gustaría colocar una recompensa de 400 Oro."
#define BOUNTY_7 "Me gustaría colocar una recompensa de 500 Oro."
#define BOUNTY_8 "Me gustaría colocar una recompensa de 700 Oro."
#endif
#if SET_CURRENCY == 1
#define BOUNTY_1 "Me gustaría colocar una recompensa de 20 honor."
#define BOUNTY_2 "Me gustaría colocar una recompensa de 40 honor."
#define BOUNTY_3 "Me gustaría colocar una recompensa de 100 honor."
#define BOUNTY_4 "Me gustaría colocar una recompensa de 200 honor."
#endif
#if SET_CURRENCY == 2
#define BOUNTY_1 "Me gustaría colocar una recompensa de  1 token."
#define BOUNTY_2 "Me gustaría colocar una recompensa de  3 token."
#define BOUNTY_3 "Me gustaría colocar una recompensa de  5 token."

#endif

#define PLACE_BOUNTY "Me gustaría colocar una recompensa."
#define LIST_BOUNTY "Lista de las recompensas actuales."
#define NVM "Olvídalo"
#define WIPE_BOUNTY "Borrar recompensas"




#if SET_CURRENCY != 2
//these are just visual prices, if you want to to change the real one, edit the sql further below
enum BountyPrice
{
	BOUNTY_PRICE_1 = 20,
	BOUNTY_PRICE_2 = 40,
	BOUNTY_PRICE_3 = 100,
	BOUNTY_PRICE_4 = 200,
	BOUNTY_PRICE_5 = 300,
	BOUNTY_PRICE_6 = 400,
	BOUNTY_PRICE_7 = 500,
	BOUNTY_PRICE_8 = 700,
};
#else
enum BountyPrice
{
	BOUNTY_PRICE_1 = 1,
	BOUNTY_PRICE_2 = 3,
	BOUNTY_PRICE_3 = 5,
	BOUNTY_PRICE_4 = 10,
};
#endif

bool passChecks(Player * pPlayer, const char * name)
{ 

	Player * pBounty = ObjectAccessor::FindPlayerByName(name);
	WorldSession * m_session = pPlayer->GetSession();
	if(!pBounty)
	{
		m_session->SendNotification("El jugador está desconectado o no existe!");
		return false;
	}
	QueryResult result = CharacterDatabase.Query("SELECT * FROM bounties WHERE guid = {}", pBounty->GetGUID().GetCounter());
	if(result)
	{
		m_session->SendNotification("Este jugador ya tiene una recompensa por ellos.!");
		return false;
	}
	if(pPlayer->GetGUID() == pBounty->GetGUID())
	{
		m_session->SendNotification("No puedes ponerte una recompensa a ti mismo!");
		return false;
	}
	return true;
}

void alertServer(const char * name, int msg)
{
	std::string message;
	if(msg == 1)
	{
		message = "Se ha puesto una recompensa por ";
		message += name;
		message += ". ¡Mátalo de inmediato para recibir la recompensa!";
	}
	else if(msg == 2)
	{
		message = "La recompensa en ";
		message += name;
		message += " ha sido recogida!";
	}
	sWorld->SendServerMessage(SERVER_MSG_STRING, message.c_str(), 0);
}


bool hasCurrency(Player * pPlayer, uint32 required, int currency)
{
	WorldSession *m_session = pPlayer->GetSession();
	switch(currency)
	{
		case 0: //gold
			{
			uint32 currentmoney = pPlayer->GetMoney();
			uint32 requiredmoney = (required * 10000);
			if(currentmoney < requiredmoney)
			{
				m_session->SendNotification("No tienes suficiente oro!");
				return false;
			}
			pPlayer->SetMoney(currentmoney - requiredmoney);
			break;
			}
		case 1: //honor
			{
			uint32 currenthonor = pPlayer->GetHonorPoints();
			if(currenthonor < required)
			{
				m_session->SendNotification("No tienes suficiente honor!");
				return false;
			}
			pPlayer->SetHonorPoints(currenthonor - required);
			break;
			}
		case 2: //tokens
			{
			if(!pPlayer->HasItemCount(TOKEN_ID, required))
			{
				m_session->SendNotification("No tienes suficientes tokens!");
				return false;
			}
			pPlayer->DestroyItemCount(TOKEN_ID, required, true, false);
			break;
			}

	}
	return true;
}

void flagPlayer(const char * name)
{
	Player * pBounty = ObjectAccessor::FindPlayerByName(name);
	pBounty->SetPvP(true);
	pBounty->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
}

class BountyHunter : public CreatureScript
{
	public:
		BountyHunter() : CreatureScript("BountyHunter"){}
		bool OnGossipHello(Player * Player, Creature * Creature)
		{
            
			AddGossipItemFor(Player, GOSSIP_ICON_BATTLE, PLACE_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+1);
			AddGossipItemFor(Player, GOSSIP_ICON_TALK, LIST_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+2);
			AddGossipItemFor(Player, GOSSIP_ICON_TALK, NVM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+3);
			if ( Player->IsGameMaster() )
                AddGossipItemFor(Player, GOSSIP_ICON_TALK, WIPE_BOUNTY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+4);

			Player->PlayerTalkClass->SendGossipMenu(907, Creature->GetGUID());
			return true;
		}

		bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
		{
			pPlayer->PlayerTalkClass->ClearMenus();
			switch(uiAction)
			{
				case GOSSIP_ACTION_INFO_DEF+1:
				{
                    AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, BOUNTY_1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+5, "", 0, true);
                    AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, BOUNTY_2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+6, "", 0, true);
                    AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, BOUNTY_3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+7, "", 0, true);
                    AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, BOUNTY_4, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+8, "", 0, true);
					pPlayer->PlayerTalkClass->SendGossipMenu(365, pCreature->GetGUID());
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+2:
				{
					QueryResult Bounties = CharacterDatabase.Query("SELECT * FROM bounties");
					
					if(!Bounties)
					{
						CloseGossipMenuFor(pPlayer);
						return false;
					}
#if SET_CURRENCY == 0
					if(	Bounties->GetRowCount() > 1)
					{
						AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, "Recompensas: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.Query("SELECT name FROM characters WHERE guid = {}", fields[0].Get<uint64>());
							Field * names = name->Fetch();
							option = names[0].Get<std::string>();
							option +=" ";
							option += fields[1].Get<std::string>();
							option += " gold";
							AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
                        AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, "Recompensas: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.Query("SELECT name FROM characters WHERE guid={}", fields[0].Get<uint64>());
						Field * names = name->Fetch();
						option = names[0].Get<std::string>();
						option +=" ";
						option += fields[1].Get<std::string>();
						option += " gold";
                        AddGossipItemFor(pPlayer, GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
#if SET_CURRENCY == 1
					if(	Bounties->GetRowCount() > 1)
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Recompensas: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid={}", fields[0].Get<uint64>());
							Field * names = name->Fetch();
							option = names[0].Get<std::string>();
							option +=" ";
							option += fields[1].Get<std::string>();
							option += " honor";
							pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Recompensas: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid={}", fields[0].Get<uint64>());
						Field * names = name->Fetch();
						option = names[0].Get<std::string>();
						option +=" ";
						option += fields[1].Get<std::string>();
						option += " honor";
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
#if SET_CURRENCY == 2
					if(	Bounties->GetRowCount() > 1)
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Recompensas: ", GOSSIP_SENDER_MAIN, 1);
						do
						{
							Field * fields = Bounties->Fetch();
							std::string option;
							QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid={}", fields[0].Get<uint64>());
							Field * names = name->Fetch();
							option = names[0].Get<std::string>();
							option +=" ";
							option += fields[1].Get<std::string>();
							option += " coins";
							pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						}while(Bounties->NextRow());
					}
					else
					{
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, "Bounties: ", GOSSIP_SENDER_MAIN, 1);
						Field * fields = Bounties->Fetch();
						std::string option;
						QueryResult name = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid={}", fields[0].Get<uint64>());
						Field * names = name->Fetch();
						option = names[0].Get<std::string>();
						option +=" ";
						option += fields[1].Get<std::string>();
						option += " coins";
						pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_BATTLE, option, GOSSIP_SENDER_MAIN, 1);
						
					}
#endif
					pPlayer->PlayerTalkClass->SendGossipMenu(878, pCreature->GetGUID());
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+3:
				{
					CloseGossipMenuFor(pPlayer);
					break;
				}
				case GOSSIP_ACTION_INFO_DEF+4:
				{
					CharacterDatabase.Execute("TRUNCATE TABLE bounties");
					CloseGossipMenuFor(pPlayer);
					break;
				}
			}
			return true;
		}

		bool OnGossipSelectCode(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char * code)
		{
			pPlayer->PlayerTalkClass->ClearMenus();
			if ( uiSender == GOSSIP_SENDER_MAIN )
			{
				if(islower(code[0]))
					toupper(code[0]);

				if(passChecks(pPlayer, code))
				{
					Player * pBounty = ObjectAccessor::FindPlayerByName(code);
					switch (uiAction)
					{
						case GOSSIP_ACTION_INFO_DEF+5:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_1, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({},'20', '1')", pBounty->GetGUID().GetCounter());
								#else
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({},'1', '1')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								CloseGossipMenuFor(pPlayer);
							}
							break;
						}
							
						case GOSSIP_ACTION_INFO_DEF+6:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_2, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({}, '40', '2')", pBounty->GetGUID().GetCounter());
								#else
								CharacterDatabase.PExecute("INSERT INTO bounties VALUES({}, '3', '2')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								CloseGossipMenuFor(pPlayer);
							}
							break;
						}
						case GOSSIP_ACTION_INFO_DEF+7:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_3, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({}, '100', '3')", pBounty->GetGUID().GetCounter());
								#else
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({}, '5', '3')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								CloseGossipMenuFor(pPlayer);
							}
							break;
						}
						case GOSSIP_ACTION_INFO_DEF+8:
						{
							if(hasCurrency(pPlayer, BOUNTY_PRICE_4, SET_CURRENCY))
							{
								#if SET_CURRENCY != 2
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({}, '200', '4')", pBounty->GetGUID().GetCounter());
								#else
								CharacterDatabase.Execute("INSERT INTO bounties VALUES({}, '10', '3')", pBounty->GetGUID());
								#endif
								alertServer(code, 1);
								flagPlayer(code);
								CloseGossipMenuFor(pPlayer);
							}
							break;
						}
						

					}
				}
				else
					CloseGossipMenuFor(pPlayer);
			}
			return true;
		}
};

class BountyhunterAnnounce : public PlayerScript
{

public:

    BountyhunterAnnounce() : PlayerScript("BountyhunterAnnounce") {}

    void OnLogin(Player* player)
    {
        // Announce Module
        if (sConfigMgr->GetBoolDefault("BountyhunterAnnounce.Enable", true))
        {
                ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Bounty Hunter |rmodule.");
         }
    }
};

class BountyKills : public PlayerScript
{
	public:
		BountyKills() : PlayerScript("BountyKills"){}
		
		void OnPVPKill(Player * Killer, Player * Bounty)
    
    {
			        // If enabled...
        if (sConfigMgr->GetBoolDefault("BountyHunter.Enable", true)) 
		{
			if(Killer->GetGUID() == Bounty->GetGUID())
				return;

			QueryResult result = CharacterDatabase.Query("SELECT * FROM bounties WHERE guid= {}", Bounty->GetGUID().GetCounter());
			if(!result)
				return;

			Field * fields = result->Fetch();
			#if SET_CURRENCY == 0
				switch(fields[2].Get<uint64>())
				{
				case 1:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_1 * 10000));
					break;
				case 2:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_2 * 10000));
					break;
				case 3:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_3 * 10000));
					break;
				case 4:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_4 * 10000));
					break;
				case 5:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_5 * 10000));
					break;
				case 6:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_6 * 10000));
					break;
				case 7:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_7 * 10000));
					break;
				case 8:
					Killer->SetMoney(Killer->GetMoney() + (BOUNTY_PRICE_8 * 10000));
					break;
				}
			#endif

			#if SET_CURRENCY == 1
				switch(fields[2].Get<uint64>())
				{
				case 1:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_1));
					break;
				case 2:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_2));
					break;
				case 3:
					Killer->SetHonorPoints(Killer->GetHonorPoints() + (BOUNTY_PRICE_3));
					break;
				case 4:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_4));
					break;
				case 5:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_5));
					break;
				case 6:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_6));
					break;
				case 7:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_7));
					break;	
				case 8:
					Killer->SetHonorPoints(Killer->GetHonorPoints()) + (BOUNTY_PRICE_8));
					break;
				}
			#endif

			#if SET_CURRENCY == 2
				switch(fields[2].Get<uint64>())
				{
				case 1:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_1);
					break;
				case 2:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_2);
					break;
				case 3:
					Killer->AddItem(TOKEN_ID, BOUNTY_PRICE_3);
					break;

				}
			#endif
			CharacterDatabase.Execute("DELETE FROM bounties WHERE guid= {}", Bounty->GetGUID().GetCounter());
			alertServer(Bounty->GetName().c_str(), 2);
        }

	}
};

class BountyHunterConfig : public WorldScript
{
public:
    BountyHunterConfig() : WorldScript("BountyHunterConfig") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            std::string conf_path = _CONF_DIR;
            std::string cfg_file = conf_path + "/bountyhunter.conf";

            std::string cfg_def_file = cfg_file + ".dist";
         
        }
    }
};

void AddBountyHunterScripts()
{
    new BountyHunterConfig();
    new BountyhunterAnnounce();
	new BountyHunter();
	new BountyKills();
}
