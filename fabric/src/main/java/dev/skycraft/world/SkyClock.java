// SkyCraft+ : the shared world clock
package dev.skycraft.world;

/**
 * SkyCraft+ : v36. Minecraft's clock, the one the host's Skyrim drives and every player's Skyrim is
 * brought to. A day is 24000 ticks with tick 0 at 6am (noon 6000, midnight 18000), so an hour of the
 * day is not a fraction of the day from where one might look for it.
 */
public final class SkyClock {
	private SkyClock() {
	}

	/** The hour of the day (0..24) at a Minecraft time, in ticks (0 = 6am). */
	public static float hour(long ticks) {
		return ((Math.floorMod(ticks, 24000L) / 1000.0F) + 6.0F) % 24.0F;
	}

	/** The Minecraft time, in ticks, of an hour of the day (0..24). */
	public static int ticks(float hour) {
		return Math.floorMod(Math.round((hour - 6.0F) * 1000.0F), 24000);
	}
}
