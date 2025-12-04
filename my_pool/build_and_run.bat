@echo off
chcp 65001 >nul
echo ================================================
echo   编译并运行 ThreadPool 演示程序
echo ================================================
echo.

REM 尝试使用 g++ 编译
where g++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo 正在使用 g++ 编译...
    g++ -std=c++17 main.cpp thread_pool.cpp -O2 -o thread_pool_demo.exe
    if %ERRORLEVEL% EQU 0 (
        echo 编译成功！
        echo.
        echo ================================================
        echo   运行结果
        echo ================================================
        thread_pool_demo.exe
    ) else (
        echo 编译失败，请检查代码或编译器配置
    )
) else (
    REM 尝试使用 cl.exe (MSVC)
    where cl >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        echo 正在使用 MSVC 编译...
        cl /std:c++17 /EHsc /O2 main.cpp thread_pool.cpp /Fe:thread_pool_demo.exe
        if %ERRORLEVEL% EQU 0 (
            echo 编译成功！
            echo.
            thread_pool_demo.exe
        ) else (
            echo 编译失败
        )
    ) else (
        echo 错误：未找到 C++ 编译器 (g++ 或 cl)
        echo 请安装 MinGW-w64 或 Visual Studio
    )
)

echo.
pause

