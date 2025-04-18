
#ifndef BOX2DCONFIG_H
#define BOX2DCONFIG_H


#ifdef _MSC_VER
    #define BOX2D_API __declspec(dllexport)
#else
    #define BOX2D_API __attribute__((visibility("default")))
#endif


#endif
