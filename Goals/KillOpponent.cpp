#include "KillOpponent.h"
#include "../Strategies/Bait.h"
#include "../Strategies/Sandbag.h"

KillOpponent::KillOpponent()
{
    m_strategy = NULL;
    m_lastAction = (ACTION)m_state->m_memory->player_one_action;
    m_lastActionSelf = (ACTION)m_state->m_memory->player_two_action;
    m_lastActionFrame = 0;
    m_lastActionFrameSelf = 0;
    m_moonwalkStepA = false;
}

KillOpponent::~KillOpponent()
{
    delete m_strategy;
}

void KillOpponent::Strategize()
{
    //Melee is inconsistent and some actions are indexed from zero instead of one
    // This throws all our math off, so let's fix that problem
    if(m_state->isIndexedFromZero((ACTION)m_state->m_memory->player_one_action))
    {
        m_state->m_memory->player_one_action_frame++;
    }
    if(m_state->isIndexedFromZero((ACTION)m_state->m_memory->player_two_action))
    {
        m_state->m_memory->player_two_action_frame++;
    }

    //XXX: Uncomment this to test what frames actions are indexed from
    // if(m_state->m_memory->player_one_action_frame == 0)
    // {
    //     std::cout << std::hex << "Add it to the list: 0x" << m_state->m_memory->player_one_action << std::endl;
    // }
    // if(m_state->m_memory->player_two_action_frame == 0)
    // {
    //     std::cout << std::hex << "Add it to the list: 0x" << m_state->m_memory->player_two_action << std::endl;
    // }

    //Moonwalk early detection
    //If the control stick went through neutral, then reset the moonwalk state bools
    if(Controller::Instance()->m_prevFrameState.m_main_stick_x == 0.5)
    {
        m_moonwalkStepA = false;
    }

    bool wasRunning = m_lastActionSelf == DASHING ||
        m_lastActionSelf == RUNNING  ||
        m_lastActionSelf == TURNING;

    bool isRunning = m_state->m_memory->player_two_action == DASHING ||
        m_state->m_memory->player_two_action == RUNNING;

    bool controlStickSmashed = Controller::Instance()->m_prevFrameState.m_main_stick_x == 0 ||
        Controller::Instance()->m_prevFrameState.m_main_stick_x == 1;

    //Step B: Be in a running state with the control stick to one side
    if(m_moonwalkStepA && isRunning && controlStickSmashed)
    {
        m_state->m_moonwalkRisk = true;
    }
    else
    {
        m_state->m_moonwalkRisk = false;
    }

    //Step A: Be in a non-running state
    if(!wasRunning)
    {
        m_moonwalkStepA = true;
    }

    if(m_state->m_memory->player_one_action == WAVEDASH_SLIDE)
    {
        m_lastActionFrame++;
        m_state->m_memory->player_one_action_frame = m_lastActionFrame;
    }
    if(m_state->m_memory->player_two_action == WAVEDASH_SLIDE)
    {
        m_lastActionFrameSelf++;
        m_state->m_memory->player_two_action_frame = m_lastActionFrameSelf;
    }

    // Unfortunately, the game reuses LANDING_SPECIAL for both landing from an UP-B and from a wavedash
    // So we figure out if it's the wavedash version and silently make a "new" action state for it
    if(m_state->m_memory->player_one_action == LANDING_SPECIAL)
    {
        if(m_lastAction != DEAD_FALL &&
            m_lastAction != UP_B)
        {
            m_state->m_memory->player_one_action = WAVEDASH_SLIDE;
            m_lastActionFrame = 1;
        }
    }
    if(m_state->m_memory->player_two_action == LANDING_SPECIAL)
    {
        if(m_lastActionSelf != DEAD_FALL &&
            m_lastActionSelf != UP_B)
        {
            m_state->m_memory->player_one_action = WAVEDASH_SLIDE;
            m_lastActionFrameSelf = 1;
        }
    }

    //So, turns out that the game changes the player's action state (to 2 or 3) on us when they're charging
    //If this happens, just change it back. Maybe there's a more elegant solution
    if(m_state->m_memory->player_one_action == 0x00 ||
        m_state->m_memory->player_one_action == 0x02 ||
        m_state->m_memory->player_one_action == 0x03)
    {
        m_state->m_memory->player_one_action = m_lastAction;
    }
    //Sometimes, it will also happen when invincible, turning to ENTRY
    if(m_state->m_memory->player_one_charging_smash &&
        m_state->m_memory->player_one_action == ENTRY)
    {
        m_state->m_memory->player_one_action = m_lastAction;
    }

    //If the opponent just started a roll, remember where they started from
    if(m_state->isRollingState((ACTION)m_state->m_memory->player_one_action) &&
        m_state->m_memory->player_one_action_frame <= 1)
    {
        m_state->m_rollStartPosition = m_state->m_memory->player_one_x;
        m_state->m_rollStartSpeed = m_state->m_memory->player_one_speed_x_attack;
        m_state->m_rollStartSpeedSelf = m_state->m_memory->player_one_speed_ground_x_self;
    }

    if(m_state->m_memory->player_one_action == EDGE_CATCHING &&
        m_state->m_memory->player_one_action_frame == 1)
    {
        m_state->m_edgeInvincibilityStart = m_state->m_memory->frame;
    }

    m_lastAction = (ACTION)m_state->m_memory->player_one_action;
    m_lastActionSelf = (ACTION)m_state->m_memory->player_two_action;

    //If the opponent is invincible, don't attack them. Just dodge everything they do
    //UNLESS they are invincible due to rolling on the stage. Then go ahead and punish it, it will be safe by the time
    //  they are back up.
    if(m_state->m_memory->player_one_invulnerable &&
        m_state->m_memory->player_one_action != EDGE_GETUP_SLOW &&
        m_state->m_memory->player_one_action != EDGE_GETUP_QUICK &&
        m_state->m_memory->player_one_action != EDGE_ROLL_SLOW &&
        m_state->m_memory->player_one_action != EDGE_ROLL_QUICK)
    {
        CreateStrategy(Sandbag);
        m_strategy->DetermineTactic();
    }
    else
    {
        CreateStrategy(Bait);
        m_strategy->DetermineTactic();
    }

}
