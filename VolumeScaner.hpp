//
// Created by Werty on 9/13/2025.
//

#ifndef COURSE_VOLUMESCANER_H
#define COURSE_VOLUMESCANER_H
// mic_level_4hz.h
#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <vector>

// Линкуем winmm.lib
#pragma comment(lib, "winmm.lib")

// Точка запуска захвата
int start();


#endif //COURSE_VOLUMESCANER_H