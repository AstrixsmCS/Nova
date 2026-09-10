#pragma once

#include "Event.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using EventListenerID = uint64_t;

// Deferred, subscription-based event broadcasting.
//
// Publish() queues events and Process() dispatches them.
// Events published during Process() are dispatched by the next Process().
//
// This implementation is single-threaded and should only be accessed
// from the main thread.

class EventBus
{
public:
	template<typename TEvent>
	static EventListenerID Subscribe(std::function<void(TEvent&)> callback)
	{
		static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

		auto& bus = Get();

		const EventListenerID id = bus.m_NextID++;
		const std::type_index type = typeid(TEvent);

		bus.m_Listeners[type].push_back({
			.ID = id,
			.Callback =
				[callback = std::move(callback)](
					Event& event) mutable
				{
					callback(static_cast<TEvent&>(event));
				}
		});

		return id;
	}

	template<typename TEvent>
	static void Unsubscribe(EventListenerID id)
	{
		static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

		auto& bus = Get();
		const std::type_index type = typeid(TEvent);

		const auto iterator = bus.m_Listeners.find(type);

		if (iterator == bus.m_Listeners.end())
			return;

		auto& listeners = iterator->second;

		std::erase_if(
			listeners,
			[id](const Listener& listener)
			{
				return listener.ID == id;
			}
		);

		if (listeners.empty())
			bus.m_Listeners.erase(iterator);
	}

	template<typename TEvent, typename... TArguments>
	static void Publish(TArguments&&... arguments)
	{
		static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

		Get().m_PendingEvents.push_back({
			.Type = typeid(TEvent),
			.Value = std::make_unique<TEvent>(
				std::forward<TArguments>(arguments)...
			)
		});
	}

	static void Process()
	{
		auto& bus = Get();

		std::vector<PendingEvent> events;
		events.swap(bus.m_PendingEvents);

		for (auto& event : events)
		{
			if (!event.Value->m_Handled)
				bus.Dispatch(event.Type, *event.Value);
		}
	}

	static void Clear()
	{
		auto& bus = Get();

		bus.m_Listeners.clear();
		bus.m_PendingEvents.clear();
	}

private:
	struct Listener
	{
		EventListenerID ID = 0;
		std::function<void(Event&)> Callback;
	};

	struct PendingEvent
	{
		std::type_index Type;
		std::unique_ptr<Event> Value;
	};

	static EventBus& Get()
	{
		static EventBus instance;
		return instance;
	}

	void Dispatch(std::type_index type, Event& event)
	{
		const auto iterator = m_Listeners.find(type);

		if (iterator == m_Listeners.end())
			return;

		// Copying allows callbacks to subscribe or unsubscribe without
		// invalidating the collection currently being traversed.
		const auto listeners = iterator->second;

		for (const auto& listener : listeners)
		{
			if (event.m_Handled)
				break;

			listener.Callback(event);
		}
	}

private:
	std::unordered_map<std::type_index, std::vector<Listener>> m_Listeners;

	std::vector<PendingEvent> m_PendingEvents;

	EventListenerID m_NextID = 1;
};
