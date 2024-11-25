#ifndef DLLOAD_HPP
#define DLLOAD_HPP


#include <map>
#include <string>
#if defined(WIN32)
#include <windows.h>
typedef HMODULE  MODULE_HANDLE;
#endif
 
#if defined(__linux__)
typedef void*  MODULE_HANDLE;
#include <dlfcn.h>
#endif
class DllFuncLoader
{
private:
    std::map<std::string,MODULE_HANDLE> m_nameDllMap;
public:
    DllFuncLoader(/* args */)
    {

    }
    ~DllFuncLoader()
    {
        for (std::map<std::string,MODULE_HANDLE>::iterator i = m_nameDllMap.begin(); i != m_nameDllMap.end(); i++)
        {            
            #if defined(WIN32)
                FreeLibrary(i->second);
            #endif
            #if defined(__linux__)
                dlclose (i->second);
            #endif
            /* code */
        }

    }

    int openDll(std::string dllName)
    {
        MODULE_HANDLE newDll;
        #if defined(WIN32)
            newDll= LoadLibraryA (dllName.c_str());
        #endif
        
        #if defined(__linux__)
            newDll= dlopen(dllName.c_str(), RTLD_NOW|RTLD_GLOBAL);
        #endif
        if (newDll==NULL)
        {
            return 1;
        }
        else
        {
            m_nameDllMap.insert(std::make_pair(dllName,newDll));
            return 0;
        }
    }
    void* getFunc(std::string dllName,std::string funcName)
    {
        if(m_nameDllMap.count(dllName))
        {
            #if defined(WIN32)
                return (void *)GetProcAddress(m_nameDllMap[dllName], funcName.c_str());
            #endif
            
            #if defined(__linux__)
                return dlsym(m_nameDllMap[dllName],funcName.c_str());
            #endif
        }
        return 0;
    }
    
};

#endif