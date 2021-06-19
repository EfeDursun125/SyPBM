//
// Copyright (c) 2003-2009, by Yet Another POD-Bot Development Team.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.sypb_aim_spring_stiffness_y
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// $Id:$
//

#include <core.h>

ConVar sypb_dangerfactor_min("sypbm_dangerfactor_min", "600");
ConVar sypb_dangerfactor_max("sypbm_dangerfactor_max", "1000");
ConVar sypbm_aimbot("sypbm_aimbot", "0");
ConVar sypbm_max_angular_vel("sypbm_max_angular_vel", "1024");
ConVar sypbm_aiming_type("sypbm_aiming_type", "1");

extern ConVar sypbm_anti_block;

int Bot::FindGoal(void)
{
	if (m_waypointGoalAPI != -1)
		return m_chosenGoalIndex = m_waypointGoalAPI;

	// SyPBM 1.53 - we will hunt enemy or walk randomly
	if (IsDeathmatchMode() || (GetGameMod() == MODE_BASE && m_personality == PERSONALITY_RUSHER && !m_isReloading && !g_bombPlanted && !m_isVIP))
	{
		if (m_currentWaypointIndex <= -1 || m_currentWaypointIndex > g_numWaypoints)
			GetValidWaypoint();

		if (!FNullEnt(m_lastEnemy) && IsAlive(m_lastEnemy))
			return m_chosenGoalIndex = g_waypoint->FindNearest(GetEntityOrigin(m_lastEnemy));

		return m_chosenGoalIndex = engine->RandomInt(0, g_numWaypoints - 1);
	}

	// chooses a destination (goal) waypoint for a bot
	// path finding behavior depending on map type
	int tactic;
	int offensive;
	int defensive;
	int goalDesire;

	int forwardDesire;
	int campDesire;
	int backoffDesire;
	int tacticChoice;

	Array <int> offensiveWpts;
	Array <int> defensiveWpts;

	if (GetGameMod() == MODE_BASE)
	{
		switch (m_team)
		{
		case TEAM_TERRORIST:
			offensiveWpts = g_waypoint->m_ctPoints;
			defensiveWpts = g_waypoint->m_terrorPoints;
			break;

		case TEAM_COUNTER:
			offensiveWpts = g_waypoint->m_terrorPoints;
			defensiveWpts = g_waypoint->m_ctPoints;
			break;
		}
	}
	else if (GetGameMod() == MODE_ZP)
	{
		if (!m_isZombieBot && !g_waypoint->m_zmHmPoints.IsEmpty())
			offensiveWpts = g_waypoint->m_zmHmPoints;
		else if (m_isZombieBot && FNullEnt(m_moveTargetEntity) && FNullEnt(m_enemy))
		{
			int checkPoint[3];
			for (int i = 0; i < 3; i++)
				checkPoint[i] = engine->RandomInt(0, g_numWaypoints - 1);

			int movePoint = checkPoint[0];
			if (engine->RandomInt(1, 5) <= 2)
			{
				int maxEnemyNum = 0;
				for (int i = 0; i < 3; i++)
				{
					int enemyNum = GetNearbyEnemiesNearPosition(g_waypoint->GetPath(checkPoint[i])->origin, 300);
					if (enemyNum > maxEnemyNum)
					{
						maxEnemyNum = enemyNum;
						movePoint = checkPoint[i];
					}
				}
			}
			else
			{
				int playerWpIndex = GetEntityWaypoint(GetEntity());
				float maxDistance = 0.0f;
				for (int i = 0; i < 3; i++)
				{
					float distance = g_waypoint->GetPathDistanceFloat(playerWpIndex, checkPoint[i]);
					if (distance >= maxDistance)
					{
						maxDistance = distance;
						movePoint = checkPoint[i];
					}
				}
			}

			return m_chosenGoalIndex = movePoint;
		}
		else
			return m_chosenGoalIndex = engine->RandomInt(0, g_numWaypoints - 1);
	}
	else
		return m_chosenGoalIndex = engine->RandomInt(0, g_numWaypoints - 1);

	// terrorist carrying the C4?
	if (pev->weapons & (1 << WEAPON_C4) || m_isVIP)
	{
		tactic = 3;
		goto TacticChoosen;
	}
	else if (HasHostage() && m_team == TEAM_COUNTER)
	{
		tactic = 2;
		offensiveWpts = g_waypoint->m_rescuePoints;

		goto TacticChoosen;
	}

	offensive = static_cast <int> (m_agressionLevel * 100);
	defensive = static_cast <int> (m_fearLevel * 100);

	if (GetGameMod() == MODE_BASE)
	{
		if (g_mapType & (MAP_AS | MAP_CS))
		{
			if (m_team == TEAM_TERRORIST)
			{
				defensive += 30;
				offensive -= 30;
			}
			else if (m_team == TEAM_COUNTER)
			{
				defensive -= 30;
				offensive += 30;
			}
		}
		else if ((g_mapType & MAP_DE))
		{
			if (m_team == TEAM_COUNTER)
			{
				if (g_bombPlanted && GetCurrentTask()->taskID != TASK_ESCAPEFROMBOMB && g_waypoint->GetBombPosition() != nullvec)
				{
					const Vector& bombPos = g_waypoint->GetBombPosition();

					if (GetBombTimeleft() >= 10.0f && IsBombDefusing(bombPos))
						return m_chosenGoalIndex = FindDefendWaypoint(bombPos);

					if (g_bombSayString)
					{
						ChatMessage(CHAT_PLANTBOMB);
						g_bombSayString = false;
					}
					return m_chosenGoalIndex = ChooseBombWaypoint();
				}
				defensive += 30;
				offensive -= 30;
			}
			else if (m_team == TEAM_TERRORIST)
			{
				defensive -= 30;
				offensive += 30;

				// send some terrorists to guard planter bomb
				if (g_bombPlanted && GetCurrentTask()->taskID != TASK_ESCAPEFROMBOMB && GetBombTimeleft() >= 15.0f)
					return m_chosenGoalIndex = FindDefendWaypoint(g_waypoint->GetBombPosition());

				float leastPathDistance = 0.0f;
				int goalIndex = -1;

				ITERATE_ARRAY(g_waypoint->m_goalPoints, i)
				{
					float realPathDistance = g_waypoint->GetPathDistanceFloat(m_currentWaypointIndex, g_waypoint->m_goalPoints[i]) + engine->RandomFloat(0.0, 128.0f);

					if (leastPathDistance > realPathDistance)
					{
						goalIndex = g_waypoint->m_goalPoints[i];
						leastPathDistance = realPathDistance;
					}
				}

				if (goalIndex != -1 && !g_bombPlanted && (pev->weapons & (1 << WEAPON_C4)))
					return m_chosenGoalIndex = goalIndex;
			}
		}
	}
	else if (IsZombieMode())
	{
		if (m_isZombieBot)
		{
			defensive += 30;
			offensive -= 30;
		}
		else
		{
			defensive -= 30;
			offensive += 30;

			// SyPB Pro P.30 - Zombie Mode Human Camp
			if (!g_waypoint->m_zmHmPoints.IsEmpty())
			{
				tactic = 4;
				goto TacticChoosen;
			}
		}
	}

	goalDesire = engine->RandomInt(0, 70) + offensive;
	forwardDesire = engine->RandomInt(0, 50) + offensive;
	campDesire = engine->RandomInt(0, 70) + defensive;
	backoffDesire = engine->RandomInt(0, 50) + defensive;

	if (UsesSniper() || ((g_mapType & MAP_DE) && m_team == TEAM_COUNTER && !g_bombPlanted) && (GetGameMod() == MODE_BASE || IsDeathmatchMode()))
		campDesire = static_cast <int> (engine->RandomFloat(1.5f, 2.5f) * static_cast <float> (campDesire));

	tacticChoice = backoffDesire;
	tactic = 0;

	if (campDesire > tacticChoice)
	{
		tacticChoice = campDesire;
		tactic = 1;
	}
	if (forwardDesire > tacticChoice)
	{
		tacticChoice = forwardDesire;
		tactic = 2;
	}
	if (goalDesire > tacticChoice)
		tactic = 3;

TacticChoosen:
	int goalChoices[4] = { -1, -1, -1, -1 };

	int playerWpIndex;

	
	if (tactic == 0 && !defensiveWpts.IsEmpty()) // careful goal
		FilterGoals(defensiveWpts, goalChoices);
	else if (tactic == 1 && !g_waypoint->m_campPoints.IsEmpty()) // camp waypoint goal
	{
		// pickup sniper points if possible for sniping bots
		if (!g_waypoint->m_sniperPoints.IsEmpty() && UsesSniper())
			FilterGoals(g_waypoint->m_sniperPoints, goalChoices);
		else
			FilterGoals(g_waypoint->m_campPoints, goalChoices);
	}
	else if (tactic == 2 && !offensiveWpts.IsEmpty()) // offensive goal
		FilterGoals(offensiveWpts, goalChoices);
	else if (tactic == 3 && !g_waypoint->m_goalPoints.IsEmpty()) // map goal waypoint
	{
		// force bomber to select closest goal, if round-start goal was reset by something
		if (pev->weapons & (1 << WEAPON_C4) && g_timeRoundStart + 10.0f < engine->GetTime())
		{
			float minDist = 99999.0f;
			int count = 0;

			ITERATE_ARRAY(g_waypoint->m_goalPoints, i)
			{
				Path* path = g_waypoint->GetPath(g_waypoint->m_goalPoints[i]);

				float distance = (path->origin - pev->origin).GetLength();

				if (distance > 1024.0f)
					continue;

				if (distance < minDist)
				{
					goalChoices[count] = i;

					if (++count > 3)
						count = 0;

					minDist = distance;
				}
			}

			for (int i = 0; i < 4; i++)
			{
				if (goalChoices[i] == -1)
				{
					goalChoices[i] = g_waypoint->m_goalPoints.GetRandomElement();
					InternalAssert(goalChoices[i] >= 0 && goalChoices[i] < g_numWaypoints);
				}
			}
		}
		else
			FilterGoals(g_waypoint->m_goalPoints, goalChoices);
	}
	// SyPB Pro P.30 - Zombie Mode Human Camp
	else if (tactic == 4 && !offensiveWpts.IsEmpty()) // offensive goal
	{
		playerWpIndex = GetEntityWaypoint(GetEntity());

		int targetWpIndex = -1;

		int wpIndex = offensiveWpts.GetRandomElement();
		if (wpIndex >= 0 && wpIndex < g_numWaypoints)
			targetWpIndex = wpIndex;

		if (targetWpIndex >= 0 && targetWpIndex < g_numWaypoints)
			return m_chosenGoalIndex = targetWpIndex;
	}

	// SyPB Pro P.40 - Small Change
	if (m_currentWaypointIndex <= -1 || m_currentWaypointIndex > g_numWaypoints)
		GetValidWaypoint();

	if (goalChoices[0] == -1)
	{
		if (GetGameMod() == MODE_BASE && ChanceOf(50))
		{
			if (!g_waypoint->m_goalPoints.IsEmpty() && ChanceOf(50)) // we can check goal waypoints
				return m_chosenGoalIndex = g_waypoint->m_goalPoints.GetRandomElement();

			if (!g_waypoint->m_rescuePoints.IsEmpty() && ChanceOf(50)) // we can check rescure waypoints
				return m_chosenGoalIndex = g_waypoint->m_rescuePoints.GetRandomElement();

			if (!g_waypoint->m_campPoints.IsEmpty() && ChanceOf(25)) // we can check camp waypoints
				return m_chosenGoalIndex = g_waypoint->m_campPoints.GetRandomElement();
		}

		return m_chosenGoalIndex = engine->RandomInt(0, g_numWaypoints - 1); // we must walk randomly, we don't know how to play in this map, we will poorly performance :(
	}

	bool isSorting = false;

	do
	{
		isSorting = false;

		for (int i = 0; i < 3; i++)
		{
			if (goalChoices[i + 1] < 0)
				break;

			if (g_exp.GetValue(m_currentWaypointIndex, goalChoices[i], m_team) < g_exp.GetValue(m_currentWaypointIndex, goalChoices[i + 1], m_team))
			{
				goalChoices[i + 1] = goalChoices[i];
				goalChoices[i] = goalChoices[i + 1];

				isSorting = true;
			}

		}
	} while (isSorting);

	// return and store goal
	return m_chosenGoalIndex = goalChoices[0];
}

void Bot::FilterGoals(const Array <int>& goals, int* result)
{
	// this function filters the goals, so new goal is not bot's old goal, and array of goals doesn't contains duplicate goals

	int searchCount = 0;

	for (int index = 0; index < 4; index++)
	{
		int rand = goals.GetRandomElement();

		if (searchCount <= 8 && (m_prevGoalIndex == rand || ((result[0] == rand || result[1] == rand || result[2] == rand || result[3] == rand) && goals.GetElementNumber() > 4)) && !IsWaypointOccupied(rand))
		{
			if (index > 0)
				index--;

			searchCount++;
			continue;
		}
		result[index] = rand;
	}
}

bool Bot::GoalIsValid(void)
{
	int goal = GetCurrentTask()->data;

	if (goal == -1) // not decided about a goal
		return false;
	else if (goal == m_currentWaypointIndex) // no nodes needed
		return true;
	else if (m_navNode == null) // no path calculated
		return false;

	// got path - check if still valid
	PathNode* node = m_navNode;

	while (node->next != null)
		node = node->next;

	if (node->index == goal)
		return true;

	return false;
}

bool Bot::DoWaypointNav(void)
{
	// this function is a main path navigation

	// check if we need to find a waypoint...
	if (m_currentWaypointIndex <= -1 || m_currentWaypointIndex > g_numWaypoints)
	{
		GetValidWaypoint();

		m_navTimeset = engine->GetTime();
	}

	if (pev->flags & FL_DUCKING)
		m_destOrigin = m_waypointOrigin;
	else
		m_destOrigin = m_waypointOrigin + pev->view_ofs;

	float waypointDistance = (pev->origin - m_waypointOrigin).GetLength();

	// this waypoint has additional travel flags - care about them
	if (m_currentTravelFlags & PATHFLAG_JUMP)
	{
		// bot is not jumped yet?
		if (!m_jumpFinished)
		{
			// if bot's on the ground or on the ladder we're free to jump. actually setting the correct velocity is cheating.
			// pressing the jump button gives the illusion of the bot actual jmping.
			if (IsOnFloor() || IsOnLadder())
			{
				if (m_desiredVelocity == nullvec || m_desiredVelocity == 0 || m_desiredVelocity == -1)
					pev->velocity = ((pev->velocity * (pev->origin - m_waypointOrigin).GetLength()) * 1.2);
				else
					pev->velocity = m_desiredVelocity;

				pev->button |= IN_JUMP;

				m_jumpFinished = true;
				m_checkTerrain = false;
				m_desiredVelocity = nullvec;
			}
		}
		else if (!sypb_knifemode.GetBool() && m_currentWeapon == WEAPON_KNIFE && IsOnFloor())
			SelectBestWeapon();
	}

	if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LADDER)
	{
		if (m_waypointOrigin.z >= (pev->origin.z + 16.0f))
			m_waypointOrigin = g_waypoint->GetPath(m_currentWaypointIndex)->origin + Vector(0.0f, 0.0f, 16.0f);
		else if (m_waypointOrigin.z < pev->origin.z + 16.0f && !IsOnLadder() && IsOnFloor() && !(pev->flags & FL_DUCKING))
		{
			// SyPB Pro P.45 - Move Speed improve
			// SyPB Pro P.46 - Ladder Move Speed improve

			m_moveSpeed = waypointDistance;

			if (m_moveSpeed > pev->maxspeed)
				m_moveSpeed = pev->maxspeed;
			else
			{
				if (m_moveSpeed <= GetWalkSpeed())
					m_moveSpeed = GetWalkSpeed();
				else if (m_moveSpeed < 150.0f)
					m_moveSpeed = 150.0f;
			}
		}
	}
	else if (pev->flags & FL_DUCKING)
		m_moveSpeed = pev->maxspeed; // fast crouch move

	// SyPB Pro P.45 - Waypoint Door improve
	bool haveDoorEntity = false;
	edict_t* door_entity = null;
	while (!FNullEnt(door_entity = FIND_ENTITY_IN_SPHERE(door_entity, pev->origin, 150.0f)))
	{
		if (strncmp(STRING(door_entity->v.classname), "func_door", 9) == 0)
		{
			haveDoorEntity = true;
			break;
		}
	}

	if (haveDoorEntity)
	{
		TraceResult tr;
		TraceLine(pev->origin, m_waypointOrigin, true, GetEntity(), &tr);

		if (!FNullEnt(tr.pHit))
		{
			// SyPB Pro P.42 - Shit Bugs Fixed.....
			if (strncmp(STRING(tr.pHit->v.classname), "func_door", 9) == 0)
			{
				m_lastCollTime = engine->GetTime() + 0.5f;

				edict_t* button = FindNearestButton(STRING(tr.pHit->v.classname));

				if (!FNullEnt(button))
				{
					m_pickupItem = button;
					m_pickupType = PICKTYPE_BUTTON;

					m_navTimeset = engine->GetTime();
				}

				if (m_timeDoorOpen < engine->GetTime())
				{
					m_doorOpenAttempt++;
					m_timeDoorOpen = engine->GetTime() + 1.0f; // retry in 1 sec until door is open

					if (m_doorOpenAttempt >= 5)
						m_doorOpenAttempt = 0;
					else if (m_doorOpenAttempt >= 2)
					{
						DeleteSearchNodes();
						ResetTasks();
					}
					else
						PushTask(TASK_PAUSE, TASKPRI_PAUSE, -1, engine->GetTime() + 1, false);
				}
			}
			else
				m_doorOpenAttempt = 0;
		}
	}
	else
		m_doorOpenAttempt = 0;

	float desiredDistance = 0.0f;

	// initialize the radius for a special waypoint type, where the wpt is considered to be reached
	if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LIFT)
		desiredDistance = 50.0f;
	else if ((pev->flags & FL_DUCKING) || (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_GOAL))
		desiredDistance = 25.0f;
	else if (g_waypoint->GetPath(m_currentWaypointIndex)->flags & WAYPOINT_LADDER)
		desiredDistance = 15.0f;
	else if (m_currentTravelFlags & PATHFLAG_JUMP)
		desiredDistance = 0.0f;
	else
		desiredDistance = g_waypoint->GetPath(m_currentWaypointIndex)->radius;

	// check if waypoint has a special travelflag, so they need to be reached more precisely
	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		if (g_waypoint->GetPath(m_currentWaypointIndex)->connectionFlags[i] != 0)
		{
			desiredDistance = 0;
			break;
		}
	}

	// needs precise placement - check if we get past the point
	if (desiredDistance < 16.0f && waypointDistance < 30.0f)
	{
		Vector nextFrameOrigin = pev->origin + (pev->velocity * m_frameInterval);

		if ((nextFrameOrigin - m_waypointOrigin).GetLength() >= waypointDistance)
			desiredDistance = waypointDistance + 1.0f;
	}

	// SyPB Pro P.42 - AMXX API
	if (m_waypointGoalAPI != -1 && m_currentWaypointIndex == m_waypointGoalAPI)
		m_waypointGoalAPI = -1;

	if ((pev->origin - m_waypointOrigin).GetLength() < desiredDistance)
	{
		// Did we reach a destination Waypoint?
		if (GetCurrentTask()->data == m_currentWaypointIndex)
		{
			// add goal values
			if (m_chosenGoalIndex != -1)
			{
				g_exp.CollectValue(m_chosenGoalIndex, m_currentWaypointIndex, static_cast <int> (pev->health), m_goalValue);
			}

			return true;
		}
		else if (m_navNode == null)
			return false;

		if ((g_mapType & MAP_DE) && g_bombPlanted && m_team == TEAM_COUNTER && GetCurrentTask()->taskID != TASK_ESCAPEFROMBOMB && m_tasks->data != -1)
		{
			Vector bombOrigin = CheckBombAudible();

			// bot within 'hearable' bomb tick noises?
			if (bombOrigin != nullvec)
			{
				float distance = (bombOrigin - g_waypoint->GetPath(m_tasks->data)->origin).GetLength();

				if (distance > 512.0f)
					g_waypoint->SetGoalVisited(m_tasks->data); // doesn't hear so not a good goal
			}
			else
				g_waypoint->SetGoalVisited(m_tasks->data); // doesn't hear so not a good goal
		}

		HeadTowardWaypoint(); // do the actual movement checking

		return false;
	}

	// SyPB Pro P.43 - Waypoint improve
	if ((m_waypointOrigin - pev->origin).GetLength2D() <= 4.0f && m_waypointOrigin.z <= pev->origin.z + 32.0f)
	{
		if (m_navNode == null || (m_navNode->next != null && g_waypoint->Reachable(GetEntity(), m_navNode->next->index)))
			HeadTowardWaypoint();
	}

	return false;
}

void Bot::FindPathPOD(int srcIndex, int destIndex)
{
	// this function finds the shortest path from source index to destination index

	if (m_pathtimer + 1.0f > engine->GetTime())
		return;

	m_pathtimer = engine->GetTime();

	if (m_isStuck)
		DeleteSearchNodes();

	if (!IsAlive(GetEntity()))
		DeleteSearchNodes();

	if (!IsOnFloor())
		DeleteSearchNodes();

	m_goalValue = 0.0f;
	m_chosenGoalIndex = srcIndex;

	PathNode* node = new PathNode;

	node->index = srcIndex;
	node->next = null;

	m_navNodeStart = node;
	m_navNode = m_navNodeStart;

	while (srcIndex != destIndex)
	{
		srcIndex = *(g_waypoint->m_pathMatrix + (srcIndex * g_numWaypoints) + destIndex);
		if (srcIndex < 0)
		{
			m_prevGoalIndex = -1;
			GetCurrentTask()->data = -1;

			return;
		}

		node->next = new PathNode;
		node = node->next;

		if (node == null)
			TerminateOnMalloc();

		node->index = srcIndex;
		node->next = null;
	}
}


// Priority queue class (smallest item out first)
class PriorityQueue
{
public:
	PriorityQueue(void);
	~PriorityQueue(void);

	inline int  Empty(void) { return m_size == 0; }
	inline int  Size(void) { return m_size; }
	void        Insert(int, float);
	int         Remove(void);

private:
	struct HeapNode_t
	{
		int   id;
		float priority;
	} *m_heap;

	int         m_size;
	int         m_heapSize;

	void        HeapSiftDown(int);
	void        HeapSiftUp(void);
};

PriorityQueue::PriorityQueue(void)
{
	m_size = 0;
	m_heapSize = Const_MaxWaypoints * 4;
	m_heap = new HeapNode_t[m_heapSize];

	if (m_heap == null)
		TerminateOnMalloc();
}

PriorityQueue::~PriorityQueue(void)
{
	if (m_heap != null)
		delete[] m_heap;

	m_heap = null;
}

// inserts a value into the priority queue
void PriorityQueue::Insert(int value, float priority)
{
	if (m_size >= m_heapSize)
	{
		m_heapSize += 100;
		m_heap = (HeapNode_t*)realloc(m_heap, sizeof(HeapNode_t) * m_heapSize);

		if (m_heap == null)
			TerminateOnMalloc();
	}

	m_heap[m_size].priority = priority;
	m_heap[m_size].id = value;

	m_size++;
	HeapSiftUp();
}

// removes the smallest item from the priority queue
int PriorityQueue::Remove(void)
{
	int retID = m_heap[0].id;

	m_size--;
	m_heap[0] = m_heap[m_size];

	HeapSiftDown(0);
	return retID;
}

void PriorityQueue::HeapSiftDown(int subRoot)
{
	int parent = subRoot;
	int child = (2 * parent) + 1;

	HeapNode_t ref = m_heap[parent];

	while (child < m_size)
	{
		int rightChild = (2 * parent) + 2;

		if (rightChild < m_size)
		{
			if (m_heap[rightChild].priority < m_heap[child].priority)
				child = rightChild;
		}
		if (ref.priority <= m_heap[child].priority)
			break;

		m_heap[parent] = m_heap[child];

		parent = child;
		child = (2 * parent) + 1;
	}
	m_heap[parent] = ref;
}


void PriorityQueue::HeapSiftUp(void)
{
	int child = m_size - 1;

	while (child)
	{
		int parent = (child - 1) / 2;

		if (m_heap[parent].priority <= m_heap[child].priority)
			break;

		HeapNode_t temp = m_heap[child];

		m_heap[child] = m_heap[parent];
		m_heap[parent] = temp;

		child = parent;
	}
}

inline const float GF_CostNormal(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 1.0f)
			return 65355.0f;
	}

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost += g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_LADDER)
		baseCost += pathDist * 1.5f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (engine->RandomFloat(sypb_dangerfactor_min.GetFloat(), sypb_dangerfactor_max.GetFloat()) * 2.0f / offset));
}

inline const float GF_CostDistNormal(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 1.0f)
			return 65355.0f;
	}

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, true);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost += g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_LADDER)
		baseCost += pathDist * 1.5f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (engine->RandomFloat(sypb_dangerfactor_min.GetFloat(), sypb_dangerfactor_max.GetFloat()) * 2.0f / offset));
}

inline const float GF_CostRusher(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 1.0f)
			return 65355.0f;
	}

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH)
		baseCost += pathDist * 2.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_min.GetFloat() * 2.0f / offset));
}

inline const float GF_CostDistRusher(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 1.0f)
			return 65355.0f;
	}

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, true);

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH)
		baseCost += pathDist * 2.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_min.GetFloat() * 2.0f / offset));
}

inline const float GF_CostCareful(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
		return 65355.0f;

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost *= g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_max.GetFloat() * 2.0f / offset));
}

inline const float GF_CostDistCareful(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 1.0f)
			return 65355.0f;
	}

	if (team == TEAM_COUNTER ? g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST : g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, true);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost *= g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_max.GetFloat() * 2.0f / offset));
}

inline const float GF_CostNoHostage(int index, int parent, int team, float offset)
{
	Path* path = g_waypoint->GetPath(index);

	if (path->flags & WAYPOINT_CROUCH)
		return 65355.0f;

	if (path->flags & WAYPOINT_LADDER)
		return 65355.0f;

	if (path->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	if (path->flags & WAYPOINT_JUMP)
		return 65355.0f;

	if (path->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (path->flags & WAYPOINT_FALLCHECK)
		return 65355.0f;

	if (path->connectionFlags[index] & PATHFLAG_JUMP)
		return 65355.0f;

	if (path->connectionFlags[index] & PATHFLAG_DOUBLE)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, true);
	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH && g_waypoint->GetPath(index)->flags & WAYPOINT_CAMP)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_max.GetFloat() * 2.0f / offset));
}

inline const float HF_PathDist(int start, int goal)
{
	Path* pathStart = g_waypoint->GetPath(start);
	Path* pathGoal = g_waypoint->GetPath(goal);

	return fabsf(pathGoal->origin.x - pathStart->origin.x) + fabsf(pathGoal->origin.y - pathStart->origin.y) + fabsf(pathGoal->origin.z - pathStart->origin.z);
}

inline const float HF_NumberNodes(int start, int goal)
{
	return HF_PathDist(start, goal) / 128.0f * g_exp.GetKillHistory();
}

inline const float HF_None(int start, int goal)
{
	return HF_PathDist(start, goal) / (128.0f * 100.0f);
}

// SyPB Pro P.42 - Zombie Waypoint improve
inline const float GF_CostZBNormal(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 0.9f)
			return 65355.0f;
	}

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (sypbm_anti_block.GetInt() == 1 && g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost += g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH)
		baseCost += pathDist * 1.5f;

	return pathDist + (baseCost * (engine->RandomFloat(sypb_dangerfactor_min.GetFloat(), sypb_dangerfactor_max.GetFloat()) * 2.0f / offset));
}

inline const float GF_CostZBRusher(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 0.9f)
			return 65355.0f;
	}

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	// SyPBM 1.52 - rusher bots cant wait for boosting
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_CROUCH)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_min.GetFloat() * 2.0f / offset));
}

inline const float GF_CostZBCareful(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_COUNTER)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (sypbm_anti_block.GetInt() == 1 && g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int neighbour = g_waypoint->GetPath(index)->index[i];

		if (neighbour != -1)
			baseCost *= g_exp.GetDamage(neighbour, neighbour, team);
	}

	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_LADDER)
		baseCost += pathDist * 2.0f;

	return pathDist + (baseCost * (sypb_dangerfactor_min.GetFloat() * 2.0f / offset));
}

inline const float GF_CostHM(int index, int parent, int team, float offset)
{
	if (g_waypoint->GetPath(index)->flags & WAYPOINT_FALLCHECK)
	{
		TraceResult tr;
		Vector src = g_waypoint->GetPath(index)->origin;
		src.y -= 20.0f;

		TraceLine(src, g_waypoint->GetPath(index)->origin, true, false, null, &tr);

		if (tr.flFraction < 0.9f)
			return 65355.0f;
	}

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_TERRORIST)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_AVOID)
		return 65355.0f;

	if (g_waypoint->GetPath(index)->flags & WAYPOINT_DJUMP)
		return 65355.0f;

	float baseCost = g_exp.GetAStarValue(index, team, false);
	float pathDist = g_waypoint->GetPathDistanceFloat(parent, index);

	return pathDist + (baseCost * (sypb_dangerfactor_min.GetFloat() * 2.0f / offset));
}

void Bot::FindPath(int srcIndex, int destIndex, uint8_t pathType)
{
	// this function finds a path from srcIndex to destIndex
	if (m_pathtimer + 1.0 > engine->GetTime())
		return;

	m_pathtimer = engine->GetTime();

	if (srcIndex > g_numWaypoints - 1 || srcIndex < 0)
	{
		AddLogEntry(LOG_ERROR, "Pathfinder source path index not valid (%d)", srcIndex);
		return;
	}

	if (destIndex > g_numWaypoints - 1 || destIndex < 0)
	{
		AddLogEntry(LOG_ERROR, "Pathfinder destination path index not valid (%d)", destIndex);
		return;
	}

	if (HasHostage())
		DeleteSearchNodes();

	if (m_isStuck && (!m_isZombieBot || sypbm_anti_block.GetInt() == 1))
		DeleteSearchNodes();

	if (!IsAlive(GetEntity()))
		DeleteSearchNodes();

	if (!IsOnFloor())
		DeleteSearchNodes();

	m_chosenGoalIndex = srcIndex;
	m_goalValue = 0.0f;

	// A* Stuff
	enum AStarState_t { OPEN, CLOSED, NEW };

	struct AStar_t
	{
		float g;
		float f;
		int parent;

		AStarState_t state;
	} astar[Const_MaxWaypoints];

	PriorityQueue openList;

	for (int i = 0; i < Const_MaxWaypoints; i++)
	{
		astar[i].g = 0;
		astar[i].f = 0;
		astar[i].parent = -1;
		astar[i].state = NEW;
	}

	const float (*gcalc) (int, int, int, float) = null;
	const float (*hcalc) (int, int) = null;

	float offset = 1.0f;

	// SyPBM 1.52 - zombie pathcost improve
	if (m_isZombieBot)
	{
		if (m_personality == PERSONALITY_RUSHER)
			gcalc = GF_CostZBRusher;
		else if (m_personality == PERSONALITY_CAREFUL)
			gcalc = GF_CostZBCareful;
		else
			gcalc = GF_CostZBNormal;

		if (pathType == 1)
			hcalc = HF_NumberNodes;
		else
			hcalc = HF_None;

		offset = static_cast <float> (m_skill / 25);
	}
	else if (IsZombieMode() && !m_isZombieBot)
	{
		gcalc = GF_CostHM;
		hcalc = HF_None;

		offset = static_cast <float> (m_skill / 25);
	}
	else if (HasHostage())
	{
		gcalc = GF_CostNoHostage;
		hcalc = HF_NumberNodes;

		offset = static_cast <float> (m_skill / 25);
	}
	if (pathType == 1)
	{
		if (m_isVIP || m_isReloading || m_inBombZone) // SyPBM 1.50 - Pathcost improve
			gcalc = GF_CostDistCareful;
		else
			gcalc = m_personality == PERSONALITY_CAREFUL ? GF_CostDistCareful : m_personality == PERSONALITY_RUSHER ? GF_CostDistRusher : GF_CostDistNormal;

		hcalc = HF_NumberNodes;

		offset = static_cast <float> (m_skill / 25);
	}
	else
	{
		if (m_isVIP || m_isReloading || m_inBombZone) // SyPBM 1.50 - Pathcost improve
			gcalc = GF_CostCareful;
		else
			gcalc = m_personality == PERSONALITY_CAREFUL ? GF_CostCareful : m_personality == PERSONALITY_RUSHER ? GF_CostRusher : GF_CostNormal;

		hcalc = HF_None;

		offset = static_cast <float> (m_skill / 25);
	}

	// put start node into open list
	astar[srcIndex].g = gcalc(srcIndex, -1, m_team, offset);
	astar[srcIndex].f = astar[srcIndex].g + hcalc(srcIndex, destIndex);
	astar[srcIndex].state = OPEN;

	openList.Insert(srcIndex, astar[srcIndex].g);

	while (!openList.Empty())
	{
		// remove the first node from the open list
		int currentIndex = openList.Remove();

		// is the current node the goal node?
		if (currentIndex == destIndex)
		{
			// build the complete path
			m_navNode = null;

			do
			{
				PathNode* path = new PathNode;

				if (path == null)
					TerminateOnMalloc();

				path->index = currentIndex;
				path->next = m_navNode;

				m_navNode = path;
				currentIndex = astar[currentIndex].parent;

			} while (currentIndex != -1);

			m_navNodeStart = m_navNode;

			return;
		}

		if (astar[currentIndex].state != OPEN)
			continue;

		// put current node into CLOSED list
		astar[currentIndex].state = CLOSED;

		// now expand the current node
		for (int i = 0; i < Const_MaxPathIndex; i++)
		{
			int self = g_waypoint->GetPath(currentIndex)->index[i];

			if (self == -1)
				continue;

			// calculate the F value as F = G + H
			float g = astar[currentIndex].g + gcalc(self, currentIndex, m_team, offset);
			float h = hcalc(srcIndex, destIndex);
			float f = g + h;

			if (astar[self].state == NEW || astar[self].f > f)
			{
				// put the current child into open list
				astar[self].parent = currentIndex;
				astar[self].state = OPEN;

				astar[self].g = g;
				astar[self].f = f;

				openList.Insert(self, g);
			}
		}
	}

	if (!HasHostage())
		FindPathPOD(srcIndex, destIndex); // A* found no path, try floyd pathfinder instead
}

void Bot::DeleteSearchNodes(void)
{
	if (m_pathtimer + 1.0f > engine->GetTime())
		return;

	PathNode* deletingNode = null;
	PathNode* node = m_navNodeStart;

	while (node != null)
	{
		deletingNode = node->next;
		delete node;

		node = deletingNode;
	}

	m_navNodeStart = null;
	m_navNode = null;
	m_chosenGoalIndex = -1;
}

// SyPB Pro P.38 - Breakable improve 
void Bot::CheckTouchEntity(edict_t* entity)
{
	if (m_checktouch + 1.0f > engine->GetTime())
		return;

	// SyPB Pro P.40 - Touch Entity Attack Action
	if (m_currentWeapon == WEAPON_KNIFE &&
		(!FNullEnt(m_enemy) || !FNullEnt(m_breakableEntity)) &&
		(m_enemy == entity || m_breakableEntity == entity))
		KnifeAttack((pev->origin - GetEntityOrigin(entity)).GetLength() + 50.0f);

	if (IsShootableBreakable(entity))
	{
		bool attackBreakable = false;
		// SyPB Pro P.40 - Breakable 
		if (m_isStuck || &m_navNode[0] == null)
			attackBreakable = true;
		else if (m_currentWaypointIndex != -1)
		{
			TraceResult tr;
			TraceLine(pev->origin, g_waypoint->GetPath(m_currentWaypointIndex)->origin, false, false, GetEntity(), &tr);
			if (tr.pHit == entity)// && tr.flFraction < 1.0f)
				attackBreakable = true;
		}

		if (attackBreakable)
		{
			m_breakableEntity = entity;
			m_breakable = GetEntityOrigin(entity);
			m_destOrigin = m_breakable;

			if (pev->origin.z > m_breakable.z)
				m_campButtons = IN_DUCK;
			else
				m_campButtons = pev->button & IN_DUCK;

			PushTask(TASK_DESTROYBREAKABLE, TASKPRI_SHOOTBREAKABLE, -1, 0.0, false);
		}
	}

	m_checktouch = engine->GetTime();
}

// SyPB Pro P.47 - Base improve
void Bot::SetEnemy(edict_t* entity)
{
	if (FNullEnt(entity) || !IsAlive(entity))
	{
		if (!FNullEnt(m_enemy) && &m_navNode[0] == null)
		{
			SetEntityWaypoint(GetEntity(), -2);
			m_currentWaypointIndex = -1;
			GetValidWaypoint();
		}

		m_enemy = null;
		m_enemyOrigin = nullvec;
		return;
	}

	// SyPB Pro P.48 - Base improve
	if (m_enemy != entity)
		m_enemyReachableTimer = 0.0f;

	m_enemy = entity;
}

// SyPB Pro P.42 - NPC Fixed
void Bot::SetLastEnemy(edict_t* entity)
{
	if (FNullEnt(entity) || !IsValidPlayer(entity) || !IsAlive(entity))
	{
		m_lastEnemy = null;
		m_lastEnemyOrigin = nullvec;
		return;
	}

	m_lastEnemy = entity;
	m_lastEnemyOrigin = GetEntityOrigin(entity);
}

// SyPB Pro P.40 - Move Target
void Bot::SetMoveTarget(edict_t* entity)
{
	m_moveTargetOrigin = GetEntityOrigin(entity);
	if (FNullEnt(entity) || m_moveTargetOrigin == nullvec)
	{
		m_moveTargetEntity = null;
		m_moveTargetOrigin = nullvec;

		// SyPB Pro P.42 - Small Change
		if (GetCurrentTask()->taskID == TASK_MOVETOTARGET)
		{
			RemoveCertainTask(TASK_MOVETOTARGET);

			// SyPB Pro P.43 - Move Target improve
			m_prevGoalIndex = -1;
			GetCurrentTask()->data = -1;

			DeleteSearchNodes();
		}

		return;
	}

	// SyPB Pro P.43 - Fixed 
	m_states &= ~STATE_SEEINGENEMY;
	SetEnemy(null);
	SetLastEnemy(null);
	m_enemyUpdateTime = 0.0f;
	m_aimFlags &= ~AIM_ENEMY;

	// SyPB Pro P.42 - Move Target
	if (m_moveTargetEntity == entity)
		return;

	SetEntityWaypoint(entity);
	SetEntityWaypoint(GetEntity(), GetEntityWaypoint(entity));

	m_currentWaypointIndex = -1;
	GetValidWaypoint();

	m_moveTargetEntity = entity;

	PushTask(TASK_MOVETOTARGET, TASKPRI_MOVETOTARGET, -1, 0.0, true);
}

int Bot::GetAimingWaypoint(Vector targetOriginPos)
{
	// return the most distant waypoint which is seen from the Bot to the Target and is within count

	if (m_currentWaypointIndex <= -1 || m_currentWaypointIndex > g_numWaypoints)
		GetValidWaypoint();

	int srcIndex = m_currentWaypointIndex;
	int destIndex = g_waypoint->FindNearest(targetOriginPos);
	int bestIndex = srcIndex;

	PathNode* node = new PathNode;

	if (node == null)
		TerminateOnMalloc();

	node->index = destIndex;
	node->next = null;

	PathNode* startNode = node;

	while (destIndex != srcIndex)
	{
		destIndex = *(g_waypoint->m_pathMatrix + (destIndex * g_numWaypoints) + srcIndex);

		if (destIndex < 0)
			break;

		node->next = new PathNode();
		node = node->next;

		if (node == null)
			TerminateOnMalloc();

		node->index = destIndex;
		node->next = null;

		if (g_waypoint->IsVisible(m_currentWaypointIndex, destIndex))
		{
			bestIndex = destIndex;
			break;
		}
	}

	while (startNode != null)
	{
		node = startNode->next;
		delete startNode;

		startNode = node;
	}
	return bestIndex;
}

int Bot::FindWaypoint(void)
{
	// SyPB Pro P.42 - Waypoint improve
	int waypointIndex = -1, waypointIndex1, waypointIndex2;
	int client = ENTINDEX(GetEntity()) - 1;
	waypointIndex1 = g_clients[client].wpIndex;
	waypointIndex2 = g_clients[client].wpIndex2;

	if (waypointIndex1 == -1 && waypointIndex2 == -1)
	{
		SetEntityWaypoint(GetEntity());
		waypointIndex1 = g_clients[client].wpIndex;
		waypointIndex2 = g_clients[client].wpIndex2;
	}

	// SyPBM 1.53 - stuck fix
	if (m_isZombieBot && IsVisible(g_waypoint->GetPath(waypointIndex2)->origin, GetEntity()) && ::IsInViewCone(g_waypoint->GetPath(waypointIndex2)->origin, GetEntity()))
		waypointIndex = waypointIndex2;
	else if (m_isZombieBot && IsVisible(g_waypoint->GetPath(waypointIndex1)->origin, GetEntity()) && ::IsInViewCone(g_waypoint->GetPath(waypointIndex1)->origin, GetEntity()))
		waypointIndex = waypointIndex1;
	else if (IsVisible(g_waypoint->GetPath(waypointIndex2)->origin, GetEntity()))
		waypointIndex = waypointIndex2;
	else if (IsVisible(g_waypoint->GetPath(waypointIndex1)->origin, GetEntity()))
		waypointIndex = waypointIndex1;
	else if (!IsWaypointOccupied(waypointIndex2))
		waypointIndex = waypointIndex2;
	else if (!IsWaypointOccupied(waypointIndex1))
		waypointIndex = waypointIndex1;
	else
	{
		if (waypointIndex2 != -1)
			waypointIndex = waypointIndex2;
		else
			waypointIndex = waypointIndex1;
	}

	m_collideTime = engine->GetTime();
	ChangeWptIndex(waypointIndex);

	return waypointIndex;
}

void Bot::IgnoreCollisionShortly(void)
{
	ResetCollideState();

	m_lastCollTime = engine->GetTime() + 0.35f;
	m_isStuck = false;
	m_checkTerrain = false;
}

// SyPB Pro P.40 - Base Change for Waypoint OS
void Bot::SetWaypointOrigin(void)
{
	m_waypointOrigin = g_waypoint->GetPath(m_currentWaypointIndex)->origin;

	if (g_waypoint->GetPath(m_currentWaypointIndex)->radius > 0)
	{
		MakeVectors(Vector(pev->angles.x, AngleNormalize(pev->angles.y + engine->RandomFloat(-90.0f, 90.0f)), 0.0f));
		float radius = g_waypoint->GetPath(m_currentWaypointIndex)->radius;

		if (&m_navNode[0] != null && m_navNode->next != null)
		{
			Vector waypointOrigin[5];
			for (int i = 0; i < 5; i++)
			{
				waypointOrigin[i] = m_waypointOrigin;
				waypointOrigin[i] += Vector(engine->RandomFloat(-radius, radius), engine->RandomFloat(-radius, radius), 0.0f);
			}

			int destIndex = m_navNode->next->index;

			float sDistance = 9999.0f;
			int sPoint = -1;
			for (int i = 0; i < 5; i++)
			{
				// SyPB Pro P.42 - Small Waypoint OS improve
				float distance = (pev->origin - waypointOrigin[i]).GetLength2D() +
					(waypointOrigin[i] - g_waypoint->GetPath(destIndex)->origin).GetLength2D();

				if (distance < sDistance)
				{
					sPoint = i;
					sDistance = distance;
				}
			}

			if (sPoint != -1)
			{
				m_waypointOrigin = waypointOrigin[sPoint];

				return;
			}
		}

		m_waypointOrigin = m_waypointOrigin + g_pGlobals->v_forward * engine->RandomFloat(0, radius);
	}
}

void Bot::GetValidWaypoint(void)
{
	// checks if the last waypoint the bot was heading for is still valid

	// SyPB Pro P.38 - Find Waypoint Improve
	bool needFindWaypont = false;
	if (m_currentWaypointIndex < 0 || m_currentWaypointIndex > g_numWaypoints)
		needFindWaypont = true;
	else if ((m_navTimeset + GetEstimatedReachTime() < engine->GetTime()))
		needFindWaypont = true;
	// SyPB Pro P.42 - Waypoint improve
	else
	{
		int waypointIndex1, waypointIndex2;
		int client = ENTINDEX(GetEntity()) - 1;
		waypointIndex1 = g_clients[client].wpIndex;
		waypointIndex2 = g_clients[client].wpIndex2;

		if (m_currentWaypointIndex != waypointIndex1 && m_currentWaypointIndex != waypointIndex2 && (g_waypoint->GetPath(m_currentWaypointIndex)->origin - pev->origin).GetLength() > 600.0f && !g_waypoint->Reachable(GetEntity(), m_currentWaypointIndex))
			needFindWaypont = true;
	}

	if (needFindWaypont)
	{
		if (m_currentWaypointIndex != -1 && GetGameMod() == MODE_BASE)
			g_exp.CollectValidDamage(m_currentWaypointIndex, m_team);

		DeleteSearchNodes();
		FindWaypoint();

		SetWaypointOrigin();
	}
}

void Bot::ChangeWptIndex(int waypointIndex)
{
	// SyPB Pro P.48 - Base Change for Waypoint OS
	bool badPrevWpt = true;
	for (int i = 0; (m_currentWaypointIndex != -1 && i < Const_MaxPathIndex); i++)
	{
		if (g_waypoint->GetPath(m_currentWaypointIndex)->index[i] == waypointIndex)
			badPrevWpt = false;
	}

	if (badPrevWpt == true)
	{
		m_prevWptIndex[1] = -1;
		m_prevWptIndex[0] = -1;
	}
	else
	{
		m_prevWptIndex[1] = m_prevWptIndex[0];
		m_prevWptIndex[0] = m_currentWaypointIndex;
	}

	m_currentWaypointIndex = waypointIndex;
	m_navTimeset = engine->GetTime();

	// get the current waypoint flags
	if (m_currentWaypointIndex != -1)
		m_waypointFlags = g_waypoint->GetPath(m_currentWaypointIndex)->flags;
	else
		m_waypointFlags = 0;
}

int Bot::ChooseBombWaypoint(void)
{
	// this function finds the best goal (bomb) waypoint for CTs when searching for a planted bomb.

	if (g_waypoint->m_goalPoints.IsEmpty())
		return engine->RandomInt(0, g_numWaypoints - 1); // reliability check

	Vector bombOrigin = CheckBombAudible();

	// if bomb returns no valid vector, return the current bot pos
	if (bombOrigin == nullvec)
		bombOrigin = pev->origin;

	Array <int> goals;

	int goal = 0, count = 0;
	float lastDistance = FLT_MAX;

	// find nearest goal waypoint either to bomb (if "heard" or player)
	ITERATE_ARRAY(g_waypoint->m_goalPoints, i)
	{
		float distance = (g_waypoint->GetPath(g_waypoint->m_goalPoints[i])->origin - bombOrigin).GetLength();

		// check if we got more close distance
		if (distance < lastDistance)
		{
			goal = g_waypoint->m_goalPoints[i];
			lastDistance = distance;

			goals.Push(goal);
		}
	}

	while (g_waypoint->IsGoalVisited(goal))
	{
		if (g_waypoint->m_goalPoints.GetElementNumber() == 1)
			goal = g_waypoint->m_goalPoints[0];
		else
			goal = goals.GetRandomElement();

		if (count++ >= goals.GetElementNumber())
			break;
	}
	return goal;
}

int Bot::FindDefendWaypoint(Vector origin)
{
	// this function tries to find a good position which has a line of sight to a position,
	// provides enough cover point, and is far away from the defending position

	TraceResult tr;

	int waypointIndex[Const_MaxPathIndex];
	int minDistance[Const_MaxPathIndex];

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		waypointIndex[i] = -1;
		minDistance[i] = 128;
	}

	int posIndex = g_waypoint->FindNearest(origin);
	int srcIndex = g_waypoint->FindNearest(pev->origin);

	// some of points not found, return random one
	if (srcIndex == -1 || posIndex == -1)
		return engine->RandomInt(0, g_waypoint->m_campPoints.GetElementNumber() - 1);

	for (int i = 0; i < g_waypoint->m_campPoints.GetElementNumber(); i++) // find the best waypoint now
	{
		// exclude ladder & current waypoints
		if ((g_waypoint->GetPath(i)->flags & WAYPOINT_LADDER) || i == srcIndex || !g_waypoint->IsVisible(i, posIndex) || IsWaypointOccupied(i))
			continue;

		// use the 'real' pathfinding distances
		int distance = g_waypoint->GetPathDistance(srcIndex, i);

		// skip wayponts with distance more than 512 units
		if (distance > 512)
			continue;

		TraceLine(g_waypoint->GetPath(i)->origin, g_waypoint->GetPath(posIndex)->origin, true, true, GetEntity(), &tr);

		// check if line not hit anything
		if (tr.flFraction != 1.0f)
			continue;

		for (int j = 0; j < Const_MaxPathIndex; j++)
		{
			if (distance > minDistance[j])
			{
				waypointIndex[j] = i;
				minDistance[j] = distance;
			}
		}
	}

	// use statistic if we have them
	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		if (waypointIndex[i] != -1)
		{
			int experience = g_exp.GetDamage(waypointIndex[i], waypointIndex[i], m_team) * 100 / MAX_EXPERIENCE_VALUE;

			minDistance[i] = (experience * 100) / 8192;
			minDistance[i] += experience;
		}
	}

	bool isOrderChanged = false;

	// sort results waypoints for farest distance
	do
	{
		isOrderChanged = false;

		// completely sort the data
		for (int i = 0; i < 3 && waypointIndex[i] != -1 && waypointIndex[i + 1] != -1 && minDistance[i] > minDistance[i + 1]; i++)
		{
			int index = waypointIndex[i];

			waypointIndex[i] = waypointIndex[i + 1];
			waypointIndex[i + 1] = index;

			index = minDistance[i];

			minDistance[i] = minDistance[i + 1];
			minDistance[i + 1] = index;

			isOrderChanged = true;
		}
	} while (isOrderChanged);

	if (waypointIndex[0] == -1)
	{
		Array <int> found;

		for (int i = 0; i < g_waypoint->m_campPoints.GetElementNumber(); i++)
		{
			if ((g_waypoint->GetPath(i)->origin - origin).GetLength() <= 1024.0f && !g_waypoint->IsVisible(i, posIndex) && !IsWaypointOccupied(i))
				found.Push(i);
		}

		if (found.IsEmpty())
			return engine->RandomInt(0, g_waypoint->m_campPoints.GetElementNumber() - 1); // most worst case, since there a evil error in waypoints

		return found.GetRandomElement();
	}

	int index = 0;

	for (; index < Const_MaxPathIndex; index++)
	{
		if (waypointIndex[index] == -1)
			break;
	}

	return waypointIndex[engine->RandomInt(0, (index - 1))];
}

int Bot::FindCoverWaypoint(float maxDistance)
{
	// this function tries to find a good cover waypoint if bot wants to hide

	// do not move to a position near to the enemy
	if (maxDistance > (m_lastEnemyOrigin - pev->origin).GetLength())
		maxDistance = (m_lastEnemyOrigin - pev->origin).GetLength();

	if (maxDistance < 300.0f)
		maxDistance = 300.0f;

	int srcIndex = m_currentWaypointIndex;
	int enemyIndex = GetEntityWaypoint(m_lastEnemy);
	if (enemyIndex == -1)
		return -1;

	Array <int> enemyIndices;

	int waypointIndex[Const_MaxPathIndex];
	int minDistance[Const_MaxPathIndex];

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		waypointIndex[i] = -1;
		minDistance[i] = static_cast <int> (maxDistance);
	}

	// now get enemies neigbouring points
	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		if (g_waypoint->GetPath(enemyIndex)->index[i] != -1)
			enemyIndices.Push(g_waypoint->GetPath(enemyIndex)->index[i]);
	}

	// ensure we're on valid point
	ChangeWptIndex(srcIndex);

	// find the best waypoint now
	for (int i = 0; i < g_numWaypoints; i++)
	{
		// exclude ladder, current waypoints and waypoints seen by the enemy
		if ((g_waypoint->GetPath(i)->flags & WAYPOINT_LADDER) || i == srcIndex || g_waypoint->IsVisible(enemyIndex, i))
			continue;

		bool neighbourVisible = false;  // now check neighbour waypoints for visibility

		ITERATE_ARRAY(enemyIndices, j)
		{
			if (g_waypoint->IsVisible(enemyIndices[j], i))
			{
				neighbourVisible = true;
				break;
			}
		}

		// skip visible points
		if (neighbourVisible)
			continue;

		// use the 'real' pathfinding distances
		int distances = g_waypoint->GetPathDistance(srcIndex, i);
		int enemyDistance = g_waypoint->GetPathDistance(enemyIndex, i);

		if (distances >= enemyDistance)
			continue;

		for (int j = 0; j < Const_MaxPathIndex; j++)
		{
			if (distances < minDistance[j])
			{
				waypointIndex[j] = i;
				minDistance[j] = distances;

				break;
			}
		}
	}

	// use statistic if we have them
	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		if (waypointIndex[i] != -1)
		{
			int experience = g_exp.GetDamage(waypointIndex[i], waypointIndex[i], m_team) * 100 / MAX_EXPERIENCE_VALUE;

			minDistance[i] = (experience * 100) / 8192;
			minDistance[i] += experience;
		}
	}

	bool isOrderChanged;

	// sort resulting waypoints for nearest distance
	do
	{
		isOrderChanged = false;

		for (int i = 0; i < 3 && waypointIndex[i] != -1 && waypointIndex[i + 1] != -1 && minDistance[i] > minDistance[i + 1]; i++)
		{
			int index = waypointIndex[i];

			waypointIndex[i] = waypointIndex[i + 1];
			waypointIndex[i + 1] = index;
			index = minDistance[i];
			minDistance[i] = minDistance[i + 1];
			minDistance[i + 1] = index;

			isOrderChanged = true;
		}
	} while (isOrderChanged);

	TraceResult tr;

	// take the first one which isn't spotted by the enemy
	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		if (waypointIndex[i] != -1)
		{
			TraceLine(m_lastEnemyOrigin + Vector(0.0f, 0.0f, 36.0f), g_waypoint->GetPath(waypointIndex[i])->origin, true, true, GetEntity(), &tr);

			if (tr.flFraction < 1.0f)
				return waypointIndex[i];
		}
	}

	// if all are seen by the enemy, take the first one
	if (waypointIndex[0] != -1)
		return waypointIndex[0];

	return -1; // do not use random points
}

bool Bot::GetBestNextWaypoint(void)
{
	// this function does a realtime postprocessing of waypoints return from the
	// pathfinder, to vary paths and find the best waypoint on our way

	InternalAssert(m_navNode != null);
	InternalAssert(m_navNode->next != null);

	if (!IsWaypointUsed(m_navNode->index))
		return false;

	for (int i = 0; i < Const_MaxPathIndex; i++)
	{
		int id = g_waypoint->GetPath(m_currentWaypointIndex)->index[i];

		if (id != -1 && g_waypoint->IsConnected(id, m_navNode->next->index) && g_waypoint->IsConnected(m_currentWaypointIndex, id))
		{
			if (g_waypoint->GetPath(id)->flags & WAYPOINT_LADDER) // don't use ladder waypoints as alternative
				continue;

			if (!IsWaypointUsed(id))
			{
				m_navNode->index = id;
				return true;
			}
		}
	}
	return false;
}

bool Bot::HeadTowardWaypoint(void)
{
	// advances in our pathfinding list and sets the appropiate destination origins for this bot

	GetValidWaypoint(); // check if old waypoints is still reliable

	// no waypoints from pathfinding?
	if (m_navNode == null)
		return false;

	TraceResult tr;

	m_navNode = m_navNode->next; // advance in list
	m_currentTravelFlags = 0; // reset travel flags (jumping etc)

	// we're not at the end of the list?
	if (m_navNode != null)
	{
		if (m_navNode->next != null && !(g_waypoint->GetPath(m_navNode->next->index)->flags & WAYPOINT_LADDER))
		{
			if (m_navNodeStart != m_navNode)
			{
				GetBestNextWaypoint();
				int taskID = GetCurrentTask()->taskID;

				m_minSpeed = pev->maxspeed;

				// only if we in normal task and bomb is not planted
				if (GetGameMod() == MODE_BASE && taskID == TASK_NORMAL && !g_bombPlanted && m_personality != PERSONALITY_RUSHER && !(pev->weapons & (1 << WEAPON_C4)) && !m_isVIP && (m_loosedBombWptIndex == -1 && m_team == TEAM_TERRORIST))
				{
					m_campButtons = 0;

					int waypoint = m_navNode->next->index;
					int kills = g_exp.GetDamage(waypoint, waypoint, m_team);

					// if damage done higher than one
					if (kills > 1 && g_timeRoundMid > engine->GetTime() && g_killHistory > 0)
					{
						kills = (kills * 100) / g_killHistory;
						kills /= 100;

						switch (m_personality)
						{
						case PERSONALITY_NORMAL:
							kills /= 3;
							break;

						default:
							kills /= 2;
							break;
						}

						if (m_baseAgressionLevel < static_cast <float> (kills))
						{
							PushTask(TASK_CAMP, TASKPRI_CAMP, -1, engine->GetTime() + (m_fearLevel * (g_timeRoundMid - engine->GetTime()) * 0.5f), true); // push camp task on to stack
							PushTask(TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, FindDefendWaypoint(g_waypoint->GetPath(waypoint)->origin), 0.0f, true);

							m_campButtons |= IN_DUCK;
						}
					}
					else if (g_botsCanPause && !IsOnLadder() && !IsInWater() && !m_currentTravelFlags && IsOnFloor())
					{
						if (static_cast <float> (kills) == m_baseAgressionLevel)
							m_campButtons |= IN_DUCK;
						else if (engine->RandomInt(1, 100) > (m_skill + engine->RandomInt(1, 20)))
							m_minSpeed = GetWalkSpeed();
					}
				}
			}
		}

		if (m_navNode != null)
		{
			int destIndex = m_navNode->index;

			// find out about connection flags
			if (m_currentWaypointIndex != -1)
			{
				Path* path = g_waypoint->GetPath(m_currentWaypointIndex);

				for (int i = 0; i < Const_MaxPathIndex; i++)
				{
					if (path->index[i] == m_navNode->index)
					{
						m_currentTravelFlags = path->connectionFlags[i];
						m_desiredVelocity = path->connectionVelocity[i];
						m_jumpFinished = false;

						break;
					}
				}

				// check if bot is going to jump
				bool willJump = false;
				float jumpDistance = 0.0f;

				Vector src = nullvec;
				Vector destination = nullvec;

				// try to find out about future connection flags
				if (m_navNode->next != null)
				{
					for (int i = 0; i < Const_MaxPathIndex; i++)
					{
						if (g_waypoint->GetPath(m_navNode->index)->index[i] == m_navNode->next->index && (g_waypoint->GetPath(m_navNode->index)->connectionFlags[i] & PATHFLAG_JUMP))
						{
							src = g_waypoint->GetPath(m_navNode->index)->origin;
							destination = g_waypoint->GetPath(m_navNode->next->index)->origin;

							jumpDistance = (g_waypoint->GetPath(m_navNode->index)->origin - g_waypoint->GetPath(m_navNode->next->index)->origin).GetLength();
							willJump = true;

							break;
						}
					}
				}

				// SyPB Pro P.48 - Jump improve 
				if (willJump && !(m_states & STATE_SEEINGENEMY) && FNullEnt(m_lastEnemy) && m_currentWeapon != WEAPON_KNIFE && !m_isReloading && (jumpDistance > 210 || (destination.z + 32.0f > src.z && jumpDistance > 150) || ((destination - src).GetLength2D() < 50 && jumpDistance > 60) || pev->maxspeed <= 210))
				{
					// SyPB Pro P.42 - Ladder improve
					if (!IsAntiBlock(GetEntity()) && !IsOnLadder() && g_waypoint->GetPath(destIndex)->flags & WAYPOINT_LADDER)
					{
						float waitTime = -1.0f;
						for (int c = 0; c < engine->GetMaxClients(); c++)
						{
							Bot* otherBot = g_botManager->GetBot(c);
							if (otherBot == null || otherBot == this || !IsAlive(otherBot->GetEntity()))
								continue;

							if (!otherBot->IsOnLadder())
							{
								if (otherBot->m_currentWaypointIndex == destIndex)
								{
									Vector wpOrigin = g_waypoint->GetPath(destIndex)->origin;

									if ((otherBot->pev->origin - wpOrigin).GetLength() <
										(pev->origin - wpOrigin).GetLength())
									{
										waitTime = 0.5f;
										break;
									}
								}

								continue;
							}

							if (&otherBot->m_navNode[0] == null)
							{
								int otherBotLadderWpIndex = otherBot->m_currentWaypointIndex;
								if (destIndex == otherBotLadderWpIndex || (m_navNode->next != null && m_navNode->next->index == otherBotLadderWpIndex))
								{
									waitTime = 0.5f;
									break;
								}

								continue;
							}

							if (otherBot->m_currentWaypointIndex == destIndex && otherBot->m_navNode != null && otherBot->m_navNode->index == m_currentWaypointIndex)
							{
								waitTime = 0.5f;
								break;
							}
						}

						if (waitTime != -1.0f)
						{
							PushTask(TASK_PAUSE, TASKPRI_PAUSE, -1, engine->GetTime() + waitTime, false);
							return false;
						}
					}
				}
			}

			ChangeWptIndex(destIndex);
		}
	}

	SetWaypointOrigin();

	if (IsOnLadder())
	{
		TraceLine(Vector(pev->origin.x, pev->origin.y, pev->absmin.z), m_waypointOrigin, true, true, GetEntity(), &tr);

		// SyPB Pro P.36 - ladder improve
		if (tr.flFraction < 1.0f)
		{
			if (m_waypointOrigin.z >= pev->origin.z)
				m_waypointOrigin += tr.vecPlaneNormal;
			else
				m_waypointOrigin -= tr.vecPlaneNormal;
		}
	}

	m_navTimeset = engine->GetTime();

	return true;
}

bool Bot::CantMoveForward(Vector normal, TraceResult* tr)
{
	// Checks if bot is blocked in his movement direction (excluding doors)

	// first do a trace from the bot's eyes forward...
	Vector src = EyePosition();
	Vector forward = src + normal * 24.0f;

	MakeVectors(Vector(0.0f, pev->angles.y, 0.0f));

	// trace from the bot's eyes straight forward...
	TraceLine(src, forward, true, GetEntity(), tr);

	// check if the trace hit something...
	if (tr->flFraction < 1.0f)
	{
		if (strncmp("func_door", STRING(tr->pHit->v.classname), 9) == 0)
			return false;

		return true;  // bot's head will hit something
	}

	// bot's head is clear, check at shoulder level...
	// trace from the bot's shoulder left diagonal forward to the right shoulder...
	src = EyePosition() + Vector(0.0f, 0.0f, -16.0f) - g_pGlobals->v_right * 16.0f;
	forward = EyePosition() + Vector(0.0f, 0.0f, -16.0f) + g_pGlobals->v_right * 16.0f + normal * 24.0f;

	TraceLine(src, forward, true, GetEntity(), tr);

	// check if the trace hit something...
	if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
		return true;  // bot's body will hit something

	 // bot's head is clear, check at shoulder level...
	 // trace from the bot's shoulder right diagonal forward to the left shoulder...
	src = EyePosition() + Vector(0.0f, 0.0f, -16.0f) + g_pGlobals->v_right * 16.0f;
	forward = EyePosition() + Vector(0.0f, 0.0f, -16.0f) - g_pGlobals->v_right * 16.0f + normal * 24.0f;

	TraceLine(src, forward, true, GetEntity(), tr);

	// check if the trace hit something...
	if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
		return true;  // bot's body will hit something

	 // now check below waist
	if (pev->flags & FL_DUCKING)
	{
		src = pev->origin + Vector(0.0f, 0.0f, -19.0f + 19.0f);
		forward = src + Vector(0.0f, 0.0f, 10.0f) + normal * 24.0f;

		TraceLine(src, forward, true, GetEntity(), tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
			return true;  // bot's body will hit something

		src = pev->origin;
		forward = src + normal * 24.0f;

		TraceLine(src, forward, true, GetEntity(), tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
			return true;  // bot's body will hit something
	}
	else
	{
		// trace from the left waist to the right forward waist pos
		src = pev->origin + Vector(0.0f, 0.0f, -17.0f) - g_pGlobals->v_right * 16.0f;
		forward = pev->origin + Vector(0.0f, 0.0f, -17.0f) + g_pGlobals->v_right * 16.0f + normal * 24.0f;

		// trace from the bot's waist straight forward...
		TraceLine(src, forward, true, GetEntity(), tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
			return true;  // bot's body will hit something

		 // trace from the left waist to the right forward waist pos
		src = pev->origin + Vector(0.0f, 0.0f, -17.0f) + g_pGlobals->v_right * 16.0f;
		forward = pev->origin + Vector(0.0f, 0.0f, -17.0f) - g_pGlobals->v_right * 16.0f + normal * 24.0f;

		TraceLine(src, forward, true, GetEntity(), tr);

		// check if the trace hit something...
		if (tr->flFraction < 1.0f && strncmp("func_door", STRING(tr->pHit->v.classname), 9) != 0)
			return true;  // bot's body will hit something
	}

	return false;  // bot can move forward, return false
}

bool Bot::CanJumpUp(Vector normal)
{
	// this function check if bot can jump over some obstacle

	TraceResult tr;

	// can't jump if not on ground and not on ladder/swimming
	if (!IsOnFloor() && (IsOnLadder() || !IsInWater()))
		return false;

	MakeVectors(Vector(0.0f, pev->angles.y, 0.0f));

	// check for normal jump height first...
	Vector src = pev->origin + Vector(0.0f, 0.0f, -36.0f + 45.0f);
	Vector dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	if (tr.flFraction < 1.0f)
		goto CheckDuckJump;
	else
	{
		// now trace from jump height upward to check for obstructions...
		src = dest;
		dest.z = dest.z + 37.0f;

		TraceLine(src, dest, true, GetEntity(), &tr);

		if (tr.flFraction < 1.0f)
			return false;
	}

	// now check same height to one side of the bot...
	src = pev->origin + g_pGlobals->v_right * 16.0f + Vector(0.0f, 0.0f, -36.0f + 45.0f);
	dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		goto CheckDuckJump;

	// now trace from jump height upward to check for obstructions...
	src = dest;
	dest.z = dest.z + 37.0f;

	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height on the other side of the bot...
	src = pev->origin + (-g_pGlobals->v_right * 16.0f) + Vector(0.0f, 0.0f, -36.0f + 45.0f);
	dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		goto CheckDuckJump;

	// now trace from jump height upward to check for obstructions...
	src = dest;
	dest.z = dest.z + 37.0f;

	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	return tr.flFraction > 1.0f;

	// here we check if a duck jump would work...
CheckDuckJump:
	// use center of the body first... maximum duck jump height is 62, so check one unit above that (63)
	src = pev->origin + Vector(0.0f, 0.0f, -36.0f + 63.0f);
	dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	if (tr.flFraction < 1.0f)
		return false;
	else
	{
		// now trace from jump height upward to check for obstructions...
		src = dest;
		dest.z = dest.z + 37.0f;

		TraceLine(src, dest, true, GetEntity(), &tr);

		// if trace hit something, check duckjump
		if (tr.flFraction < 1.0f)
			return false;
	}

	// now check same height to one side of the bot...
	src = pev->origin + g_pGlobals->v_right * 16.0f + Vector(0.0f, 0.0f, -36.0f + 63.0f);
	dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		return false;

	// now trace from jump height upward to check for obstructions...
	src = dest;
	dest.z = dest.z + 37.0f;

	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		return false;

	// now check same height on the other side of the bot...
	src = pev->origin + (-g_pGlobals->v_right * 16.0f) + Vector(0.0f, 0.0f, -36.0f + 63.0f);
	dest = src + normal * 32.0f;

	// trace a line forward at maximum jump height...
	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	if (tr.flFraction < 1.0f)
		return false;

	// now trace from jump height upward to check for obstructions...
	src = dest;
	dest.z = dest.z + 37.0f;

	TraceLine(src, dest, true, GetEntity(), &tr);

	// if trace hit something, return false
	return tr.flFraction > 1.0f;
}

bool Bot::CheckWallOnLeft(void)
{
	TraceResult tr;
	MakeVectors(pev->angles);

	TraceLine(pev->origin, pev->origin - g_pGlobals->v_right * 40.0f, true, GetEntity(), &tr);

	// check if the trace hit something...
	if (tr.flFraction < 1.0f)
		return true;

	return false;
}

bool Bot::CheckWallOnRight(void)
{
	TraceResult tr;
	MakeVectors(pev->angles);

	// do a trace to the right...
	TraceLine(pev->origin, pev->origin + g_pGlobals->v_right * 40.0f, true, GetEntity(), &tr);

	// check if the trace hit something...
	if (tr.flFraction < 1.0f)
		return true;

	return false;
}

bool Bot::IsDeadlyDrop(Vector targetOriginPos)
{
	// this function eturns if given location would hurt Bot with falling damage

	Vector botPos = pev->origin;
	TraceResult tr;

	Vector move((targetOriginPos - botPos).ToYaw(), 0.0f, 0.0f);
	MakeVectors(move);

	Vector direction = (targetOriginPos - botPos).Normalize();  // 1 unit long
	Vector check = botPos;
	Vector down = botPos;

	down.z = down.z - 1000.0f;  // straight down 1000 units

	TraceHull(check, down, true, head_hull, GetEntity(), &tr);

	if (tr.flFraction > 0.036f) // We're not on ground anymore?
		tr.flFraction = 0.036f;

	float height;
	float lastHeight = tr.flFraction * 1000.0f;  // height from ground

	float distance = (targetOriginPos - check).GetLength();  // distance from goal

	while (distance > 16.0f)
	{
		check = check + direction * 16.0f; // move 10 units closer to the goal...

		down = check;
		down.z = down.z - 1000.0f;  // straight down 1000 units

		TraceHull(check, down, true, head_hull, GetEntity(), &tr);

		if (tr.fStartSolid) // Wall blocking?
			return false;

		height = tr.flFraction * 1000.0f;  // height from ground

		if (lastHeight < height - 100.0f) // Drops more than 100 Units?
			return true;

		lastHeight = height;
		distance = (targetOriginPos - check).GetLength();  // distance from goal
	}
	return false;
}

// SyPB Pro P.47 - Base improve
void Bot::CheckCloseAvoidance(const Vector& dirNormal)
{
	if (m_seeEnemyTime + 2.0f < engine->GetTime())
		return;

	edict_t* nearest = NULL;
	float nearestDist = 99999.0f;
	int playerCount = 0;

	for (int i = 0; i < engine->GetMaxClients(); i++)
	{
		if (!(g_clients[i].flags & (CFLAG_USED | CFLAG_ALIVE)) || g_clients[i].ent == GetEntity() || GetTeam(g_clients[i].ent) != m_team)
			continue;

		float distance = (g_clients[i].ent->v.origin - pev->origin).GetLength();

		if (distance < nearestDist && distance < pev->maxspeed)
		{
			nearestDist = distance;
			nearest = g_clients[i].ent;

			playerCount++;
		}
	}

	if (playerCount < 4 && IsValidPlayer(nearest))
	{
		MakeVectors(m_moveAngles); // use our movement angles

		// try to predict where we should be next frame
		Vector moved = pev->origin + g_pGlobals->v_forward * m_moveSpeed * m_frameInterval;
		moved += g_pGlobals->v_right * m_strafeSpeed * m_frameInterval;
		moved += pev->velocity * m_frameInterval;

		float nearestDistance = (nearest->v.origin - pev->origin).GetLength2D();
		float nextFrameDistance = ((nearest->v.origin + nearest->v.velocity * m_frameInterval) - pev->origin).GetLength2D();

		// is player that near now or in future that we need to steer away?
		if ((nearest->v.origin - moved).GetLength2D() <= 48.0f || (nearestDistance <= 56.0f && nextFrameDistance < nearestDistance))
		{
			// to start strafing, we have to first figure out if the target is on the left side or right side
			const Vector& dirToPoint = (pev->origin - nearest->v.origin).Normalize2D();

			if ((dirToPoint | g_pGlobals->v_right.Normalize2D()) > 0.0f)
				SetStrafeSpeed(dirNormal, pev->maxspeed);
			else
				SetStrafeSpeed(dirNormal, -pev->maxspeed);

			if (nearestDistance < 56.0f && (dirToPoint | g_pGlobals->v_forward.Normalize2D()) < 0.0f)
				m_moveSpeed = -pev->maxspeed;
		}
	}
}

int Bot::GetCampAimingWaypoint(void)
{
	// Find a good WP to look at when camping

	int count = 0, indeces[3];
	float distTab[3];
	uint16 visibility[3];

	int currentWaypoint = GetEntityWaypoint(GetEntity());
	if (currentWaypoint == -1)
		return engine->RandomInt(0, g_numWaypoints - 1);

	for (int i = 0; i < g_numWaypoints; i++)
	{
		if (currentWaypoint == i || IsVisible(g_waypoint->GetPath(i)->origin, GetEntity()))
			continue;

		if (!IsZombieMode() && !(::IsInViewCone(g_waypoint->GetPath(i)->origin, GetEntity())))
			continue;

		if (count < 3)
		{
			indeces[count] = i;

			distTab[count] = (pev->origin - g_waypoint->GetPath(i)->origin).GetLength();
			visibility[count] = g_waypoint->GetPath(i)->vis.crouch + g_waypoint->GetPath(i)->vis.stand;

			count++;
		}
		else
		{
			float distance = (pev->origin - g_waypoint->GetPath(i)->origin).GetLength();
			uint16 visBits = g_waypoint->GetPath(i)->vis.crouch + g_waypoint->GetPath(i)->vis.stand;

			for (int j = 0; j < 3; j++)
			{
				if (visBits >= visibility[j] && distance > distTab[j])
				{
					indeces[j] = i;

					distTab[j] = distance;
					visibility[j] = visBits;

					break;
				}
			}
		}
	}

	count--;

	if (count >= 0)
		return indeces[engine->RandomInt(0, count)];

	return engine->RandomInt(0, g_numWaypoints - 1);
}

void Bot::FacePosition(void) // SyPBM 1.50 - reworked aim
{
	// SyPB Pro P.30 - AMXX API
	if (m_lookAtAPI != nullvec)
		m_lookAt = m_lookAtAPI;

	if (sypbm_aiming_type.GetInt() == 1) // reworked aim
	{
		if (pev->button & IN_ATTACK && sypbm_aimbot.GetInt() == 1)
		{
			pev->v_angle = (m_lookAt - EyePosition()).ToAngles() + pev->punchangle;

			return;
		}

		if (m_trackingtime < engine->GetTime() || IsZombieMode())
		{
			m_goalaimposition = m_lookAt;

			// SyPBM 1.53 - now tracking time based on skill
			if ((5 / m_skill) > 1)
				m_trackingtime = engine->GetTime() + 1.0f; // max 1 seconds.
			else
				m_trackingtime = engine->GetTime() + float(5 / m_skill);
		};

		// save the cpu
		//if(pev->v_angle = (m_lookAt - EyePosition()).ToAngles() + pev->punchangle)
		//	return;

		// adjust all body and view angles to face an absolute vector
		Vector direction = (m_lookAt - EyePosition()).ToAngles() + pev->punchangle;
		direction.x *= -1.0f; // invert for engine

		Vector deviation = (direction - pev->v_angle);

		Vector eye_to_target;
		engine->SubtractVectors(m_goalaimposition, EyePosition(), eye_to_target);

		eye_to_target.Normalize();

		float cos_error = engine->GetVectorDotProduct(m_goalaimposition, direction);

		float max_angvel = sypbm_max_angular_vel.GetFloat();

		// adjust angular velocity limit based on aim error amount
		if (cos_error > 0.7f)
			max_angvel *= float(sin((1.0f + ((-49.0f / 15.0f) * (cos_error - 0.7f))) * (3.14f / 2.0f)));

		if (engine->GetTime() - m_itaimstart < 0.25f)
			max_angvel *= 4.0f * (engine->GetTime() - m_itaimstart);

		if ((direction - m_goalaimposition).GetLength() >= 1)
			m_itaimstart = engine->GetTime();

		m_aimSpeed.x = engine->ApproachAngle(direction.x, deviation.x, (max_angvel * g_pGlobals->frametime) * engine->RandomFloat(0.4f, 0.6f));
		m_aimSpeed.y = engine->ApproachAngle(direction.y, deviation.y, (max_angvel * g_pGlobals->frametime));
		m_aimSpeed.Normalize();

		// SyPBM 1.53 - aim fix
		if (IsZombieMode() && !m_isZombieBot && !FNullEnt(m_enemy) && IsInViewCone(GetEntityOrigin(m_enemy)))
		{
			if ((m_skill / 7) < 1.3f)
			{
				m_aimSpeed.x = AngleNormalize(m_aimSpeed.x) * 1.3f;
				m_aimSpeed.y = AngleNormalize(m_aimSpeed.y) * 1.3f;
			}
			else
			{
				m_aimSpeed.x = AngleNormalize(m_aimSpeed.x) * (m_skill / 7);
				m_aimSpeed.y = AngleNormalize(m_aimSpeed.y) * (m_skill / 7);
			}
		}
		else
		{
			if ((m_skill / 30) < 1.1f)
			{
				m_aimSpeed.x = AngleNormalize(m_aimSpeed.x) * 1.1f;
				m_aimSpeed.y = AngleNormalize(m_aimSpeed.y) * 1.1f;
			}
			else
			{
				m_aimSpeed.x = AngleNormalize(m_aimSpeed.x) * (m_skill / 35);
				m_aimSpeed.y = AngleNormalize(m_aimSpeed.y) * (m_skill / 35);
			}
		}

		m_aimSpeed.z = 0.0;

		// set the body angles to point the gun correctly
		pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
		pev->angles.y = pev->v_angle.y;

		m_idealAngles = m_aimSpeed;

		if (m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE | AIM_LASTENEMY) || GetCurrentTask()->taskID == TASK_DESTROYBREAKABLE)
		{
			m_playerTargetTime = engine->GetTime();
			m_randomizedIdealAngles = m_idealAngles;
		}

		if (sypbm_aimbot.GetInt() == 1)
			pev->v_angle = m_aimSpeed;
		else
			pev->v_angle = pev->v_angle + m_frameInterval * Vector(m_aimSpeed.x, m_aimSpeed.y, 0);

		pev->v_angle.ClampAngles();
		pev->v_angle.Normalize();
	}
	else // YaPB's aiming type
	{
	    if (pev->button & IN_ATTACK && sypbm_aimbot.GetInt() == 1)
	    {
	    	pev->v_angle = (m_lookAt - EyePosition()).ToAngles() + pev->punchangle;
 
	    	return;
	    }

	    float delta = engine->Clamp(engine->GetTime() - m_trackingtime, 0.001f, 1.0f / 30.0f);
		m_trackingtime = engine->GetTime();

		// adjust all body and view angles to face an absolute vector
		Vector direction = (m_lookAt - EyePosition()).ToAngles() + pev->punchangle;
		direction.x *= -1.0f; // invert for engine

		direction.ClampAngles();

		float accelerate = float(m_skill * 33);
		float stiffness = float(m_skill * 3);
		float damping = float(m_skill / 2.5);

		m_idealAngles = pev->v_angle;

		float angleDiffPitch = engine->AngleDiff(direction.x, m_idealAngles.x);
		float angleDiffYaw = engine->AngleDiff(direction.y, m_idealAngles.y);

		if (angleDiffYaw < 1.0f && angleDiffYaw > -1.0f)
		{
			m_lookYawVel = 0.0f;
			m_idealAngles.y = direction.y;
		}
		else
		{
			float accel = engine->Clamp(stiffness * angleDiffYaw - damping * m_lookYawVel, -accelerate, accelerate);

			m_lookYawVel += delta * accel;
			m_idealAngles.y += delta * m_lookYawVel;
		}

		float accel = engine->Clamp(2.0f * stiffness * angleDiffPitch - damping * m_lookPitchVel, -accelerate, accelerate);

		m_lookPitchVel += delta * accel;
		m_idealAngles.x += engine->Clamp(delta * m_lookPitchVel, -89.0f, 89.0f);

		if (!FNullEnt(m_enemy) && IsZombieEntity(m_enemy) && IsInViewCone(GetEntityOrigin(m_enemy)))
			m_idealAngles * float(m_skill / 30);

		pev->v_angle = m_idealAngles;
		pev->v_angle.ClampAngles();

		if (m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE | AIM_LASTENEMY) || GetCurrentTask()->taskID == TASK_DESTROYBREAKABLE)
		{
			m_playerTargetTime = engine->GetTime();
			m_randomizedIdealAngles = m_idealAngles;
		}

		// set the body angles to point the gun correctly
		pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
		pev->angles.y = pev->v_angle.y;
	}
}

void Bot::SetStrafeSpeed(Vector moveDir, float strafeSpeed)
{
	MakeVectors(pev->angles);

	Vector los = (moveDir - pev->origin).Normalize2D();
	float dot = los | g_pGlobals->v_forward.SkipZ();

	if (dot > 0 && !CheckWallOnRight())
		m_strafeSpeed = strafeSpeed;
	else if (!CheckWallOnLeft())
		m_strafeSpeed = -strafeSpeed;
}

// SyPB Pro P.28 - CT CS Ai
int Bot::FindHostage(void)
{
	if (m_isZombieBot)
		return -1;

	if (m_team != TEAM_COUNTER || !(g_mapType & MAP_CS))
		return -1;

	edict_t* ent = null;

	while (!FNullEnt(ent = FIND_ENTITY_BY_CLASSNAME(ent, "hostage_entity")))
	{
		bool canF = true;
		for (int i = 0; i < engine->GetMaxClients(); i++)
		{
			Bot* bot;
			if ((bot = g_botManager->GetBot(i)) != null && IsAlive(bot->GetEntity()))
			{
				for (int j = 0; j < Const_MaxHostages; j++)
				{
					if (bot->m_hostages[j] == ent)
						canF = false;
				}
			}
		}

		int nearestIndex = g_waypoint->FindNearest(GetEntityOrigin(ent));
		if ((nearestIndex >= 0) && (nearestIndex < g_numWaypoints) && canF)
			return nearestIndex;
	}

	return -1;
}

int Bot::FindLoosedBomb(void)
{
	// this function tries to find droped c4 on the defuse scenario map  and returns nearest to it waypoint
	if (m_isZombieBot)
		return -1;

	if (m_team != TEAM_TERRORIST || !(g_mapType & MAP_DE))
		return -1; // don't search for bomb if the player is CT, or it's not defusing bomb

	edict_t* bombEntity = null; // temporaly pointer to bomb

	// search the bomb on the map
	while (!FNullEnt(bombEntity = FIND_ENTITY_BY_CLASSNAME(bombEntity, "weaponbox")))
	{
		if (strcmp(STRING(bombEntity->v.model) + 9, "backpack.mdl") == 0)
		{
			int nearestIndex = g_waypoint->FindNearest(GetEntityOrigin(bombEntity));

			if ((nearestIndex >= 0) && (nearestIndex < g_numWaypoints))
				return nearestIndex;

			break;
		}
	}

	return -1;
}

bool Bot::IsWaypointUsed(int index)
{
	if (index < 0 || index >= g_numWaypoints)
		return true;

	// SyPB Pro P.40 - AntiBlock Check 
	if (IsAntiBlock(GetEntity()))
		return false;

	// SyPB Pro P.48 - Small improve
	for (int i = 0; i < engine->GetMaxClients(); i++)
	{
		edict_t* player = INDEXENT(i + 1);

		if (!IsAlive(player))
			continue;

		if (IsAntiBlock(player))
			continue;

		if (player == GetEntity())
			continue;

		if ((g_waypoint->GetPath(index)->origin - GetEntityOrigin(player)).GetLength() < 50.0f)
			return true;

		Bot* bot = g_botManager->GetBot(i);
		if (bot != null && bot->m_currentWaypointIndex == index)
			return true;
	}

	return false;
}

bool Bot::IsWaypointOccupied(int index)
{
	if (index < 0 || index >= g_numWaypoints)
		return true;

	// first check if current waypoint of one of the bots is index waypoint
	for (int i = 0; i < engine->GetMaxClients(); i++)
	{
		edict_t* player = INDEXENT(i + 1);

		if (player == NULL || player == GetEntity() || !IsAlive(player))
			continue;

		if (m_notKilled && m_currentWaypointIndex != -1 && m_prevWptIndex[0] != -1)
		{
			int occupyId = GetShootingConeDeviation(GetEntity(), &pev->origin) >= 0.7f ? m_prevWptIndex[0] : m_currentWaypointIndex;

			// length check
			float length = (g_waypoint->GetPath(occupyId)->origin - g_waypoint->GetPath(index)->origin).GetLength();

			if (occupyId == index || GetCurrentTask()->data == index || length < (64.0f * 64.0f))
				return true;
		}

		if (IsWaypointUsed(index))
			return true;
	}

	return false;
}

edict_t* Bot::FindNearestButton(const char* className)
{
	// this function tries to find nearest to current bot button, and returns pointer to
	// it's entity, also here must be specified the target, that button must open.

	if (IsNullString(className))
		return null;

	float nearestDistance = FLT_MAX;
	edict_t* searchEntity = null, * foundEntity = null;

	// find the nearest button which can open our target
	while (!FNullEnt(searchEntity = FIND_ENTITY_BY_TARGET(searchEntity, className)))
	{
		Vector entityOrign = GetEntityOrigin(searchEntity);

		float distance = (pev->origin - entityOrign).GetLength();

		if (distance <= nearestDistance)
		{
			nearestDistance = distance;
			foundEntity = searchEntity;
		}
	}

	return foundEntity;
}

// SyPB Pro P.38 - AMXX API
int Bot::CheckBotPointAPI(int mod)
{
	if (mod == 0)
		return GetEntityWaypoint(GetEntity());
	else if (mod == 1)
		return m_currentWaypointIndex;
	else if (mod == 2)
	{
		if (&m_navNode[0] != null)
			return m_navNode->index;
	}

	return -1;
}

// SyPB Pro P.40 - AMXX API
int Bot::GetNavData(int data)
{
	PathNode* navid = &m_navNode[0];
	int pointNum = 0;

	while (navid != null)
	{
		pointNum++;
		if (pointNum == data)
			return navid->index;

		navid = navid->next;
	}

	if (data == -1)
		return pointNum;

	return -1;
}