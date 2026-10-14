package dev.skycraft.world;

import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.Test;

/**
 * The world's clock: Minecraft's day is 24000 ticks with tick 0 at 6am, and Skyrim's hour has to line
 * up with it both ways (the server sets Minecraft's clock from the sleeper's hour, and every client
 * reads an hour back out to hand its own Skyrim).
 */
class SkyClockTest {
	@Test
	void theDayStartsAtSixWithNoonAndMidnightWhereTheyBelong() {
		assertEquals(6.0F, SkyClock.hour(0));
		assertEquals(12.0F, SkyClock.hour(6000));
		assertEquals(18.0F, SkyClock.hour(12000));
		assertEquals(0.0F, SkyClock.hour(18000));
	}

	@Test
	void ticksWrapAndSoDoHours() {
		assertEquals(6.0F, SkyClock.hour(24000));  // the next morning
		assertEquals(5.0F, SkyClock.hour(-1000));  // 5am, taken as the day before
		assertEquals(5.0F, SkyClock.hour(23000));
		assertEquals(18000, SkyClock.ticks(24.0F));  // 24 is midnight, the same hour as 0
		assertEquals(17000, SkyClock.ticks(23.0F));
	}

	@Test
	void anHourSurvivesTheRoundTripAtEveryFiftyMinutes() {
		for (int minutes = 0; minutes < 24 * 60; minutes += 50) {
			float hour = minutes / 60.0F;
			assertEquals(hour, SkyClock.hour(SkyClock.ticks(hour)), 0.01F, "hour " + hour);
		}
	}

	@Test
	void everyWholeHourComesBackAsItsOwnTick() {
		for (int ticks = 0; ticks < 24000; ticks += 1000) {
			assertEquals(ticks, SkyClock.ticks(SkyClock.hour(ticks)), "tick " + ticks);
		}
	}
}
