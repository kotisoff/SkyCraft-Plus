#include "Game.h"
#include "Sleep.h"

namespace skycraft::Sleep
{
	namespace
	{
		// There is one clock: Skyrim's own runs the host's, Minecraft's follows it, and everyone else
		// follows Minecraft. A difference under this is the two running by their own tick rates, and is
		// left alone; a bigger one is a sleep or a pause somewhere, and Skyrim is brought to it.
		constexpr float kSyncHours = 0.5f;
		// A big difference is taken only once it has held still this long. Where Minecraft skips a night
		// of its own, the morning shows for a moment and the next sync puts the night back.
		constexpr float         kBigJumpHours = 2.0f;
		constexpr std::uint64_t kBigJumpMs = 1500;
		// This player's own sleep is what moved the clock: the two already agree, and Skyrim's own
		// waking-up is still settling.
		constexpr std::uint64_t kAfterSleepMs = 4000;

		std::atomic<bool> pendingWake{ false };  // this player woke: its hour goes out on the next frame
		std::uint64_t     wokeAt = 0;
		float             pendingHour = 0.0f;    // a shared hour seen in the distance, waiting to hold still
		std::uint64_t     pendingSince = 0;
		bool              bedMenuOpened = false; // Skyrim's own sleep menu, opened for a Minecraft bed
		std::uint64_t     bedMenuAt = 0;         // when it was asked for: the queue shows it a moment on
		bool              bedMenuSeen = false;   // ... and it has actually been on screen
		// AddMessage only queues the menu: a frame or two before it is up, counting a close already
		// would take the player out of the bed the moment they got in.
		constexpr std::uint64_t kMenuAppearMs = 2000;
		// Skyrim's own clock may be set straight back by the engine, which keeps the hour itself: one
		// check a second after a change says which it is, once.
		float         verifyHour = -1.0f;
		std::uint64_t verifyAt = 0;
		bool          refused = false;

		/** Skyrim's sleep menu (the one its own beds and its wait key open). */
		bool SleepMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME);
		}

		/** The Minecraft body out of the bed with no sleep: nothing happened, or Skyrim wouldn't sleep. */
		void OutOfBed()
		{
			Link::Get().PushInput(proto::kInSlept, 1, 0);
		}

		void Woke()
		{
			wokeAt = GetTickCount64();
			pendingWake = true;
		}

		/** The hour this player's Skyrim woke at, to Minecraft's world clock (proto::kInSlept). */
		void SendHour()
		{
			auto* calendar = RE::Calendar::GetSingleton();
			if (!calendar) {
				return;
			}
			const float hour = calendar->GetHour();
			Link::Get().PushInput(proto::kInSlept, 0, static_cast<std::int32_t>(std::lround(hour * 100.0f)));
			logger::info("Skyrim woke at {:.2f}: Minecraft's world clock follows", hour);
		}

		/** This Skyrim's clock to the shared one, when they are far enough apart. */
		void Sync(float a_worldHour)
		{
			if (!(a_worldHour >= 0.0f)) {
				return;  // unknown: not in a world, or Minecraft didn't say
			}
			if (GetTickCount64() - wokeAt < kAfterSleepMs) {
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || player->AsActorState()->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
				return;  // its own bed, bench or crafting station: Skyrim's clock is in charge of its own
			}
			if (State().skyrimMenuOpen) {
				return;  // a menu owns it (the sleep menu itself, the console, a loading screen)
			}
			auto* calendar = RE::Calendar::GetSingleton();
			if (!calendar || !calendar->gameHour || !calendar->gameDaysPassed) {
				return;
			}
			const float hour = calendar->GetHour();
			// The nearest way round: a morning the clock is behind is a few hours, not most of a day.
			float delta = a_worldHour - hour;
			if (delta > 12.0f) {
				delta -= 24.0f;
			} else if (delta < -12.0f) {
				delta += 24.0f;
			}
			if (std::abs(delta) < kSyncHours) {
				pendingHour = 0.0f;
				return;
			}
			if (std::abs(delta) > kBigJumpHours) {
				const auto now = GetTickCount64();
				if (std::abs(a_worldHour - pendingHour) > 0.05f) {
					pendingHour = a_worldHour;
					pendingSince = now;
					return;
				}
				if (now - pendingSince < kBigJumpMs) {
					return;
				}
			}
			pendingHour = 0.0f;
			calendar->gameHour->value = a_worldHour;
			calendar->gameDaysPassed->value += delta / 24.0f;  // across midnight the day goes with it
			verifyHour = a_worldHour;
			verifyAt = GetTickCount64();
			logger::info("the world's clock is {:.2f} and Skyrim's was {:.2f}: set to the world's", a_worldHour, hour);
		}

		struct SleepStopSink final : RE::BSTEventSink<RE::TESSleepStopEvent>
		{
			static SleepStopSink* Get()
			{
				static SleepStopSink sink;
				return &sink;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent*, RE::BSTEventSource<RE::TESSleepStopEvent>*) override
			{
				Woke();
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		struct WaitStopSink final : RE::BSTEventSink<RE::TESWaitStopEvent>
		{
			static WaitStopSink* Get()
			{
				static WaitStopSink sink;
				return &sink;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent*, RE::BSTEventSource<RE::TESWaitStopEvent>*) override
			{
				Woke();
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESSleepStopEvent>(SleepStopSink::Get());
			events->AddEventSink<RE::TESWaitStopEvent>(WaitStopSink::Get());
		}
	}

	void OpenMenu()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || player->IsDead() || !player->CanSleepWait(nullptr)) {
			// Skyrim won't sleep now (CanSleepWait has shown its own reason: enemies about, and the
			// like). The Minecraft bed is still where the body lies, and leaving it is Skyrim's job.
			OutOfBed();
			return;
		}
		if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(RE::SleepWaitMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			bedMenuOpened = true;
			bedMenuSeen = false;
			bedMenuAt = GetTickCount64();
			logger::info("a Minecraft bed: Skyrim's sleep menu");
		} else {
			OutOfBed();
		}
	}

	void PerFrame(float a_worldHour)
	{
		// The sleep menu Skyrim opened for that bed is closed again. A sleep that did happen reports
		// itself a moment later (kInSlept with the hour it woke at); closed with no sleep at all - the
		// player cancelled it - the Minecraft body comes out of the bed now. Skyrim is the one that
		// sleeps, and on its own Minecraft leaves the body lying until everyone else has slept too.
		// (The menu is only queued by OpenMenu, hence bedMenuAt: until it has been up once, the only
		// thing a closed menu can mean is that it has not opened yet.)
		if (bedMenuOpened) {
			if (SleepMenuOpen()) {
				bedMenuSeen = true;
			} else if (bedMenuSeen || GetTickCount64() - bedMenuAt > kMenuAppearMs) {
				bedMenuOpened = false;
				if (!pendingWake) {
					OutOfBed();
					logger::info("Skyrim's sleep menu closed with no sleep: out of the Minecraft bed");
				}
			}
		}
		if (pendingWake.exchange(false) && Link::Get().McAlive()) {
			SendHour();
		}
		Sync(a_worldHour);
		if (verifyHour >= 0.0f && GetTickCount64() - verifyAt > 1000) {
			auto* calendar = RE::Calendar::GetSingleton();
			const float hour = calendar ? calendar->GetHour() : verifyHour;
			float       back = hour - verifyHour;
			if (back > 12.0f) {
				back -= 24.0f;
			} else if (back < -12.0f) {
				back += 24.0f;
			}
			if (!refused && std::abs(back) > kSyncHours) {
				refused = true;
				logger::warn("Skyrim's clock went back to {:.2f} after being set to {:.2f}: it keeps its own time", hour, verifyHour);
			}
			verifyHour = -1.0f;
		}
	}
}
