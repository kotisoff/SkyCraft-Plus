// SkyCraft+ : the shared world clock
package dev.skycraft.client;

import dev.skycraft.SkyCraft;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import net.fabricmc.loader.api.FabricLoader;

/**
 * Whether a sleep moves the world's clock. On: a Minecraft bed (or a bed or a wait in Skyrim) moves
 * the clock everyone shares, and every player's Skyrim is brought to it, so nobody wakes into another
 * hour than their friends. Off: a sleep only moves the sleeper's own Skyrim, as it always did.
 * Saved as {@code sleepSync=} in config/skycraft.properties.
 */
public final class SleepSyncToggle {
	private static final String KEY = "sleepSync";
	public static volatile boolean on = true;

	private SleepSyncToggle() {
	}

	/** Shown by Mod Menu's screen. */
	static final String TIP = "Sleep together: a sleep or a wait moves the world's clock, and every player's Skyrim follows it. "
		+ "Off, every Skyrim keeps its own time.";

	public static void register() {
		load();
	}

	private static Path file() {
		return FabricLoader.getInstance().getConfigDir().resolve("skycraft.properties");
	}

	private static void load() {
		Properties props = new Properties();
		try (var in = Files.newBufferedReader(file())) {
			props.load(in);
		} catch (NoSuchFileException e) {
			return;
		} catch (IOException e) {
			SkyCraft.LOG.warn("SkyCraft: couldn't read {}", file(), e);
			return;
		}
		on = Boolean.parseBoolean(props.getProperty(KEY, "true").trim());
	}

	/** Rewrites only its own line, keeping the file's comments and other settings. */
	static void save() {
		Path file = file();
		String line = KEY + "=" + on;
		try {
			List<String> lines = Files.exists(file) ? new ArrayList<>(Files.readAllLines(file)) : new ArrayList<>(List.of("# SkyCraft"));
			boolean found = false;
			for (int i = 0; i < lines.size(); i++) {
				if (lines.get(i).trim().startsWith(KEY + "=")) {
					lines.set(i, line);
					found = true;
				}
			}
			if (!found) {
				lines.add("# Whether a sleep moves the world's clock everyone shares, or only the sleeper's own Skyrim.");
				lines.add(line);
			}
			Files.createDirectories(file.getParent());
			Files.write(file, lines);
		} catch (IOException e) {
			SkyCraft.LOG.warn("SkyCraft: couldn't save {}", file, e);
		}
	}
}
