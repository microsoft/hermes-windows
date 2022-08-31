/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes/VM/HostModel.h"

#include "hermes/VM/BuildMetadata.h"

namespace hermes {
namespace vm {

//===----------------------------------------------------------------------===//
// class FinalizableNativeFunction

const CallableVTable FinalizableNativeFunction::vt{
    {
        VTable(
            CellKind::FinalizableNativeFunctionKind,
            cellSize<FinalizableNativeFunction>(),
            FinalizableNativeFunction::_finalizeImpl),
        FinalizableNativeFunction::_getOwnIndexedRangeImpl,
        FinalizableNativeFunction::_haveOwnIndexedImpl,
        FinalizableNativeFunction::_getOwnIndexedPropertyFlagsImpl,
        FinalizableNativeFunction::_getOwnIndexedImpl,
        FinalizableNativeFunction::_setOwnIndexedImpl,
        FinalizableNativeFunction::_deleteOwnIndexedImpl,
        FinalizableNativeFunction::_checkAllOwnIndexedImpl,
    },
    FinalizableNativeFunction::_newObjectImpl,
    FinalizableNativeFunction::_callImpl};

void FinalizableNativeFunctionBuildMeta(
    const GCCell *cell,
    Metadata::Builder &mb) {
  mb.addJSObjectOverlapSlots(
      JSObject::numOverlapSlots<FinalizableNativeFunction>());
  NativeFunctionBuildMeta(cell, mb);
  mb.setVTable(&FinalizableNativeFunction::vt);
}

CallResult<HermesValue> FinalizableNativeFunction::createWithoutPrototype(
    Runtime &runtime,
    void *context,
    NativeFunctionPtr functionPtr,
    FinalizeNativeFunctionPtr finalizePtr,
    SymbolID name,
    unsigned paramCount) {
  auto parentHandle = Handle<JSObject>::vmcast(&runtime.functionPrototype);

  auto *cell = runtime.makeAFixed<FinalizableNativeFunction, HasFinalizer::Yes>(
      runtime,
      parentHandle,
      runtime.getHiddenClassForPrototype(
          *parentHandle, numOverlapSlots<FinalizableNativeFunction>()),
      context,
      functionPtr,
      finalizePtr);
  auto selfHandle = JSObjectInit::initToHandle(runtime, cell);

  auto prototypeObjectHandle = runtime.makeNullHandle<JSObject>();

  auto st = defineNameLengthAndPrototype(
      selfHandle,
      runtime,
      name,
      paramCount,
      prototypeObjectHandle,
      Callable::WritablePrototype::Yes,
      false);
  (void)st;
  assert(
      st != ExecutionStatus::EXCEPTION && "defineLengthAndPrototype() failed");

  return selfHandle.getHermesValue();
}

//===----------------------------------------------------------------------===//
// class FinalizableNativeConstructor

const CallableVTable FinalizableNativeConstructor::vt{
    {
        VTable(
            CellKind::FinalizableNativeConstructorKind,
            cellSize<FinalizableNativeConstructor>(),
            FinalizableNativeConstructor::_finalizeImpl),
        FinalizableNativeConstructor::_getOwnIndexedRangeImpl,
        FinalizableNativeConstructor::_haveOwnIndexedImpl,
        FinalizableNativeConstructor::_getOwnIndexedPropertyFlagsImpl,
        FinalizableNativeConstructor::_getOwnIndexedImpl,
        FinalizableNativeConstructor::_setOwnIndexedImpl,
        FinalizableNativeConstructor::_deleteOwnIndexedImpl,
        FinalizableNativeConstructor::_checkAllOwnIndexedImpl,
    },
    FinalizableNativeConstructor::_newObjectImpl,
    FinalizableNativeConstructor::_callImpl};

void FinalizableNativeConstructorBuildMeta(
    const GCCell *cell,
    Metadata::Builder &mb) {
  mb.addJSObjectOverlapSlots(
      JSObject::numOverlapSlots<FinalizableNativeConstructor>());
  NativeConstructorBuildMeta(cell, mb);
  mb.setVTable(&FinalizableNativeConstructor::vt);
}

CallResult<Handle<FinalizableNativeConstructor>>
FinalizableNativeConstructor::create(
    Runtime &runtime,
    void *context,
    NativeFunctionPtr functionPtr,
    FinalizeNativeFunctionPtr finalizePtr,
    Handle<JSObject> prototypeObjectHandle,
    SymbolID name,
    unsigned paramCount) {
  Handle<JSObject> parentHandle =
      Handle<JSObject>::vmcast(&runtime.functionPrototype);

  FinalizableNativeConstructor *cell =
      runtime.makeAFixed<FinalizableNativeConstructor, HasFinalizer::Yes>(
          runtime,
          parentHandle,
          runtime.getHiddenClassForPrototype(
              *parentHandle, numOverlapSlots<FinalizableNativeConstructor>()),
          context,
          functionPtr,
          finalizePtr,
          creatorFunction<JSObject>,
          CellKind::JSObjectKind);
  Handle<FinalizableNativeConstructor> selfHandle =
      JSObjectInit::initToHandle(runtime, cell);

  ExecutionStatus st = defineNameLengthAndPrototype(
      selfHandle,
      runtime,
      name,
      paramCount,
      prototypeObjectHandle,
      Callable::WritablePrototype::Yes,
      /*strictMode*/ false);
  if (st == ExecutionStatus::EXCEPTION) {
    assert(false && "defineLengthAndPrototype() failed");
    return ExecutionStatus::EXCEPTION;
  }

  return selfHandle;
}

//===----------------------------------------------------------------------===//
// class HostObject

HostObjectProxy::~HostObjectProxy() {}

const ObjectVTable HostObject::vt{
    VTable(
        CellKind::HostObjectKind,
        cellSize<HostObject>(),
        HostObject::_finalizeImpl),
    HostObject::_getOwnIndexedRangeImpl,
    HostObject::_haveOwnIndexedImpl,
    HostObject::_getOwnIndexedPropertyFlagsImpl,
    HostObject::_getOwnIndexedImpl,
    HostObject::_setOwnIndexedImpl,
    HostObject::_deleteOwnIndexedImpl,
    HostObject::_checkAllOwnIndexedImpl,
};

void HostObjectBuildMeta(const GCCell *cell, Metadata::Builder &mb) {
  mb.addJSObjectOverlapSlots(JSObject::numOverlapSlots<HostObject>());
  JSObjectBuildMeta(cell, mb);
  mb.setVTable(&HostObject::vt);
}

CallResult<HermesValue> HostObject::createWithoutPrototype(
    Runtime &runtime,
    std::unique_ptr<HostObjectProxy> proxy) {
  auto parentHandle = Handle<JSObject>::vmcast(&runtime.objectPrototype);

  HostObject *hostObj = runtime.makeAFixed<HostObject, HasFinalizer::Yes>(
      runtime,
      parentHandle,
      runtime.getHiddenClassForPrototype(
          *parentHandle, numOverlapSlots<HostObject>()),
      std::move(proxy));

  hostObj->flags_.hostObject = true;

  return JSObjectInit::initToHermesValue(runtime, hostObj);
}

} // namespace vm
} // namespace hermes
