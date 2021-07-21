#!/usr/bin/env node

// Modules we use:
var path = require('path');
var spawnSync = require('child_process').spawnSync;

// Command names per-platform:
var commandNames = {
  linux: 'packager-linux',
  darwin: 'packager-osx',
  win32: 'packager-win.exe',
};

// Find the platform-specific binary:
var binaryPath = path.resolve(__dirname, 'bin', commandNames[process.platform]);

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
