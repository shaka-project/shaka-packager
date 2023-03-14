# Build Instructions

Shaka Packager supports building on Windows, Mac and Linux host systems.

## Linux build dependencies

Most development is done on Ubuntu (currently 14.04, Trusty Tahr). The
dependencies mentioned here are only for Ubuntu. There are some instructions
for [other distros below](#notes-for-other-linux-distros).

```shell
sudo apt-get update
sudo apt-get install -y \
        curl \
        libc-ares-dev \
        build-essential git python python3
```

Note that `Git` must be v1.7.5 or above.

## Mac system requirements

*   [Xcode](https://developer.apple.com/xcode) 7.3+.
*   The OS X 10.10 SDK or later. Run

    ```shell
    ls `xcode-select -p`/Platforms/MacOSX.platform/Developer/SDKs
    ```

    to check whether you have it.

*   Note that there is a known problem with 10.15 SDK or later right now. You
    can workaround it by using 10.14 SDK. See
    [#660](https://github.com/shaka-project/shaka-packager/issues/660#issuecomment-552576341)
    for details.

## Windows system requirements

* Visual Studio 2015 Update 3, 2017, or 2019. (See below.)
* Windows 7 or newer.

Install Visual Studio 2015 Update 3 or later - Community Edition should work if
its license is appropriate for you. Use the Custom Install option and select:

- Visual C++, which will select three sub-categories including MFC
- Universal Windows Apps Development Tools > Tools (1.4.1) and Windows 10 SDK
  (10.0.14393)

If using VS 2017 or VS 2019, you must set the following environment variables,
with versions and paths adjusted to match your actual system:

```shell
GYP_MSVS_VERSION="2019"
GYP_MSVS_OVERRIDE_PATH="C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
```

## Install `depot_tools`

Clone a particular branch of the `depot_tools` repository from Chromium:

```shell
git clone -b chrome/4147 https://chromium.googlesource.com/chromium/tools/depot_tools.git
touch depot_tools/.disable_auto_update
```

The latest version of depot_tools will not work, so please use that branch!


### Linux and Mac

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
export PATH="$PATH:/path/to/depot_tools"
```

### Windows

Add depot_tools to the start of your PATH (must be ahead of any installs of
Python). Assuming you cloned the repo to C:\src\depot_tools, open:

Control Panel → System and Security → System → Advanced system settings

If you have Administrator access, Modify the PATH system variable and
put `C:\src\depot_tools` at the front (or at least in front of any directory
that might already have a copy of Python or Git).

If you don't have Administrator access, you can add a user-level PATH
environment variable and put `C:\src\depot_tools` at the front, but
if your system PATH has a Python in it, you will be out of luck.

From a cmd.exe shell, run the command gclient (without arguments). On first
run, gclient will install all the Windows-specific bits needed to work with
the code, including msysgit and python.

* If you run gclient from a non-cmd shell (e.g., cygwin, PowerShell),
  it may appear to run properly, but msysgit, python, and other tools
  may not get installed correctly.
* If you see strange errors with the file system on the first run of gclient,
  you may want to
  [disable Windows Indexing](http://tortoisesvn.tigris.org/faq.html#cantmove2).

## Get the code

Create a `shaka_packager` directory for the checkout and change to it (you can
call this whatever you like and put it wherever you like, as long as the full
path has no spaces):

```shell
mkdir shaka_packager && cd shaka_packager
```

Run the `gclient` tool from `depot_tools` to check out the code and its
dependencies.

```shell
gclient config https://github.com/shaka-project/shaka-packager.git --name=src --unmanaged
gclient sync -r main
```

To sync to a particular commit or version, add the '-r \<revision\>' flag to
`gclient sync`, e.g.

```shell
gclient sync -r 4cb5326355e1559d60b46167740e04624d0d2f51
```

```shell
gclient sync -r v1.2.0
```

If you don't want the full repo history, you can save some time by adding the
`--no-history` flag to `gclient sync`.

When the above commands completes, it will have created a hidden `.gclient` file
and a directory called `src` in the working directory. The remaining
instructions assume you have switched to the `src` directory:

```shell
cd src
```

### Build Shaka Packager

#### Linux and Mac

Shaka Packager uses [Ninja](https://ninja-build.org) as its main build tool,
which is bundled in depot_tools.

To build the code, run `ninja` command:

```shell
ninja -C out/Release
```

If you want to build debug code, replace `Release` above with `Debug`.

We also provide a mechanism to change build settings, for example,
you can change build system to `make` by overriding `GYP_GENERATORS`:

```shell
GYP_GENERATORS='make' gclient runhooks
```

#### Windows

The instructions are similar, except that Windows allows using either `/` or `\`
as path separator:

```shell
ninja -C out/Release
ninja -C out\Release
```

Also, unlike Linux / Mac, 32-bit is chosen by default even if the system is
64-bit. 64-bit has to be enabled explicitly and the output directory is
configured to `out/%CONFIGURATION%_x64`, i.e.:

```shell
SET GYP_DEFINES='target_arch=x64'
gclient runhooks
ninja -C out/Release_x64
```

### Build artifacts

After a successful build, you can find build artifacts including the main
`packager` binary in build output directory (`out/Release` or `out/Release_x64`
for release build).

See [Shaka Packager Documentation](https://shaka-project.github.io/shaka-packager/html/)
on how to use `Shaka Packager`.

### Update your checkout

To update an existing checkout, you can run

```shell
git pull origin main --rebase
gclient sync
```

The first command updates the primary Packager source repository and rebases on
top of tip-of-tree (aka the Git branch `origin/main`). You can also use other
common Git commands to update the repo.

The second command syncs dependencies to the appropriate versions and re-runs
hooks as needed.

## Cross compiling for ARM on Ubuntu host

The install-build-deps script can be used to install all the compiler
and library dependencies directly from Ubuntu:

```shell
./packager/build/install-build-deps.sh
```

Install sysroot image and others using `gclient`:

```shell
GYP_CROSSCOMPILE=1 GYP_DEFINES="target_arch=arm" gclient runhooks
```

The build command is the same as in Ubuntu:

```shell
ninja -C out/Release
```

## Notes for other linux distros

### Alpine Linux

Use `apk` command to install dependencies:

```shell
apk add --no-cache \
        bash curl \
        bsd-compat-headers c-ares-dev linux-headers \
        build-base git ninja python2 python3
```

Alpine uses musl which does not have mallinfo defined in malloc.h. It is
required by one of Shaka Packager's dependency. To workaround the problem, a
dummy structure has to be defined in /usr/include/malloc.h, e.g.

```shell
sed -i \
  '/malloc_usable_size/a \\nstruct mallinfo {\n  int arena;\n  int hblkhd;\n  int uordblks;\n};' \
  /usr/include/malloc.h
```

We also need to enable musl in the build config:

```shell
export GYP_DEFINES='musl=1'
```

### Arch Linux

Instead of running `sudo apt-get install` to install build dependencies, run:

```shell
sudo pacman -Sy --needed \
        core/which \
        c-ares \
        gcc git python2 python3
```

### Debian

Same as Ubuntu.

### Fedora

Instead of running `sudo apt-get install` to install build dependencies, run:

```shell
su -c 'yum install -y \
        which \
        c-ares-devel libatomic \
        gcc-c++ git python2'
```

### CentOS

Same as Fedora.

### OpenSUSE

Use `zypper` command to install dependencies:

```shell
sudo zypper in -y \
        curl which \
        c-ares-devel \
        gcc-c++ git python python3
```

## Tips, tricks, and troubleshooting

### Xcode license agreement

If you are getting the error

> Agreeing to the Xcode/iOS license requires admin privileges, please re-run as
> root via sudo.

the Xcode license has not been accepted yet which (contrary to the message) any
user can do by running:

```shell
xcodebuild -license
```

Only accepting for all users of the machine requires root:

```shell
sudo xcodebuild -license
```

### Missing curl CA bundle

If you are getting the error

> gyp: Call to 'config/mac/find_curl_ca_bundle.sh' returned exit status 1 ...

curl CA bundle is not able to be located. Installing curl with openssl should
resolve the issue:

```shell
brew install curl --with-openssl
```

### Using an IDE

No specific instructions are available.

You might find Gyp generators helpful. Output is not guaranteed to work.
Manual editing might be necessary.

To generate CMakeLists.txt in out/Release and out/Debug use:

```shell
GYP_GENERATORS=cmake gclient runhooks
```

To generate IDE project files in out/Release and out/Debug use:

```shell
GYP_GENERATORS=eclipse gclient runhooks
GYP_GENERATORS=xcode gclient runhooks
GYP_GENERATORS=xcode_test gclient runhooks
GYP_GENERATORS=msvs gclient runhooks
GYP_GENERATORS=msvs_test gclient runhooks
```

## Contributing

If you have improvements or fixes, we would love to have your contributions.
See https://github.com/shaka-project/shaka-packager/blob/main/CONTRIBUTING.md for
details.

We have continue integration tests setup on pull requests. You can also verify
locally by running the tests manually.

If you know which tests are affected by your change, you can limit which tests
are run using the `--gtest_filter` arg, e.g.:

```shell
out/Debug/mp4_unittest --gtest_filter="MP4MediaParserTest.*"
```

You can find out more about GoogleTest at its
[GitHub page](https://github.com/google/googletest).
