pssh-box - Utility to generate and print PSSH boxes
===================================================

## Installation

To use this script you must first install the Python ProtoBuf library.  If you
have it installed already, you can just use the script directly.  These
instructions describe how to compile the ProtoBuf library so this script can
run.  This will not install ProtoBuf globally; it will only compile it.

1) You need Python 2.6 or newer.

2) Install `setuptools`.  This is installed by default when you
install `pip`.  If you don't have it, when you run `setup.py` it will install
it locally.  If you want to install manually, see:

```
 https://packaging.python.org/en/latest/installing.html#setup-for-installing-packages
```

3) Build the packager, which will build `protoc` in `out/{Debug,Release}`.

4) Run `setup.py`.  You will need to have `protoc` in PATH, which was build in
the previous step:

```bash
cd packager/third_party/protobuf/python
PATH=../../../../out/{Debug,Release}/:"$PATH" python setup.py build
```
