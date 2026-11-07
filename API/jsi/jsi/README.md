# JavaScript Interface 

This folder contains definitions for JSI.
JSI is the public API for Hermes engine.
It is being used by React Native project to work with JS engines.

## JSI versions

JSI has versions associated with the following commit hashes in the 
https://github.com/facebook/hermes repo.
New version 22 rows use `static_h` hashes; existing rows retain their `main`
hashes. Version 21's `606a4249` (`main`) equals `b10cafda8e1` (`static_h`):
their `API/jsi` trees are identical.

This fork builds `hermes.dll` and its internal C++ clients at JSI 22. Public
headers still default to JSI 21 when a consumer supplies neither `JSI_VERSION`
nor a `jsi/jsi-version.h` override. RNW uses JSI 21 over the C API boundary;
no JSI C++ objects cross that boundary. Direct C++ consumers must build all JSI
objects and clients at matching epochs; the fallback is not DLL C++ ABI
compatibility.

Version 22 groups the following changes into one epoch.
Serialization remains opt-in under `JSI_UNSTABLE`, not part of version 22.
Event-loop control is a separate UUID-discovered extension, not a numbered
JSI version. Later decorator forwarding fixes do not introduce new epochs.

| Version | Commit Hash                                | Commit Description
|--------:|:-------------------------------------------|------------------------------------------------------
|      22 | `4ec5f281981f70727c5bae0f5155887f9665b8a4` | Split IRuntime from Runtime; use IRuntime in helpers
|         | `ee7bedb565f1a7fa79d5d93d346a31d7062b2fac` | Add Array::push
|         | `bd725d32b28d1d6dd44350b533012f20d69c4d10` | Add ArrayBuffer::tryGetMutableBuffer
|         | `2521f099b8aafb69596de799fce737b870fbd6ce` | Add ArrayBuffer::detached
|         | `f03f407164d0fb584b3c93da29ad8d86eb45e898` | Add TypedArray and Uint8Array APIs
|         | `417e8ddadb92d1eb40260e5724be63d1e5767242` | Add runtime error creation APIs and JSError factories
|         | `590396bd4601d4c836cd3c35f24ad1ce6b81a13e` | Add String::length
|         | `100841794bda7dab547f0308149f6c4ab61a8611` | Add Value::isInteger
|      21 | `606a4249fea27b072118c791431e799287cacf7c` | Move dumpOpcodeStats() to jsi::Instrumentation, remove IHermesExtra
|         | `195afcf8232db8df27f5b2a09c420a6962068c7c` | Add deleteProperty API 
|         | `7f716de439a5ae83eedc137c6e4f2a1d43a576eb` | Add Value override for has/get/setProperty
|      20 | `3eeb01121b52dbe6f9fd67c623a87e08ae9d5197` | Add UUID struct to JSI
|         | `8d1641f1918ed97b2f69e02ec9f420aa70701331` | Add JSI castInterface API  
|         | `34b807b919c5dacafdf23cd8f855d8f78acf810f` | Adds setRuntimeData and getRuntimeData JSI APIs
|      19 | `00a84e7bae9b8569e7b3f4118f4238544c67fe1b` | Add createFromUtf16 JSI method
|      18 | `d0328097291aade7269b2879910e11c48c8fbeb1` | Add default implementation for Object.create(prototype)
|      17 | `1012f891165e7dda3b51939b75bbc52e16e48a75` | Add default implementation for Object.getPrototypeOf and Object.setPrototypeOf
|      16 | `215bbe6ddc75393332b74138205e7cf5ab874e4e` | Add default getStringData/getPropNameIdData implementation
|      15 | `54e428c749358fb4aa25c6d7e2dcc3fc23b47124` | Let Pointer be nothrow-move-constructible
|         | `c582a23d62505a00948f5aa49006f60f959f8df4` | Let PointerValue::invalidate() be noexcept
|         | `b07ef4fc10dd53ad542f811040c820064c5ceb57` | Let Value be nothrow-move-constructible
|      14 | `c98b00e88bb685e75b769be67919a23a7f03b2e0` | Add utf16 method to JSI
|      13 | `077f2633dea36e1d0d18d6c4f114bf66e3ab74cf` | Add HeapSnapshotOptions for jsi::Instrumentation
|      12 | `de9dfe408bc3f8715aef161c5fcec291fc6dacb2` | Add queueMicrotask method to JSI
|         | `a699e87f995bc2f7193990ff36a57ec9cad940e5` | Make queueMicrotask pure virtual
|      11 | `a1c168705f609c8f1ae800c60d88eb199154264b` | Add JSI method for setting external memory size
|      10 | `b81666598672cb5f8b365fe6548d3273f216322e` | Clarify const-ness of JSI references
|       9 | `e6d887ae96bef5c71032f11ed1a9fb9fecec7b46` | Add external ArrayBuffers to JSI
|       8 | `4d64e61a1f9926eca0afd4eb38d17cea30bdc34c` | Add BigInt JSI API support
|         | `e8cc57311877464478da5421265bcc898098e136` | Add methods for creating and interacting with BigInt
|       7 | `4efad65b8266e3f26e04e3ca9addf92fc4d6ded8` | Add API for setting/getting native state
|       6 | `bc3cfb87fbfc82732936ec4445c9765bf9a5f08a` | Add BigInt skeleton
|       5 | `2b6d408980d7f23f50602fd88169c8a9881592a6` | Add PropNameID::fromSymbol
|       4 | `a5bee55c8301bb8662e408feee28bbc3e2a1fc81` | Introduce drainMicrotasks to JSI
|       3 | `0c9daa5a5eee7649558a53e3e541b80c89048c42` | Change jsi::Runtime::lockWeakObject to take a mutable ref
|       2 | `e0616e77e1ddc3ea5e2ccbca2e20dd0c4049c637` | Make it possible for a Runtime implementation to provide its own JSON parsing
|       1 | `3ba9615f80913764ecb6456779d502e31dde9e5d` | Fix build break in MSVC (#26462)
|       0 | `f22a18f67da3f03db59c1ec715d6ec3776b03fbf` | Initial commit

Relationship to React Native versions:

| Hermes RN Version | JSI version |
|------------------:|------------:|
|              main |          22 |
|             0.86+ |          22 |
|         0.82-0.85 |          21 |
|              0.81 |          20 |
|              0.80 |          19 |
|            0.79.0 |          19 |
|            0.78.0 |          18 |
|            0.77.0 |          15 |
|            0.76.0 |          13 |
|            0.74.3 |          12 |
|            0.74.0 |          11 |
|            0.73.0 |          10 |
|            0.72.0 |          10 |
|            0.71.0 |           9 |
|            0.70.0 |           6 |
|            0.69.0 |           5 |
