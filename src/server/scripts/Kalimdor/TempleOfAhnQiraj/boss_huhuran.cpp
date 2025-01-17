/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

/* ScriptData
SDName: Boss_Huhuran
SD%Complete: 100
SDComment:
SDCategory: Temple of Ahn'Qiraj
EndScriptData */

#include "ScriptedCreature.h"
#include "ScriptMgr.h"

enum Huhuran
{
    EMOTE_FRENZY_KILL           = 0,
    EMOTE_BERSERK               = 1,

    SPELL_FRENZY                = 26051,
    SPELL_BERSERK               = 26068,
    SPELL_POISONBOLT            = 26052,
    SPELL_NOXIOUSPOISON         = 26053,
    SPELL_WYVERNSTING           = 26180,
    SPELL_ACIDSPIT              = 26050
};

class boss_huhuran : public CreatureScript
{
public:
    boss_huhuran() : CreatureScript("boss_huhuran") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_huhuranAI(creature);
    }

    struct boss_huhuranAI : public ScriptedAI
    {
        boss_huhuranAI(Creature* creature) : ScriptedAI(creature) { }

        uint32 Frenzy_Timer;
        uint32 Wyvern_Timer;
        uint32 Spit_Timer;
        uint32 PoisonBolt_Timer;
        uint32 NoxiousPoison_Timer;
        uint32 FrenzyBack_Timer;

        bool Frenzy;
        bool Berserk;

        void Reset() override
        {
            Frenzy_Timer = urand(25000, 35000);
            Wyvern_Timer = urand(18000, 28000);
            Spit_Timer = 8000;
            PoisonBolt_Timer = 4000;
            NoxiousPoison_Timer = urand(10000, 20000);
            FrenzyBack_Timer = 15000;

            Frenzy = false;
            Berserk = false;
        }

        void EnterCombat(Unit* /*who*/) override
        {
        }

        void UpdateAI(uint32 diff) override
        {
            //Return since we have no target
            if (!UpdateVictim())
                return;

            //Frenzy_Timer
            if (!Frenzy && Frenzy_Timer <= diff)
            {
                DoCast(me, SPELL_FRENZY);
                Talk(EMOTE_FRENZY_KILL);
                Frenzy = true;
                PoisonBolt_Timer = 3000;
                Frenzy_Timer = urand(25000, 35000);
            }
            else Frenzy_Timer -= diff;

            // Wyvern Timer
            if (Wyvern_Timer <= diff)
            {
                if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                    DoCast(target, SPELL_WYVERNSTING);
                Wyvern_Timer = urand(15000, 32000);
            }
            else Wyvern_Timer -= diff;

            //Spit Timer
            if (Spit_Timer <= diff)
            {
                DoCastVictim(SPELL_ACIDSPIT);
                Spit_Timer = urand(5000, 10000);
            }
            else Spit_Timer -= diff;

            //NoxiousPoison_Timer
            if (NoxiousPoison_Timer <= diff)
            {
                DoCastVictim(SPELL_NOXIOUSPOISON);
                NoxiousPoison_Timer = urand(12000, 24000);
            }
            else NoxiousPoison_Timer -= diff;

            //PoisonBolt only if frenzy or berserk
            if (Frenzy || Berserk)
            {
                if (PoisonBolt_Timer <= diff)
                {
                    DoCastVictim(SPELL_POISONBOLT);
                    PoisonBolt_Timer = 3000;
                }
                else PoisonBolt_Timer -= diff;
            }

            //FrenzyBack_Timer
            if (Frenzy && FrenzyBack_Timer <= diff)
            {
                me->InterruptNonMeleeSpells(false);
                Frenzy = false;
                FrenzyBack_Timer = 15000;
            }
            else FrenzyBack_Timer -= diff;

            if (!Berserk && HealthBelowPct(31))
            {
                me->InterruptNonMeleeSpells(false);
                Talk(EMOTE_BERSERK);
                DoCast(me, SPELL_BERSERK);
                Berserk = true;
            }

            DoMeleeAttackIfReady();
        }
    };
};

void AddSC_boss_huhuran()
{
    new boss_huhuran();
}
