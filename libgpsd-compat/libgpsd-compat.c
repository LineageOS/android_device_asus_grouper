// Written by Dmitry Grinberg

#define LOG_TAG "libgpsd-compat"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <utils/Log.h>
#include <hardware/power.h>
#include <hardware/hardware.h>

/*
 * Problems:
 * 1. Nexus 10's GPS library was made to work with android L
 * 2. Android M changed a few things around that make it not work
 *   a. Sensor manager API changed in a few places
 *   b. BoringSSL replaced OpenSSL
 * 3. Due to these now-missing unresolved symbols GPS library will not load or run
 *
 * A tricky but clever solution: INTERPOSITION library
 * 1. We'll edit the GPS library and replace one of its "NEED" record with one referencing a new library
 *    which we'll create.
 * 2. Make sure that libgpsd-compat's NEED records include the library whose record we replaced in the GPS
 *    library, to make sure that the linker brings it in afterall and all symbols in it are found
 * 3. Implement libgpsd-compat such that it provides the missing things and does them in such a way that the
 *    GPS library is happy.
 *
 * Result: GPS library works on M, with the help of libgpsd-compat and a small binary patch to the GPS
 *         library itself (replacing one of the "NEED" records with a NEED record for "libgpsd-compat")
 */


//various funcs we'll need to call, in their mangled form

    //android::String8::String8(char const*)
    extern void _ZN7android7String8C1EPKc(void **str8P, const char *str);

    //android::String8::~String8()
    extern void _ZN7android7String8D1Ev(void **str8P);

    //android::String16::String16(char const*)
    extern void _ZN7android8String16C1EPKc(void **str16P, const char *str);

    //android::String16::~String16()
    extern void _ZN7android8String16D1Ev(void **str16P);

    //android::SensorManager::~SensorManager()
    extern void _ZN7android13SensorManagerD1Ev(void *sensorMgr);

    //android::SensorManager::SensorManager(android::String16 const&)
    extern void _ZN7android13SensorManagerC1ERKNS_8String16E(void *sensorMgr, void **str16P);

    //android::SensorManager::createEventQueue(android::String8, int)
    extern void _ZN7android13SensorManager16createEventQueueENS_7String8Ei(void **retVal, void *sensorMgr, void **str8P, int mode);


//data exports we must provide for gps library to be happy

    /*
     * DATA:     android::Singleton<android::SensorManager>::sLock
     * USE:      INTERPOSE: a mutes that GPS lib will insist on accessing
     * NOTES:    In L, the sensor manager exposed this lock that callers
     *           actually locked & unlocked when accessing it. In M this
     *           is no longer the case, but we still must provide it for
     *           the GPS library to be happy. It will lock nothnhing, but
     *           as long as it is a real lock and pthread_mutex_* funcs
     *           work on it, the GPS library will be happy.
     */
    pthread_mutex_t _ZN7android9SingletonINS_13SensorManagerEE5sLockE = PTHREAD_MUTEX_INITIALIZER;

    /*
     * DATA:     android::Singleton<android::SensorManager>::sInstance
     * USE:      INTERPOSE: a singleton instance of SensorManager
     * NOTES:    In L, the sensor manager exposed this variable, as it was
     *           a singleton and one could just access this directly to get
     *           the current already-existing instance if it happened to
     *           already exist. If not one would create one and store it
     *           there. In M this is entirely different, but the GPS library
     *           does not know that. So we'll init it to NULL to signify that
     *           no current instance exists, let it create one, and store it
     *           here, and upon unloading we'll clean it up, if it is not
     *           NULL (which is what it would be if the GPS library itself
     *           did the cleanup).
     */
    void* _ZN7android9SingletonINS_13SensorManagerEE9sInstanceE = NULL;


//code exports we provide

    //android::SensorManager::SensorManager(void)
    void _ZN7android13SensorManagerC1Ev(void *sensorMgr);

    //android::SensorManager::createEventQueue(void)
    void _ZN7android13SensorManager16createEventQueueEv(void **retVal, void *sensorMgr);

    //this used to exist in OpenSLL, but does not in BoringSSL - for some reason GPS library uses it anyways
    void *CRYPTO_malloc(uint32_t sz, const char *file, uint32_t line);


//library on-load and on-unload handlers (to help us set things up and tear them down)
    void libEvtUnloading(void) __attribute__((destructor));


/*
 * FUNCTION: android::SensorManager::SensorManager(void)
 * USE:      INTERPOSE: construct a sensor manager object
 * NOTES:    This constructor no longer exists in M, instead now one must pass
 *           in a package name as a "string16" to the consrtuctor. Since this
 *           lib only services GPS library, it is easy for us to just do that
 *           and this provide the constructor that the GPS library wants.
 *           The package name we use if "gps.manta". Why not?
 */
void _ZN7android13SensorManagerC1Ev(void *sensorMgr)
{
    void *string;

    _ZN7android8String16C1EPKc(&string, "gps.tegra3");
    _ZN7android13SensorManagerC1ERKNS_8String16E(sensorMgr, &string);
    _ZN7android8String16D1Ev(&string);
}

/*
 * FUNCTION: android::SensorManager::createEventQueue(void)
 * USE:      INTERPOSE: create an event queue to receive events
 * NOTES:    This function no longer exists in M, instead now one must pass
 *           in a client name as a "string8" and an integer "mode"to it. M
 *           sources list default values for these params as an empty string
 *           and 0. So we'll craft the same call here.
 */
void _ZN7android13SensorManager16createEventQueueEv(void **retVal, void *sensorMgr)
{
    void *string;

    _ZN7android7String8C1EPKc(&string, "");
    _ZN7android13SensorManager16createEventQueueENS_7String8Ei(retVal, sensorMgr, &string, 0);
    _ZN7android7String8D1Ev(&string);
}

/*
 * FUNCTION: CRYPTO_malloc(uint32_t sz, const char *file, uint32_t line)
 * USE:      INTERPOSE: Allocate memory
 * NOTES:    In OpenSSL, this just allocates memory and optionally tracks it.
 *           Why manta's GPS library chose to use it is a mystery, but to make
 *           it happy we must provide it, so we do, backing the allocation with
 *           a calloc()-ed memory chunk.
 */
void *CRYPTO_malloc(uint32_t sz, const char *file, uint32_t line)
{
    (void)file;
    (void)line;
    return calloc(sz, 1);
}

/*
 * FUNCTION: libEvtUnloading()
 * USE:      Handle library unloading
 * NOTES:    This is a good time to free whatever is unfreed and say goodbye
 */
void libEvtUnloading(void)
{
    if (_ZN7android9SingletonINS_13SensorManagerEE9sInstanceE) {
        //if an instance stil exists, free it by calling the destructor, just to be throrough
        _ZN7android13SensorManagerD1Ev(_ZN7android9SingletonINS_13SensorManagerEE9sInstanceE);
        _ZN7android9SingletonINS_13SensorManagerEE9sInstanceE = NULL;
    }
}
