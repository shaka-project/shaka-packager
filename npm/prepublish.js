#!/usr/bin/env node

// Modules we use:
var fs = require('fs');
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

// Get the current package version:
var package = require(path.resolve(__dirname, 'package.json'));
console.log('Preparing Shaka Packager v' + package.version);

// Calculate the repo name.  In GitHub Actions context, this will pull binaries
// correctly from a fork.  When run by hand, it will default to the official
// repo.
var repo = process.env.GITHUB_REPOSITORY || 'shaka-project/shaka-packager';

// For fetching binaries from GitHub:
var urlBase = 'https://github.com/' + repo + '/releases/download/v' +
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
  for (var arch in commandNames[platform]) {
    // Find the destination for this binary:
    var command = commandNames[platform][arch];
    var binaryPath = path.resolve(binFolderPath, command);

    try {
      download(urlBase + command, binaryPath);
      fs.chmodSync(binaryPath, 0755);
    } catch (error) {
      if (arch == 'arm64') {
        // Optional.  Forks may not have arm64 builds available.  Ignore.
      } else {
        // Required.  Re-throw and fail.
        throw error;
      }
    }
  }
}

// Fetch LICENSE and README files from the same tag, and include them in the
// package.
var licenseUrl = 'https://raw.githubusercontent.com/' + repo + '/' +
    'v' + package.version + '/LICENSE';
download(licenseUrl, 'LICENSE');

var readmeUrl = 'https://raw.githubusercontent.com/' + repo + '/' +
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
    throw new Error('Download of ' + url + ' failed: ' + returnValue.status);
  }
}
