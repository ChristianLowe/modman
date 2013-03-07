#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <exception>
#include <windows.h>

using std::cout;    using std::endl;
using std::vector;  using std::cin;
using std::string;  using std::transform;

void pause()
{
    cout << endl << "Press Enter to continue...";
    cin.ignore();
}

int get_python_version(string pypath)
{
    transform(pypath.begin(), pypath.end(), pypath.begin(), ::tolower);
    unsigned found = pypath.find("python");

    if (found == string::npos)
    {
        return string::npos;
    }
    else
    {
        string version = pypath.substr(found+6, 2);
        return std::atoi(version.c_str());
    }
}

bool python_exists(vector<string> paths)
{
    // Cycle through all the paths, try to find one with Python.
    for (int i = 0; i < paths.size(); i++)
    {
        int version = get_python_version(paths[i]);

        if (version != -1)
        {
            if (version < 26)
            {
                cout << "Old version (" << version << ") of Python found." << endl;
                continue;
            }
            else
            {
                // Version is > then 2.6; we're good
                return true;
            }
        }
    }

    return false;
}

string get_modman_dir()
{
    TCHAR path[MAX_PATH];
    int bytes = GetModuleFileName(NULL, path, MAX_PATH);

    if (bytes == 0)
    {
        cout << "Error: Could not get the current directory." << endl
             << "Try running main.py directly.";
        pause();
        exit(3);
    }

    string spath = path;
    unsigned found = spath.rfind("\\");

    return spath.substr(0, found+1);
}

int main()
{
    // Read in PATH
    const char* path = getenv("PATH");

    if (path == NULL)
    {
        cout << "No %PATH% found... "
                "Sure you're running on Windows?" << endl;
        pause();
        return 1;
    }

    // Seperate all of the paths
    vector<string> paths;
    paths.push_back("");

    for (int i = 0; i < strlen(path); i++)
    {
        if (path[i] == ';')
        {
            paths.push_back("");
        }
        else
        {
            paths[paths.size()-1] += path[i];
        }
    }

    // Make sure Python is on the system
    if (!python_exists(paths))
    {
        cout << "No supported version of Python found on this system." << endl
             << "Download Python 2.7.3 at: www.python.org/getit/" << endl;
        pause();
        return 2;
    }

    string runlocation = "python \"" + get_modman_dir() + "src\\main.py\"";

    system(runlocation.c_str());

    return 0;
}
