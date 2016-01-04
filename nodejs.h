#ifndef EMSPROJ_NODEJS_H
#define EMSPROJ_NODEJS_H
#include "ems.h"

class MyObject : public Nan::ObjectWrap {
public:
    static void initialize(v8::Local<v8::Object> exports);
    //-----------------------------------
    // static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
    // static void GetValue(const Nan::FunctionCallbackInfo<v8::Value>& info);
    // static void PlusOne(const Nan::FunctionCallbackInfo<v8::Value>& info);
    // static void Multiply(const Nan::FunctionCallbackInfo<v8::Value>& info);

private:
    explicit MyObject(double value = 0);
    ~MyObject();
    //--------------------------------------------------------------

    static void EMScriticalEnter(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMScriticalExit(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSbarrier(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSsingleTask(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMScas(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSpush(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSpop(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSenqueue(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSdequeue(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSloopChunk(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static char EMStransitionFEtag(EMStag_t volatile *tag, unsigned char oldFE, unsigned char newFE, unsigned char oldType);
    static int64_t EMSwriteIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static int64_t EMSreadIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info);
    //--------------------------------------------------------------
    static void EMSread(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void EMSsync(const Nan::FunctionCallbackInfo<v8::Value>& info);
// int64_t EMShashString(const char *key);

    //----------------------------------------------------------------------------

    static void EMSwriteUsingTags(const Nan::FunctionCallbackInfo<v8::Value>& info,  // Index to read from
                                  unsigned char initialFE,             // Block until F/E tags are this value
                                  unsigned char finalFE);               // Set the tag to this value when done

    static void EMSreadUsingTags(const Nan::FunctionCallbackInfo<v8::Value>& info, // Index to read from
                                 unsigned char initialFE,            // Block until F/E tags are this value
                                 unsigned char finalFE);              // Set the tag to this value when done

    size_t emsMutexMem_alloc(struct emsMem *heap,   // Base of EMS malloc structs
                             size_t len,    // Number of bytes to allocate
                             volatile char *mutex);  // Pointer to the mem allocator's mutex
    static void emsMutexMem_free(struct emsMem *heap,  // Base of EMS malloc structs
                                 size_t addr,  // Offset of alloc'd block in EMS memory
                                 volatile char *mutex); // Pointer to the mem allocator's mutex

    static Nan::Persistent<v8::Function> constructor;
    double value_;
};


#endif //EMSPROJ_NODEJS_H
