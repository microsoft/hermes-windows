---
id: testing-windows-with-VSCode
title: Testing Windows UnitTests with VSCode
---

This document describes how run hermes-windows unittests with VSCode. Working from the command line can be hard to read and VSCode allows you to debug unit tests.

## Dependencies

- The latest version of Visual Studio Code (https://code.visualstudio.com/)

- VSCode Extension C/C++ Extension Pack from Microsoft

![image](https://user-images.githubusercontent.com/42554868/184951424-3fda89e3-e5fe-41c8-aee9-55d3e2b06a86.png)

- VSCode Extension C++ TestMate from Mate Pek

![image](https://user-images.githubusercontent.com/42554868/184951352-894ca171-2796-44fb-a70e-3fac0e221a17.png)

## Building Hermes via VSCode

| ![image](https://user-images.githubusercontent.com/42554868/184952178-eca03e8c-6e6b-4d49-b89d-c28d789f20bb.png) | After installing these extenstions, you should have two new Icons on the your navigation bar, one being CMAKE where you can build Hermes and the other being Testing for running the testsuite |
| --- | :--- |

After Navigating to the CMake section, you should be able to build hermes + the testsuite by clicking `Build All Project` located on the top of the navigation bar

![image](https://user-images.githubusercontent.com/42554868/184953720-c305a545-e3d7-4d53-aebd-1d73a2a2c1ef.png)

If you have already built hermes, you can just build the test suite by building `HermesUnitTests - Utility`

![image](https://user-images.githubusercontent.com/42554868/184954189-8d97c3e8-97a7-46fa-8010-462f6eb2aa8b.png)

## Running the TestSuite

Navigate to the Testing section. This extension combs through the repository to find all the current unittests, you are able to run and debug them here by either clicking Run Test or Debug Test

![image](https://user-images.githubusercontent.com/42554868/184954595-871e46e4-fc83-4655-97a4-b50dd7123889.png)

## Adding to the TestSuite

Unit Tests can be found under the folder `unittests`. You can either create a new folder for your tests or find the appropiate folder to put it in. If you create a new folder, you will need to add it to `unittests/MakeLists.txt`
