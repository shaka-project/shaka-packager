#!/usr/bin/env node

// Modules we use:
var path = require('path');
var spawnSync = require('child_process').spawnSync;

// Command names per-platform (process.platform) and per-architecture
// (process.arch):
var commandNames = {
  linux: {
    'x64': 'packager-linux-x64',
    'arm64': 'packager-linux-arm64',
  },
  darwin: {
    'x64': 'packager-osx-x64',
    'arm64': 'packager-osx-arm64',
  },
  win32: {
    'x64': 'packager-win-x64.exe',
  },
};

// Find the platform-specific binary:
if (!(process.platform in commandNames)) {
  throw new Error('Platform not supported: ' + process.platform);
}
if (!(process.arch in commandNames[process.platform])) {
  throw new Error(
      'Architecture not supported: ' + process.platform + '/' + process.arch);
}
var commandName = commandNames[process.platform][process.arch];
var binaryPath = path.resolve(__dirname, 'bin', commandName);

// Find the args to pass to that binary:
// argv[0] is node itself, and argv[1] is the script.
// The rest of the args start at 2.
var args = process.argv.slice(2);

var options = {
  detached: false,   // Do not let the child process continue without us
  stdio: 'inherit',  // Pass stdin/stdout/stderr straight through
};

// Execute synchronously:
var returnValue = spawnSync(binaryPath, args, options);

// Pipe the exit code back to the OS:
var exitCode = returnValue.error ? returnValue.error.code : 0;
process.exit(exitCode);
