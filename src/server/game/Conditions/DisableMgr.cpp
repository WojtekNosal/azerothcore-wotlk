/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "DisableMgr.h"
#include "ObjectMgr.h"
#include "OutdoorPvP.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "VMapManager2.h"

namespace DisableMgr
{

    namespace
    {
        struct DisableData
        {
            uint8 flags;
            std::set<uint32> params[2];                             // params0, params1
        };

        // single disables here with optional data
        typedef std::map<uint32, DisableData> DisableTypeMap;
        // global disable map by source
        typedef std::map<DisableType, DisableTypeMap> DisableMap;

        DisableMap m_DisableMap;

        uint8 MAX_DISABLE_TYPES = 9;
    }

    void LoadDisables()
    {
        uint32 oldMSTime = getMSTime();

        // reload case
        for (DisableMap::iterator itr = m_DisableMap.begin(); itr != m_DisableMap.end(); ++itr)
            itr->second.clear();

        m_DisableMap.clear();

        QueryResult result = WorldDatabase.Query("SELECT sourceType, entry, flags, params_0, params_1 FROM disables");

        uint32 total_count = 0;

        if (!result)
        {
            sLog->outString(">> Loaded 0 disables. DB table `disables` is empty!");
            sLog->outString();
            return;
        }

        Field* fields;
        do
        {
            fields = result->Fetch();
            DisableType type = DisableType(fields[0].GetUInt32());
            if (type >= MAX_DISABLE_TYPES)
            {
                sLog->outErrorDb("Invalid type %u specified in `disables` table, skipped.", type);
                continue;
            }

            uint32 entry = fields[1].GetUInt32();
            uint8 flags = fields[2].GetUInt8();
            std::string params_0 = fields[3].GetString();
            std::string params_1 = fields[4].GetString();

            DisableData data;
            data.flags = flags;

            switch (type)
            {
                case DISABLE_TYPE_GO_LOS:
                    if (!sObjectMgr->GetGameObjectTemplate(entry))
                    {
                        sLog->outErrorDb("Gameobject entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        continue;
                    }
                    if (flags)
                        sLog->outErrorDb("Disable flags specified for gameobject %u, useless data.", entry);
                    break;
                case DISABLE_TYPE_SPELL:
                    if (!(sSpellMgr->GetSpellInfo(entry) || flags & SPELL_DISABLE_DEPRECATED_SPELL))
                    {
                        sLog->outErrorDb("Spell entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        continue;
                    }

                    if (!flags || flags > MAX_SPELL_DISABLE_TYPE)
                    {
                        sLog->outErrorDb("Disable flags for spell %u are invalid, skipped.", entry);
                        continue;
                    }

                    if (flags & SPELL_DISABLE_MAP)
                    {
                        Tokenizer tokens(params_0, ',');
                        for (uint8 i = 0; i < tokens.size(); )
                            data.params[0].insert(atoi(tokens[i++]));
                    }

                    if (flags & SPELL_DISABLE_AREA)
                    {
                        Tokenizer tokens(params_1, ',');
                        for (uint8 i = 0; i < tokens.size(); )
                            data.params[1].insert(atoi(tokens[i++]));
                    }

                    // xinef: if spell has disabled los, add flag
                    if (flags & SPELL_DISABLE_LOS)
                    {
                        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(entry));
                        spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS;
                    }

                    break;
                // checked later
                case DISABLE_TYPE_QUEST:
                    break;
                case DISABLE_TYPE_MAP:
                case DISABLE_TYPE_LFG_MAP:
                    {
                        MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                        if (!mapEntry)
                        {
                            sLog->outErrorDb("Map entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                            continue;
                        }
                        bool isFlagInvalid = false;
                        switch (mapEntry->map_type)
                        {
                            case MAP_COMMON:
                                if (flags)
                                    isFlagInvalid = true;
                                break;
                            case MAP_INSTANCE:
                            case MAP_RAID:
                                if (flags & DUNGEON_STATUSFLAG_HEROIC && !GetMapDifficultyData(entry, DUNGEON_DIFFICULTY_HEROIC))
                                    isFlagInvalid = true;
                                else if (flags & RAID_STATUSFLAG_10MAN_HEROIC && !GetMapDifficultyData(entry, RAID_DIFFICULTY_10MAN_HEROIC))
                                    isFlagInvalid = true;
                                else if (flags & RAID_STATUSFLAG_25MAN_HEROIC && !GetMapDifficultyData(entry, RAID_DIFFICULTY_25MAN_HEROIC))
                                    isFlagInvalid = true;
                                break;
                            case MAP_BATTLEGROUND:
                            case MAP_ARENA:
                                sLog->outErrorDb("Battleground map %u specified to be disabled in map case, skipped.", entry);
                                continue;
                        }
                        if (isFlagInvalid)
                        {
                            sLog->outErrorDb("Disable flags for map %u are invalid, skipped.", entry);
                            continue;
                        }
                        break;
                    }
                case DISABLE_TYPE_BATTLEGROUND:
                    if (!sBattlemasterListStore.LookupEntry(entry))
                    {
                        sLog->outErrorDb("Battleground entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        continue;
                    }
                    if (flags)
                        sLog->outErrorDb("Disable flags specified for battleground %u, useless data.", entry);
                    break;
                case DISABLE_TYPE_OUTDOORPVP:
                    if (entry > MAX_OUTDOORPVP_TYPES)
                    {
                        sLog->outErrorDb("OutdoorPvPTypes value %u from `disables` is invalid, skipped.", entry);
                        continue;
                    }
                    if (flags)
                        sLog->outErrorDb("Disable flags specified for outdoor PvP %u, useless data.", entry);
                    break;
                case DISABLE_TYPE_ACHIEVEMENT_CRITERIA:
                    if (!sAchievementCriteriaStore.LookupEntry(entry))
                    {
                        sLog->outErrorDb("Achievement Criteria entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                        continue;
                    }
                    if (flags)
                        sLog->outErrorDb("Disable flags specified for Achievement Criteria %u, useless data.", entry);
                    break;
                case DISABLE_TYPE_VMAP:
                    {
                        MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                        if (!mapEntry)
                        {
                            sLog->outErrorDb("Map entry %u from `disables` doesn't exist in dbc, skipped.", entry);
                            continue;
                        }
                        switch (mapEntry->map_type)
                        {
                            case MAP_COMMON:
                                if (flags & VMAP_DISABLE_AREAFLAG)
                                    sLog->outString("Areaflag disabled for world map %u.", entry);
                                if (flags & VMAP_DISABLE_LIQUIDSTATUS)
                                    sLog->outString("Liquid status disabled for world map %u.", entry);
                                break;
                            case MAP_INSTANCE:
                            case MAP_RAID:
                                if (flags & VMAP_DISABLE_HEIGHT)
                                    sLog->outString("Height disabled for instance map %u.", entry);
                                if (flags & VMAP_DISABLE_LOS)
                                    sLog->outString("LoS disabled for instance map %u.", entry);
                                break;
                            case MAP_BATTLEGROUND:
                                if (flags & VMAP_DISABLE_HEIGHT)
                                    sLog->outString("Height disabled for battleground map %u.", entry);
                                if (flags & VMAP_DISABLE_LOS)
                                    sLog->outString("LoS disabled for battleground map %u.", entry);
                                break;
                            case MAP_ARENA:
                                if (flags & VMAP_DISABLE_HEIGHT)
                                    sLog->outString("Height disabled for arena map %u.", entry);
                                if (flags & VMAP_DISABLE_LOS)
                                    sLog->outString("LoS disabled for arena map %u.", entry);
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                default:
                    break;
            }

            m_DisableMap[type].insert(DisableTypeMap::value_type(entry, data));
            ++total_count;
        } while (result->NextRow());

        sLog->outString(">> Loaded %u disables in %u ms", total_count, GetMSTimeDiffToNow(oldMSTime));
        sLog->outString();
    }

    void CheckQuestDisables()
    {
        uint32 oldMSTime = getMSTime();

        uint32 count = m_DisableMap[DISABLE_TYPE_QUEST].size();
        if (!count)
        {
            sLog->outString(">> Checked 0 quest disables.");
            sLog->outString();
            return;
        }

        // check only quests, rest already done at startup
        for (DisableTypeMap::iterator itr = m_DisableMap[DISABLE_TYPE_QUEST].begin(); itr != m_DisableMap[DISABLE_TYPE_QUEST].end();)
        {
            const uint32 entry = itr->first;
            if (!sObjectMgr->GetQuestTemplate(entry))
            {
                sLog->outErrorDb("Quest entry %u from `disables` doesn't exist, skipped.", entry);
                m_DisableMap[DISABLE_TYPE_QUEST].erase(itr++);
                continue;
            }
            if (itr->second.flags)
                sLog->outErrorDb("Disable flags specified for quest %u, useless data.", entry);
            ++itr;
        }

        sLog->outString(">> Checked %u quest disables in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        sLog->outString();
    }

    bool IsDisabledFor(DisableType type, uint32 entry, Unit const* unit, uint8 flags)
    {
        ASSERT(type < MAX_DISABLE_TYPES);
        if (m_DisableMap[type].empty())
            return false;

        DisableTypeMap::iterator itr = m_DisableMap[type].find(entry);
        if (itr == m_DisableMap[type].end())    // not disabled
            return false;

        switch (type)
        {
            case DISABLE_TYPE_SPELL:
                {
                    uint8 spellFlags = itr->second.flags;
                    if (unit)
                    {
                        if ((spellFlags & SPELL_DISABLE_PLAYER && unit->GetTypeId() == TYPEID_PLAYER) ||
                                (unit->GetTypeId() == TYPEID_UNIT && ((unit->IsPet() && spellFlags & SPELL_DISABLE_PET) || spellFlags & SPELL_DISABLE_CREATURE)))
                        {
                            if (spellFlags & SPELL_DISABLE_MAP)
                            {
                                std::set<uint32> const& mapIds = itr->second.params[0];
                                if (mapIds.find(unit->GetMapId()) != mapIds.end())
                                    return true;                                        // Spell is disabled on current map

                                if (!(spellFlags & SPELL_DISABLE_AREA))
                                    return false;                                       // Spell is disabled on another map, but not this one, return false

                                // Spell is disabled in an area, but not explicitly our current mapId. Continue processing.
                            }

                            if (spellFlags & SPELL_DISABLE_AREA)
                            {
                                std::set<uint32> const& areaIds = itr->second.params[1];
                                if (areaIds.find(unit->GetAreaId()) != areaIds.end())
                                    return true;                                        // Spell is disabled in this area
                                return false;                                           // Spell is disabled in another area, but not this one, return false
                            }
                            else
                                return true;                                            // Spell disabled for all maps
                        }

                        return false;
                    }
                    else if (spellFlags & SPELL_DISABLE_DEPRECATED_SPELL)    // call not from spellcast
                        return true;

                    break;
                }
            case DISABLE_TYPE_MAP:
            case DISABLE_TYPE_LFG_MAP:
                if (Player const* player = unit->ToPlayer())
                {
                    MapEntry const* mapEntry = sMapStore.LookupEntry(entry);
                    if (mapEntry->IsDungeon())
                    {
                        uint8 disabledModes = itr->second.flags;
                        Difficulty targetDifficulty = player->GetDifficulty(mapEntry->IsRaid());
                        GetDownscaledMapDifficultyData(entry, targetDifficulty);
                        switch (targetDifficulty)
                        {
                            case DUNGEON_DIFFICULTY_NORMAL:
                                return disabledModes & DUNGEON_STATUSFLAG_NORMAL;
                            case DUNGEON_DIFFICULTY_HEROIC:
                                return disabledModes & DUNGEON_STATUSFLAG_HEROIC;
                            case RAID_DIFFICULTY_10MAN_HEROIC:
                                return disabledModes & RAID_STATUSFLAG_10MAN_HEROIC;
                            case RAID_DIFFICULTY_25MAN_HEROIC:
                                return disabledModes & RAID_STATUSFLAG_25MAN_HEROIC;
                        }
                    }
                    else if (mapEntry->map_type == MAP_COMMON)
                        return true;
                }
                return false;
            case DISABLE_TYPE_QUEST:
                return true;
            case DISABLE_TYPE_BATTLEGROUND:
            case DISABLE_TYPE_OUTDOORPVP:
            case DISABLE_TYPE_ACHIEVEMENT_CRITERIA:
                return true;
            case DISABLE_TYPE_VMAP:
                return flags & itr->second.flags;
            case DISABLE_TYPE_GO_LOS:
                return true;
        }

        return false;
    }

} // Namespace
