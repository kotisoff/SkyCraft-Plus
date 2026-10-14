#pragma once

// SkyCraft+ : sleeping, and the shared world clock (v36). Using a Minecraft bed opens Skyrim's own
// sleep menu, exactly as a bed of Skyrim's does, and Minecraft's own sleep runs beside it (spawn
// point, the night's rest). A sleep or a wait anywhere moves Minecraft's world clock, and every
// player's Skyrim is then brought to it, so nobody wakes into yesterday.
namespace skycraft::Sleep
{
	void Install();
	// A player used a Minecraft bed (proto::kEvSleep, from that player's client).
	void OpenMenu();
	// Main thread, every frame: a sleep of this player's that just ended goes out to Minecraft, and
	// this Skyrim is brought to the shared clock once the two have drifted apart.
	// a_worldHour: SkyState's counterpart, McState::worldHour (negative: unknown).
	void PerFrame(float a_worldHour);
}
