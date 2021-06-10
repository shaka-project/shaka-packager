#!/usr/bin/env node

// Modules we use:
var fs = require('fs');
var path = require('path');
var spawnSync = require('child_process').spawnSync;

// Command names per-platform:
var commandNames = {
  linux: 'packager-linux',
  darwin: 'packager-osx',
  win32: 'packager-win.exe',
};

// Get the current package version:
var package = require(path.resolve(__dirname, 'package.json'));
console.log('Preparing Shaka Packager v' + package.version);

// For fetching binaries from GitHub:
var urlBase = 'https://github.com/google/shaka-packager/releases/download/v' +
    package.version + '/';

// For spawning curl subprocesses:
var options = {
  detached: false,   // Do not let the child process continue without us
  stdio: 'inherit',  // Pass stdin/stdout/stderr straight through
};

// Create the bin folder if needed:
var binFolderPath = path.resolve(__dirname, 'bin');
if (!fs.existsSync(binFolderPath)) {
  fs.mkdirSync(binFolderPath, 0755);
}

// Wipe the bin folder's contents if needed:
fs.readdirSync(binFolderPath).forEach(function(childName) {
  var childPath = path.resolve(binFolderPath, childName);
  fs.unlinkSync(childPath);
});

for (var platform in commandNames) {
  // Find the destination for this binary:
  var command = commandNames[platform];
  var binaryPath = path.resolve(binFolderPath, command);

  download(urlBase + command, binaryPath);
  fs.chmodSync(binaryPath, 0755);
}

// Fetch LICENSE and README files from the same tag, and include them in the
// package.
var licenseUrl = 'https://raw.githubusercontent.com/google/shaka-packager/' +
    'v' + package.version + '/LICENSE';
download(licenseUrl, 'LICENSE');

var readmeUrl = 'https://raw.githubusercontent.com/google/shaka-packager/' +
    'v' + package.version + '/README.md';
download(readmeUrl, 'README.md');

console.log('Done!');


// Generic download helper
function download(url, outputPath) {
  // Curl args:
  var args = [
    '-L',  // follow redirects
    '-f',  // fail if the request fails
    // output destination:
    '-o',
    outputPath,
    '--show-error',  // show errors
    '--silent',      // but no progress bar
    url,
  ];

  // Now fetch the binary and fail the script if that fails:
  console.log('Downloading', url, 'to', outputPath);
  var returnValue = spawnSync('curl', args, options);
  if (returnValue.status != 0) {
    process.exit(returnValue.status);
  }
}
