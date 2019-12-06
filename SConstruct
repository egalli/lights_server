
import subprocess

# HACK: rpi_ws281x is not setup to be built in a nested manner
subprocess.check_call('cd rpi_ws281x && scons libws2811.a', shell=True)

env=Environment(CPPPATH=['rpi_ws281x'],
    CFLAGS=['-Wall', '-ggdb'],
    CPPFLAGS=['-Wall', '-ggdb'],
    CPPDEFINES=[],
    LIBPATH=['rpi_ws281x'],
    LIBS=['libws2811.a'],
    SCONS_CXX_STANDARD="c++17"
    )

env.Program('lights_server', 'src/main.cpp')