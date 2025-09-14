#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <thread>
#include <chrono>
#include <iostream>
#include "AudioRecorder.h"

#define UNICODE

static HWND hStatic;
static HWND hLevelStatic;
static AudioRecorder* recorder = nullptr;
static std::thread monitorThread;
static bool isMonitoring = false;
static COLORREF currentLevelColor = RGB(0, 255, 0); // Зеленый по умолчанию

// Прототип функции обработки сообщений
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void levelMonitorThread(HWND hWnd) {
    while (isMonitoring && recorder && recorder->isRunning()) {
        double level = recorder->getLatestLevel();

        // Обновляем текст в статическом элементе (кросс-поточный вызов)
        wchar_t levelText[64];
        swprintf(levelText, 64, L"Уровень звука: %.1f%%", level);

        // Используем SendMessage для безопасного обновления UI из другого потока
        SendMessage(hLevelStatic, WM_SETTEXT, 0, (LPARAM)levelText);

        // Меняем цвет в зависимости от уровня
        if (level > 50) {
            currentLevelColor = RGB(255, 0, 0); // Красный
        } else if (level > 20) {
            currentLevelColor = RGB(255, 165, 0); // Оранжевый
        } else {
            currentLevelColor = RGB(0, 255, 0); // Зеленый
        }

        // Принудительная перерисовка
        InvalidateRect(hLevelStatic, NULL, TRUE);
        UpdateWindow(hLevelStatic);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void startAudioMonitoring(HWND hWnd) {
    if (!recorder) {
        recorder = new AudioRecorder();
    }

    if (!isMonitoring) {
        isMonitoring = true;
        recorder->start();
        monitorThread = std::thread(levelMonitorThread, hWnd);
        SetWindowTextW(hStatic, L"Мониторинг запущен");
    }
}

void stopAudioMonitoring() {
    isMonitoring = false;

    if (recorder) {
        recorder->stop();
    }

    if (monitorThread.joinable()) {
        monitorThread.join();
    }

    SetWindowTextW(hStatic, L"Мониторинг остановлен");
    SetWindowTextW(hLevelStatic, L"Уровень звука: 0.0%");
    currentLevelColor = RGB(0, 255, 0); // Сброс цвета к зеленому
    InvalidateRect(hLevelStatic, NULL, TRUE);
    UpdateWindow(hLevelStatic);
}

// Точка входа Windows приложения
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Регистрация класса окна
    const wchar_t CLASS_NAME[] = L"MainWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassW(&wc))
    {
        MessageBoxW(NULL, L"Ошибка регистрации класса окна!", L"Ошибка", MB_ICONERROR);
        return 0;
    }

    // Создание главного окна
    HWND hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Аудио Монитор - WinAPI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL
    );

    if (hWnd == NULL)
    {
        MessageBoxW(NULL, L"Ошибка создания окна!", L"Ошибка", MB_ICONERROR);
        return 0;
    }

    // Создание кнопки запуска/остановки
    HWND hButton = CreateWindowW(
        L"BUTTON",
        L"Запустить мониторинг",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 50, 150, 30,
        hWnd,
        (HMENU)1,
        hInstance,
        NULL
    );

    // Создание кнопки остановки
    HWND hStopButton = CreateWindowW(
        L"BUTTON",
        L"Остановить мониторинг",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD,
        210, 50, 150, 30,
        hWnd,
        (HMENU)2,
        hInstance,
        NULL
    );

    // Статический текст для статуса
    hStatic = CreateWindowW(
        L"STATIC",
        L"Мониторинг не активен",
        WS_VISIBLE | WS_CHILD,
        50, 90, 200, 20,
        hWnd,
        (HMENU)3,
        hInstance,
        NULL
    );

    // Статический текст для уровня звука
    hLevelStatic = CreateWindowW(
        L"STATIC",
        L"Уровень звука: 0.0%",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        50, 120, 200, 30,
        hWnd,
        (HMENU)4,
        hInstance,
        NULL
    );

    // Установка шрифта для уровня звука
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    SendMessage(hLevelStatic, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Показать окно
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Цикл обработки сообщений
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Очистка перед выходом
    stopAudioMonitoring();
    if (recorder) {
        delete recorder;
        recorder = nullptr;
    }
    DeleteObject(hFont);

    return (int)msg.wParam;
}

// Функция обработки сообщений
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Обработка нажатия кнопки запуска
            if (wmId == 1)
            {
                startAudioMonitoring(hWnd);
            }
            // Обработка нажатия кнопки остановки
            else if (wmId == 2)
            {
                stopAudioMonitoring();
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;

            if (hwndStatic == hLevelStatic) {
                SetTextColor(hdcStatic, currentLevelColor);
                SetBkMode(hdcStatic, TRANSPARENT);

                // Возвращаем прозрачную кисть для фона
                static HBRUSH hBrush = NULL;
                if (hBrush == NULL) {
                    hBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
                }
                return (LRESULT)hBrush;
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}