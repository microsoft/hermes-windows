---
id: hermes-windows-versioning
title: Versioning of hermes-windows
---

## Overview

In this document we outline the [microsoft/hermes-windows](https://github.com/microsoft/hermes-windows)
repo branches and the release versioning. 

## Branches

- `main` - contains the latest code. Currently we manually sync it with 
Meta hermes code.

- `rnw/*` - the folder with release branches

	- `rnw/0.70-stable` - targets React Native for Windows (RNW) 0.70 release.
    - `rnw/0.69-stable` - targets React Native for Windows (RNW) 0.69 release.
    - `rnw/0.68-stable` - targets React Native for Windows (RNW) 0.68 release.

- `meta/*` - the folder with `main` and release `rn/*` branches from [facebook/hermes](https://github.com/facebook/hermes)
repo. We have a nightly process that pulls code to these branch. They have
no other changes. The goal for these branches is to see the integration
history into the `main` branch. 
	
## Release versions

We have the following versioning schema for the `hermes-windows` releases:

- The **pre-release package versions** built in the `main` branch look like
**0.0.0-2209.9002-8af7870c** where the `<major>.<minor>.<patch>` versions are
always `'0.0.0'`, and the prerelease part that follows after `'-'` is
`'yyMM.drrr-hhhhhhhh'`. Where `'yy'` are two numbers for the year, `'MM'` are
two numbers for the month, `'d'` is the day without `0` prefix, `'rrr'` is a
three digit number for the today's revision, and `'hhhhhhhh'` are the first 8
hexadecimal numbers from the source GitHub commit hash.
- The **pre-release file versions** look like `0.0.2209.9002` where the the
encoding is `'0.0.yyMM.drrr'`. Where numbers after `'0.0.'` have the same
encoding as for the pre-release package version.
- The **released package versions** use the usual semantic schema which is
based on RNW release numbers like `0.70.1`, where the `'0.70'` is the
`<major>.<minor>` release of RNW and RN, and the last number is a `'patch'`
number for that release. Note that the `'patch'` number will not match the
`'patch'` number of the RNW. We are going to generate the `'patch'` number
using ADO build revision with format `'r'` that avoids `0` prefixes for the
version to be valid semantic version.
- The `release file versions` look like `0.70.1.0` to match the version of the package. The last part of the file version number is always `0`.
