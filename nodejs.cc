#include "ems.h"
#include "nodejs.h"

Nan::Persistent<v8::Function> MyObject::constructor;
MyObject::MyObject(double value) : value_(value) {
    ADD_FUNC_TO_V8_OBJ(target, "initialize", initialize);
    ADD_FUNC_TO_V8_OBJ(target, "barrier", EMSbarrier);
    ADD_FUNC_TO_V8_OBJ(target, "singleTask", EMSsingleTask);
    ADD_FUNC_TO_V8_OBJ(target, "criticalEnter", EMScriticalEnter);
    ADD_FUNC_TO_V8_OBJ(target, "criticalExit", EMScriticalExit);
    ADD_FUNC_TO_V8_OBJ(target, "loopInit", EMSloopInit);
    ADD_FUNC_TO_V8_OBJ(target, "loopChunk", EMSloopChunk);
    // ADD_FUNC_TO_V8_OBJ(target, "sync", EMSsync);
    // ADD_FUNC_TO_V8_OBJ(target, "length");
    // ADD_FUNC_TO_V8_OBJ(target, "Buffer");
}

MyObject::~MyObject() {
    // static void EMSarrFinalize(char *data, void *hint) {
}


void MyObject::initialize(const Nan::FunctionCallbackInfo<v8::Value>& info){
    initialize(info);
}


void MyObject::EMScriticalEnter(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMScriticalEnter(info);
}

void MyObject::EMScriticalExit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMScriticalExit(info);
}

void MyObject::EMSbarrier(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSbarrier(info);
}

void MyObject::EMSsingleTask(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSsingleTask(info);
}

void MyObject::EMScas(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMScas(info);
}

void MyObject::EMSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSfaa(info);
}

void MyObject::EMSpush(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSpush(info);
}

void MyObject::EMSpop(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSpop(info);
}

void MyObject::EMSenqueue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSenqueue(info);
}

void MyObject::EMSdequeue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSdequeue(info);
}

void MyObject::EMSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSloopInit(info);
}

void MyObject::EMSloopChunk(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSloopChunk(info);
}

unsigned char MyObject::EMStransitionFEtag(EMStag_t volatile *tag, unsigned char oldFE, unsigned char newFE, unsigned char oldType) {
    EMStransitionFEtag(tag, oldFE, newFE, oldType);
}

int64_t MyObject::EMSwriteIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteIndexMap(info);
}

int64_t MyObject::EMSreadIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadIndexMap(info);
}

//--------------------------------------------------------------
void MyObject::EMSread(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSread(info);
}

void MyObject::EMSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info)  {
    EMSreadRW(info);
}

void MyObject::EMSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreleaseRW(info);
}

void MyObject::EMSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadFE(info);
}

void MyObject::EMSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadFF(info);
}

void MyObject::EMSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwrite(info);
}

void MyObject::EMSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteEF(info);
}

void MyObject::EMSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteXF(info);
}

void MyObject::EMSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteXE(info);
}

void MyObject::EMSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSsetTag(info);
}

void MyObject::EMSsync(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSsync(info);
}
// int64_t EMShashString(const char *key);
