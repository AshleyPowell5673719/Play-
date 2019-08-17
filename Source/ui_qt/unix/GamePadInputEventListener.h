#pragma once

#include <atomic>
#include "Signal.hpp"
#include <thread>
#include <libevdev.h>
#include "Types.h"
#include "GamePadUtils.h"

class CGamePadInputEventListener
{
public:
	CGamePadInputEventListener(std::string);
	virtual ~CGamePadInputEventListener();

	Framework::CSignal<void(GamePadDeviceId, int, int, int, const input_absinfo*)> OnInputEvent;

private:
	std::string m_device;
	std::atomic<bool> m_running;
	std::thread m_thread;

	void InputDeviceListenerThread();
};
