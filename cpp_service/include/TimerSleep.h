#pragma once
#include <iostream>
#include <Windows.h>

// Apparently this is already defined when I use msvc cl.
// #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002;
// 
// Timer sleepHelper 这种方法精度巨高 但是CPU占用量巨大 15%左右
// 使用while不断获取时间 精度略次之 大概偏差5%左右,CPU占用量9% 左右
// 自带的sleep函数和thread的forsleep 一样误差大 synchapi.h sleep函数
// 最终使用 windows 消息机制配合WaitableTimer
static int usleep(HANDLE timer, LONGLONG d) {
	LARGE_INTEGER liDueTime;
	DWORD ret;
	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;

	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	liDueTime.QuadPart = d;
	liDueTime.QuadPart = liDueTime.QuadPart * 10 * 1000;	// us into 100 of ns units
	liDueTime.QuadPart = -liDueTime.QuadPart;	// negative for relative dure time

	if (!SetWaitableTimer(timer, &liDueTime, 0, NULL, NULL, 0)) {
		printf("SetWaitableTimer failed: errno=%d\n", GetLastError());
		return 1;
	}

	ret = WaitForSingleObject(timer, INFINITE);
	if (ret != WAIT_OBJECT_0) {
		printf("WaitForSingleObject failed: ret=%d errno=%d\n", ret, GetLastError());
		return 1;
	}

	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

#ifdef DEBUG	
	printf("delay is %lld ms - slept for %lld ms\n", d, ElapsedMicroseconds.QuadPart);
#endif // DEBUG
	return 0;
}

static int sleep_for_ms(LONGLONG time)
{
	HANDLE timer;

	timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (timer == NULL) {
		printf("CreateWaitableTimerEx failed: errno=%d\n", GetLastError());
		return 1;
	}
	usleep(timer, time);
	CloseHandle(timer);
	return 0;
}