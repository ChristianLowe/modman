Modman.exe
==========

A simple Windows launcher for Grognak's Mod Manager ([link](https://github.com/Grognak/Grognaks-Mod-Manager)).

It searches for an installed python interpreter and tells it to run main.py.

If python.exe is found in a PATH dir, that will be used.

Otherwise the registry is checked.

* [HKCU and HKLM]\SOFTWARE\Python\PythonCore
* [HKCU and HKLM]\SOFTWARE\Wow6432Node\Python\PythonCore
* Priority is given to Python 2.7+, then 3.x, then 2.6.
