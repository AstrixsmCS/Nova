#pragma once

#include "Event.hpp"

	class KeyEvent : public Event
	{
	public:
		inline uint32_t GetKeyCode() const { return m_KeyCode; }

		virtual uint32_t GetCategoryFlags() const override { return EventCategoryKeyboard | EventCategoryInput; }
	protected:
		KeyEvent(uint32_t keycode) : m_KeyCode(keycode) {}
		uint32_t m_KeyCode;
	};

	class KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(uint32_t keycode, int repeatCount) : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

		inline int GetRepeatCount() const { return m_RepeatCount; }

		virtual const char* GetName() const override { return "KeyPressedEvent"; }
		static const char* GetStaticName() { return "KeyPressedEvent"; }
	private:
		int m_RepeatCount;
	};

	class KeyReleasedEvent : public KeyEvent
	{
	public:
		KeyReleasedEvent(uint32_t keycode) : KeyEvent(keycode) {}

		virtual const char* GetName() const override { return "KeyReleasedEvent"; }
		static const char* GetStaticName() { return "KeyReleasedEvent"; }
	};

	class KeyTypedEvent : public KeyEvent
	{
	public:
		KeyTypedEvent(uint32_t keycode) : KeyEvent(keycode) {}

		virtual const char* GetName() const override { return "KeyTypedEvent"; }
		static const char* GetStaticName() { return "KeyTypedEvent"; }
	};
